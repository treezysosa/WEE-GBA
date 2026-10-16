#include "vocal/VocalAnalysis.h"
#include "vocal/VocalEngine.h"
#include "vocal/ChopSequencer.h"
#include "dsp/LayerRack.h"
#include <iostream>
#include <memory>
#include <cstdlib>
#include <new>
static thread_local bool audio=false;static int allocations=0;
void* operator new(std::size_t n){if(audio)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
double energy(const std::vector<float>& data){double sum=0;for(float x:data){check(std::isfinite(x),"finite vocal output");sum+=x*x;}return sum;}
int main(){
    wee::SliceMap map;check(map.valid() && map.add(.25) && map.add(.5) && map.add(.75),"manual split insertion");check(map.count==4 && map.sliceAt(.6)==2,"slice selection");check(!map.add(.5) && !map.remove(0) && map.remove(2),"marker bounds/removal");
    wee::SampleData onsets;onsets.rate=48000;onsets.left.resize(48000,0);onsets.right.resize(48000,0);
    for(int start:{4800,14400,24000,33600})for(int i=0;i<2400;++i)onsets.left[size_t(start+i)]=onsets.right[size_t(start+i)]=float(std::sin(i*.1))*.7f;
    const auto detected=wee::detectSlices(onsets,.5f);check(detected.valid() && detected.count>=5 && detected.count<=9,"four transient boundaries detected");
    for(double onset:{.1,.3,.5,.7}){bool found=false;for(int i=1;i<detected.count;++i)if(std::abs(detected.points[size_t(i)]-onset)<.012)found=true;check(found,"onset localization");}
    wee::SampleData silence=onsets;std::fill(silence.left.begin(),silence.left.end(),0);silence.right=silence.left;check(wee::detectSlices(silence,1).count==1,"silence does not invent transients");
    auto bank=std::make_unique<wee::WavetableBank>();bank->prepare();wee::SampleData sample;sample.rate=48000;sample.left.resize(48000);sample.right.resize(48000);
    for(size_t i=0;i<sample.left.size();++i){sample.left[i]=float(std::sin(i*6.283185307179586*440/48000))*.5f;sample.right[i]=sample.left[i]*.25f;}
    wee::LayerConfig layer;wee::Parameters p;p.gain=1;p.attack=.001f;p.sustain=1;p.release=.05f;p.bypassTone=true;wee::VocalConfig config;config.v[wee::VChopMap]=0;
    for(double rate:{44100.0,48000.0,96000.0})for(int block:{32,128,512,1024})for(float stretch:{.5f,1.0f,2.0f}){
        wee::VocalEngine engine;engine.prepare(rate,*bank);engine.setSample(&sample);config.v[wee::VStretch]=stretch;
        std::vector<float> l(size_t(std::ceil(rate*stretch))+size_t(block),0),r(l.size(),0);
        audio=true;engine.noteOn(60,1,layer,config);for(int at=0;at<int(l.size());at+=block)engine.render(l.data()+at,r.data()+at,std::min(block,int(l.size())-at),p,config,120);audio=false;
        check(energy(l)>.1 && energy(l)>energy(r)*8,"stretched stereo audio");check(engine.activeVoices()==0,"duration ends independently of held MIDI note");
        size_t last=0;for(size_t i=0;i<l.size();++i)if(std::abs(l[i])>.00001f)last=i;
        check(std::abs(double(last)-rate*stretch)<rate*.015,"stretch changes duration predictably");
    }
    for(float pitch:{-12.0f,0.0f,12.0f}){
        wee::VocalEngine engine;engine.prepare(48000,*bank);engine.setSample(&sample);config.v[wee::VStretch]=1;config.v[wee::VPitch]=pitch;
        std::vector<float> l(49000,0),r(l.size(),0);engine.noteOn(60,1,layer,config);audio=true;for(int i=0;i<int(l.size());i+=128)engine.render(l.data()+i,r.data()+i,std::min(128,int(l.size())-i),p,config,120);audio=false;
        int crossings=0;for(size_t i=12001;i<36000;++i)if(l[i-1]<=0 && l[i]>0)++crossings;
        const int expected=int(220*std::pow(2.0,pitch/12));if(std::abs(crossings-expected)>3)std::cerr<<"pitch "<<pitch<<" crossings "<<crossings<<" expected "<<expected<<'\n';
        check(std::abs(crossings-expected)<=3,"pitch changes frequency independently of duration");check(engine.activeVoices()==0,"pitch does not extend sample duration");
    }
    {
        wee::VocalEngine engine;engine.prepare(48000,*bank);engine.setSample(&sample);config={};config.v[wee::VChopMap]=0;config.v[wee::VReverse]=1;
        std::vector<float> l(48000,0),r(l.size(),0);engine.noteOn(60,1,layer,config);engine.render(l.data(),r.data(),48000,p,config,120,0,true);
        double error=0;for(size_t i=2400;i<45000;++i)error+=std::abs(l[i]-sample.left[47999-i]);check(error/42600<.002,"reverse unity reconstruction");
        engine.setSample(nullptr);std::fill(l.begin(),l.end(),0);engine.noteOn(60,1,layer,config);engine.render(l.data(),nullptr,48000,p,config,120);check(energy(l)==0,"missing vocal sample silence");
    }
    {
        wee::VocalEngine engine;engine.prepare(48000,*bank);sample.slices.add(.5);engine.setSample(&sample);config={};
        engine.noteOn(59,1,layer,config);engine.noteOn(62,1,layer,config);check(engine.activeVoices()==0,"chop note range rejects missing slices");
        engine.noteOn(61,1,layer,config);std::vector<float> l(25000,0);engine.render(l.data(),nullptr,25000,p,config,120);check(engine.activeVoices()==0 && energy(l)>.1,"MIDI maps individual chop");
        layer.v[wee::Loop]=1;for(int i=0;i<40;++i)engine.noteOn(60,1,layer,config);check(engine.activeVoices()==16,"vocal pool voice stealing");engine.allNotesOff(true);engine.noteOn(60,1,layer,config,0);engine.noteOn(60,1,layer,config,1,0);engine.stopLane(1);check(engine.activeVoices()==1,"sequence and keyboard lanes isolated");
        engine.sustainPedal(true);engine.noteOff(60);engine.render(l.data(),nullptr,25000,p,config,120);check(engine.activeVoices()==1,"vocal sustain");engine.sustainPedal(false);engine.render(l.data(),nullptr,25000,p,config,120);check(engine.activeVoices()==0,"vocal release");layer.v[wee::Loop]=0;
    }
    // Spectral-envelope warp changes harmonic balance while retaining the carrier.
    std::vector<float> vowel(24000,0),shifted;
    for(size_t i=0;i<vowel.size();++i)for(int h=1;h<35;++h){double f=h*120.0,a=std::exp(-std::pow((f-700)/180,2))+.6*std::exp(-std::pow((f-1500)/240,2));vowel[i]+=float(std::sin(i*6.283185307179586*f/48000)*a*.05);}
    check(wee::shiftFormants(vowel,shifted,48000,5),"spectral formant render");double difference=0;for(size_t i=0;i<vowel.size();++i)difference+=std::abs(vowel[i]-shifted[i]);check(difference>10 && energy(shifted)>0,"formant warp changes audible spectrum");
    auto centroid=[](const std::vector<float>& data){double sum=0,weight=0;for(int h=1;h<35;++h){double real=0,imaginary=0;for(size_t i=6000;i<18000;++i){const double angle=i*6.283185307179586*h*120/48000;real+=data[i]*std::cos(angle);imaginary-=data[i]*std::sin(angle);}const double power=real*real+imaginary*imaginary;sum+=power*h*120;weight+=power;}return sum/weight;};
    check(centroid(shifted)>centroid(vowel)*1.05,"positive formant shift raises harmonic spectral centroid");
    check(!wee::shiftFormants(vowel,shifted,48000,5,[]{return true;}),"background processing cancellation");
    // Sequence timing is invariant to callback splits, with sample-exact gates.
    for(int block:{37,128,511,1024}){
        wee::ChopSequencer sequence;config={};config.v[wee::VSeqEnabled]=1;config.v[wee::VSeqLength]=4;for(int i=0;i<4;++i)config.v[wee::VStep1+size_t(i)]=float(i+1);
        int ons=0,offs=0;int64_t total=0;audio=true;
        while(total<48000){const int count=int(std::min(int64_t(block),48000-total));sequence.begin(config,count,48000,120,false,true,false,0,4);int cursor=0;
            sequence.until(count,[&](int a,int b){check(a==cursor && b>=a,"sequence render spans contiguous");cursor=b;},[&](int note,float){check(total+cursor==ons*6000,"sequence note on sample");check(note==60+ons%4,"sequence rearrangement");++ons;},[&](int){check(total+cursor==offs*6000+3000,"sequence gate sample");++offs;});check(cursor==count,"sequence renders complete block");total+=count;}
        audio=false;check(ons==8 && offs==8,"sequence event count");
        check(sequence.begin(config,128,48000,120,true,false,true,4,4),"transport stop clears sequence lane");
        check(!sequence.begin(config,128,48000,120,true,true,true,8,4),"start with no old voices");
        check(sequence.begin(config,128,48000,120,true,true,true,1,4),"transport seek clears sequence lane");
    }
    config={};config.v[wee::VSync]=1;config.v[wee::VSourceBpm]=120;check(config.stretch(60)==2 && config.stretch(240)==.5,"host tempo stretch ratio");
    check(allocations==0,"no vocal playback/sequence allocations");
    std::cout<<"Vocal tests passed: onsets/editing, pitch/stretch, stereo/reverse, mapping/lanes, formants, cancellation, tempo and sample-exact sequencing.\n";
}
