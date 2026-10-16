#pragma once
#include "dsp/Sampler.h"
#include <complex>
#include <vector>
#include <functional>
namespace wee {
inline SliceMap detectSlices(const SampleData& audio,float sensitivity) {
    SliceMap result;if(!audio.valid())return result;
    const size_t hop=size_t(std::max(32.0,audio.rate*.005));
    const int minGap=std::max(2,int(.05*audio.rate/double(hop)));
    const size_t frames=(audio.left.size()+hop-1)/hop;
    std::vector<double> rms(frames,0);double peak=0;
    for(size_t f=0;f<frames;++f){double energy=0;const auto end=std::min(audio.left.size(),(f+1)*hop);for(size_t i=f*hop;i<end;++i)energy+=(double(audio.left[i])*audio.left[i]+double(audio.right[i])*audio.right[i])*.5;rms[f]=std::sqrt(energy/double(std::max(size_t(1),end-f*hop)));peak=std::max(peak,rms[f]);}
    if(peak<.00001)return result;
    double floor=0,previous=0;int last=-minGap;
    const double threshold=std::clamp(1.0f-sensitivity,0.0f,1.0f)*2+.6;
    for(size_t f=0;f<frames && result.count<maxSlices;++f){
        const double level=rms[f],rise=level-previous;
        if(int(f)-last>=minGap && level>peak*.03 && rise>peak*(.025+.08*(1-sensitivity)) && level>std::max(peak*.02,floor*threshold)){
            const double normalized=double(f*hop)/double(audio.left.size());
            if(normalized>.000001 && normalized<.999999)result.add(normalized);
            last=int(f);
        }
        floor=.94*floor+.06*level;previous=level;
    }
    return result;
}
template<size_t N> void fft(std::array<std::complex<double>,N>& data,bool inverse){
    for(size_t i=1,j=0;i<N;++i){size_t bit=N>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(data[i],data[j]);}
    for(size_t length=2;length<=N;length<<=1){const double angle=(inverse?2:-2)*3.14159265358979323846/double(length);const std::complex<double> step(std::cos(angle),std::sin(angle));for(size_t i=0;i<N;i+=length){std::complex<double> w(1,0);for(size_t j=0;j<length/2;++j){auto a=data[i+j],b=data[i+j+length/2]*w;data[i+j]=a+b;data[i+j+length/2]=a-b;w*=step;}}}
    if(inverse)for(auto& x:data)x/=double(N);
}
// Background STFT spectral-envelope warping: bin phases/harmonics stay in place.
inline bool shiftFormants(const std::vector<float>& input,std::vector<float>& output,double rate,float semitones,const std::function<bool()>& cancelled={}){
    constexpr size_t N=1024,hop=256,bins=N/2+1;
    if(std::abs(semitones)<.0001f){output.clear();return true;}
    output.assign(input.size(),0);std::vector<float> weights(input.size(),0);
    std::array<double,N> window {};for(size_t i=0;i<N;++i)window[i]=.5-.5*std::cos(2*3.14159265358979323846*double(i)/double(N));
    std::array<std::complex<double>,N> spectrum {};std::array<double,bins> logMagnitude {},envelope {};
    const int radius=std::clamp(int(350*double(N)/rate),2,32);
    const double ratio=std::pow(2.0,std::clamp(double(semitones),-18.0,18.0)/12);
    for(int64_t start=-int64_t(N-hop);start<int64_t(input.size());start+=int64_t(hop)){
        if(cancelled && cancelled())return false;
        for(size_t i=0;i<N;++i){const auto at=start+int64_t(i);spectrum[i]=at>=0 && at<int64_t(input.size())?double(input[size_t(at)])*window[i]:0;}
        fft(spectrum,false);for(size_t k=0;k<bins;++k)logMagnitude[k]=std::log(std::max(1e-8,std::abs(spectrum[k])));
        for(size_t k=0;k<bins;++k){double sum=0,weight=0;for(int j=-radius;j<=radius;++j){const size_t at=size_t(std::clamp(int(k)+j,0,int(bins-1)));const double w=radius+1-std::abs(j);sum+=w*logMagnitude[at];weight+=w;}envelope[k]=sum/weight;}
        for(size_t k=1;k<bins-1;++k){const double source=std::clamp(double(k)/ratio,0.0,double(bins-1));const size_t a=std::min(bins-2,size_t(source));const double f=source-double(a),shifted=envelope[a]+f*(envelope[a+1]-envelope[a]);const double gain=std::clamp(std::exp(shifted-envelope[k]),.125,8.0);spectrum[k]*=gain;spectrum[N-k]=std::conj(spectrum[k]);}
        fft(spectrum,true);for(size_t i=0;i<N;++i){const auto at=start+int64_t(i);if(at>=0 && at<int64_t(input.size())){output[size_t(at)]+=float(spectrum[i].real()*window[i]);weights[size_t(at)]+=float(window[i]*window[i]);}}
    }
    for(size_t i=0;i<output.size();++i)output[i]=weights[i]>1e-8f?std::clamp(output[i]/weights[i],-8.0f,8.0f):0;
    return true;
}
}
