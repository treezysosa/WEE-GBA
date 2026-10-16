#pragma once
#include "VocalConfig.h"
#include "dsp/Wavetable.h"
namespace wee {
class VocalEngine {
    struct Head{double position=0;int age=0;bool active=false;};
    struct Voice{
        int note=0,lane=0,stage=0,countdown=0,direction=1;bool held=false,sustained=false,loop=false;
        float velocity=0,envelope=0,filterL=0,filterR=0;double begin=0,end=0,clock=0,basePitch=1;uint64_t age=0;std::array<Head,4> heads {};
    };
    std::array<Voice,16> voices {};const SampleData* sample=nullptr;const WavetableBank* bank=nullptr;
    double rate=44100,bend=1;bool pedal=false;uint64_t serial=0;int window=1764,hop=441;float gain=0;
    double align(const Voice& voice,double nominal,double step,const std::vector<float>& data)const noexcept {
        const Head* reference=nullptr;for(const auto& h:voice.heads)if(h.active && (!reference || h.age<reference->age))reference=&h;
        if(!reference || std::abs(nominal-reference->position)<.00001)return nominal;
        auto read=[&](double p){if(voice.loop){p=voice.begin+std::fmod(p-voice.begin,voice.end-voice.begin);if(p<voice.begin)p+=voice.end-voice.begin;}if(p<voice.begin || p>=voice.end)return 0.0f;const auto i=size_t(p),j=double(i+1)<voice.end?i+1:i;return data[i]+float(p-double(i))*(data[j]-data[i]);};
        std::array<float,32> target {};double targetEnergy=0;
        for(size_t i=0;i<target.size();++i){target[i]=read(reference->position+double(i)*step*2);targetEnergy+=double(target[i])*target[i];}
        if(targetEnergy<1e-8)return nominal;
        double best=nominal,bestScore=-2;
        auto score=[&](double position){if(position<voice.begin || position>=voice.end)return;double dot=0,energy=0;for(size_t i=0;i<target.size();++i){const double x=read(position+double(i)*step*2);dot+=x*target[i];energy+=x*x;}if(energy<1e-8)return;const double value=dot/std::sqrt(energy*targetEnergy)-.00001*std::abs(position-nominal)/std::max(1.0,sample->rate*.006);if(value>bestScore){bestScore=value;best=position;}};
        const double radius=sample->rate*.006,coarse=radius/16;
        for(int i=-16;i<=16;++i)score(nominal+i*coarse);
        const double center=best,fine=coarse/8;for(int i=-8;i<=8;++i)score(center+i*fine);
        const double refined=best;for(int i=-8;i<=8;++i)score(refined+i*fine/8);
        return best;
    }

public:
    void prepare(double sr,const WavetableBank& tables) noexcept{rate=std::max(8000.0,sr);bank=&tables;window=std::max(8,int(rate*.04)/4*4);hop=window/4;voices={};bend=1;pedal=false;serial=0;gain=0;}
    void setSample(const SampleData* asset) noexcept{allNotesOff(true);sample=asset;}
    void pitchWheel(int value) noexcept{bend=std::pow(2.0,(value-8192)/8192.0*2/12);}
    void sustainPedal(bool down) noexcept{pedal=down;if(!down)for(auto& v:voices)if(v.sustained && !v.held){v.stage=4;v.sustained=false;}}
    void stopLane(int lane) noexcept{for(auto& v:voices)if(v.lane==lane)v.stage=0;}
    void noteOn(int note,float velocity,const LayerConfig& c,const VocalConfig& settings,int lane=0,int forcedSlice=-1) noexcept{
        if(velocity<=0){noteOff(note,lane);return;}if(!sample || !sample->valid() || !sample->slices.valid())return;
        const auto mapping=c.mapping();if(note<mapping.low || note>mapping.high)return;
        int slice=forcedSlice;if(slice<0 && settings.v[VChopMap]>=.5f)slice=note-int(settings.v[VBase]);
        const bool mapped=slice>=0;
        if(settings.v[VChopMap]>=.5f && forcedSlice<0 && note<int(settings.v[VBase]))return;
        if(mapped && slice>=sample->slices.count)return;
        const double length=double(sample->left.size()),first=std::max(double(mapping.start),mapped?sample->slices.points[size_t(slice)]:0.0),last=std::min(double(mapping.end),mapped?sample->slices.points[size_t(slice+1)]:1.0);
        const double begin=std::floor(std::clamp(first,0.0,1.0)*(length-1)),end=std::floor(std::clamp(last,0.0,1.0)*length);if(end-begin<2)return;
        Voice* target=nullptr;for(auto& v:voices)if(!v.stage){target=&v;break;}if(!target)target=&*std::min_element(voices.begin(),voices.end(),[](auto& a,auto& b){return a.age<b.age;});
        *target={};target->note=note;target->lane=lane;target->stage=1;target->held=true;target->age=++serial;target->velocity=velocity;
        target->begin=begin;target->end=end;target->direction=settings.v[VReverse]>=.5f?-1:1;target->clock=target->direction>0?begin:end-1;
        target->basePitch=std::pow(2.0,(mapping.transpose+(mapped?0:note-mapping.root))/12.0);target->loop=lane==2?false:mapping.loop;
    }
    void noteOff(int note,int lane=0) noexcept{for(auto& v:voices)if(v.stage && v.note==note && v.lane==lane && v.held){v.held=false;v.sustained=pedal && lane==0;if(!v.sustained)v.stage=4;}}
    void allNotesOff(bool immediate=false) noexcept{pedal=false;for(auto& v:voices){v.held=v.sustained=false;v.stage=immediate?0:4;}}
    int activeVoices()const noexcept{int n=0;for(const auto& v:voices)n+=v.stage!=0;return n;}
    void render(float* left,float* right,int count,const Parameters& p,const VocalConfig& c,double bpm,float modulationPitch=0,bool dry=false) noexcept{
        if(!sample || !sample->valid() || !bank)return;
        const auto& dataL=sample->vocalLeft.empty()?sample->left:sample->vocalLeft;const auto& dataR=sample->vocalRight.empty()?sample->right:sample->vocalRight;
        const double sourceStep=sample->rate/rate,scan=sourceStep/c.stretch(bpm),pitch=bend*std::pow(2.0,(c.v[VPitch]+modulationPitch)/12.0);
        const float a=float(1/(rate*std::max(.001f,p.attack))),d=float((1-p.sustain)/(rate*std::max(.001f,p.decay))),r=float(1/(rate*std::max(.001f,p.release)));
        const float smooth=1-std::exp(float(-1/(rate*.005))),fc=1-std::exp(float(-6.283185307179586*std::clamp(double(p.cutoff),20.0,rate*.45)/rate));
        for(int i=0;i<count;++i){float sumL=0,sumR=0;
            for(auto& v:voices){
                if(!v.stage)continue;
                if(v.clock<v.begin || v.clock>=v.end){if(v.loop){double pos=std::fmod(v.clock-v.begin,v.end-v.begin);if(pos<0)pos+=v.end-v.begin;v.clock=v.begin+pos;}else{v.stage=0;continue;}}
                if(v.stage==1){v.envelope+=a;if(v.envelope>=1){v.envelope=1;v.stage=2;}}else if(v.stage==2){v.envelope-=d;if(v.envelope<=p.sustain){v.envelope=p.sustain;v.stage=3;}}else if(v.stage==3)v.envelope=p.sustain;else{v.envelope-=r;if(v.envelope<=0){v.stage=0;continue;}}
                if(v.countdown<=0){for(auto& h:v.heads)if(!h.active){h={align(v,v.clock,v.direction*sourceStep*v.basePitch*pitch,dataL),0,true};break;}v.countdown+=hop;}--v.countdown;
                float l=0,rValue=0,weight=0;
                for(auto& h:v.heads)if(h.active){
                    double position=h.position;if(v.loop){position=v.begin+std::fmod(position-v.begin,v.end-v.begin);if(position<v.begin)position+=v.end-v.begin;}
                    const float w=bank->hann(float(h.age)/float(window-1));
                    if(position>=v.begin && position<v.end){const auto index=size_t(position),next=double(index+1)<v.end?index+1:(v.loop?size_t(v.begin):index);const float f=float(position-double(index));l+=(dataL[index]+f*(dataL[next]-dataL[index]))*w;rValue+=(dataR[index]+f*(dataR[next]-dataR[index]))*w;}
                    weight+=w;h.position+=v.direction*sourceStep*v.basePitch*pitch;if(++h.age>=window)h.active=false;
                }
                if(weight>1e-7f){l/=weight;rValue/=weight;}
                if(p.bypassTone){v.filterL=l;v.filterR=rValue;}else{v.filterL+=fc*(l-v.filterL);v.filterR+=fc*(rValue-v.filterR);}
                const float edge=v.loop?1.0f:float(std::clamp(std::min(v.clock-v.begin,v.end-v.clock)/std::max(1.0,sample->rate*.002),0.0,1.0));
                const float amplitude=v.envelope*v.velocity*edge;sumL+=v.filterL*amplitude;sumR+=v.filterR*amplitude;v.clock+=v.direction*scan;
            }
            gain+=smooth*(p.gain-gain);const float l=dry?sumL*gain:std::tanh(sumL*gain*.5f),rValue=dry?sumR*gain:std::tanh(sumR*gain*.5f);
            if(right){left[i]+=l;right[i]+=rValue;}else left[i]+=(l+rValue)*.5f;
        }
    }
};
}
