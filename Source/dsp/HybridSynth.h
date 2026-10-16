#pragma once
#include "LayerConfig.h"
#include "Wavetable.h"
namespace wee {
class HybridSynth {
    struct Grain { double position=0; int age=0,length=1; bool active=false; };
    struct Voice {
        int note=0,stage=0;
        float velocity=0,envelope=0,filterL=0,filterR=0;
        double phase=0,secondPhase=0,increment=0,sampleIncrement=1,begin=0,end=0;
        bool held=false,sustained=false;
        uint64_t age=0;
        uint32_t seed=1;
        double countdown=0,dt=0,dtSecond=0;
        int mip=0,mipSecond=0;
        std::array<Grain,4> grains {};
    };
    std::array<Voice,32> voices {};
    const WavetableBank* bank=nullptr;
    const SampleData* sample=nullptr;
    double rate=44100,bend=1;
    uint64_t clock=0;
    bool pedal=false;
    float gain=0;
    static double wrap(double phase) noexcept { return phase-std::floor(phase); }
public:
    void prepare(double sr,const WavetableBank& tables) noexcept { rate=std::max(8000.0,sr); bank=&tables; reset(); }
    void reset() noexcept { voices={}; bend=1; pedal=false; gain=0; clock=0; }
    void setSample(const SampleData* asset, bool stopVoices=true) noexcept { if(stopVoices) allNotesOff(true); sample=asset; }
    void pitchWheel(int value) noexcept { bend=std::pow(2.0,(value-8192)/8192.0*2/12); }
    void sustainPedal(bool down) noexcept {
        pedal=down;
        if(!down) for(auto& v:voices) if(v.sustained && !v.held) { v.sustained=false; v.stage=4; }
    }
    void noteOn(int note,float velocity,const LayerConfig& c) noexcept {
        if(velocity<=0) { noteOff(note); return; }
        const bool granular=int(c.v[SynthMode])==3;
        const auto mapping=c.mapping();
        if(note<mapping.low || note>mapping.high || (granular && (!sample || !sample->valid()))) return;
        double begin=0,end=0;
        if(granular) {
            begin=std::floor(std::clamp(double(mapping.start),0.0,1.0)*(double(sample->left.size())-1));
            end=std::floor(std::clamp(double(mapping.end),0.0,1.0)*double(sample->left.size()));
            if(end-begin<2) return;
        }
        Voice* target=nullptr;
        for(auto& v:voices) if(!v.stage) { target=&v; break; }
        if(!target) target=&*std::min_element(voices.begin(),voices.end(),[](auto& a,auto& b){return a.age<b.age;});
        *target={}; target->note=note; target->velocity=std::clamp(velocity,0.0f,1.0f);
        target->stage=1; target->held=true; target->age=++clock;
        target->increment=440*std::pow(2.0,(note+mapping.transpose-69)/12.0)/rate;
        target->sampleIncrement=granular ? sample->rate/rate*std::pow(2.0,(note+mapping.transpose-mapping.root)/12.0) : 1;
        target->begin=begin; target->end=end;
        target->seed=uint32_t(note+1)*747796405u+uint32_t(clock)*2891336453u;
    }
    void noteOff(int note) noexcept {
        for(auto& v:voices) if(v.note==note && v.held) { v.held=false; v.sustained=pedal; if(!pedal) v.stage=4; }
    }
    void allNotesOff(bool immediate=false) noexcept {
        pedal=false;
        for(auto& v:voices) { v.held=v.sustained=false; v.stage=immediate ? 0 : 4; }
    }
    int activeVoices() const noexcept { int n=0; for(const auto& v:voices) n+=v.stage!=0; return n; }
    void render(float* left,float* right,int count,const Parameters& p,const LayerConfig& c,float pitch,float wt,float fm,float grainPosition) noexcept {
        if(!bank) return;
        const int mode=int(c.v[SynthMode]);
        if(mode==3 && (!sample || !sample->valid())) return;
        const double multiplier=bend*std::pow(2.0,pitch/12.0);
        const double detune=std::pow(2.0,c.v[Detune]/1200.0);
        const float attack=float(1/(rate*std::max(.001f,p.attack))), decay=float((1-p.sustain)/(rate*std::max(.001f,p.decay))), release=float(1/(rate*std::max(.001f,p.release)));
        const float smooth=1-std::exp(float(-1/(rate*.005)));
        const float fc=1-std::exp(float(-6.283185307179586*std::clamp(double(p.cutoff),20.0,rate*.45)/rate));
        for(auto& v:voices) if(v.stage) {
            v.dt=std::min(.45,v.increment*multiplier);
            v.dtSecond=std::min(.45,v.dt*detune);
            if(mode==1){v.mip=bank->levelFor(v.dt);v.mipSecond=bank->levelFor(v.dtSecond);}
        }
        for(int i=0;i<count;++i) {
            float sumL=0,sumR=0;
            for(auto& v:voices) {
                if(!v.stage) continue;
                if(v.stage==1) { v.envelope+=attack; if(v.envelope>=1) {v.envelope=1;v.stage=2;} }
                else if(v.stage==2) {v.envelope-=decay; if(v.envelope<=p.sustain){v.envelope=p.sustain;v.stage=3;}}
                else if(v.stage==3) v.envelope=p.sustain;
                else {v.envelope-=release;if(v.envelope<=0){v.stage=0;continue;}}
                float l=0,r=0;
                const double dt=v.dt;
                if(mode==1) {
                    const float a=bank->lookupLevel(v.phase,wt,v.mip), b=bank->lookupLevel(v.secondPhase,wt,v.mipSecond);
                    l=r=a+c.v[OscMix]*(b-a);
                    v.phase=wrap(v.phase+dt); v.secondPhase=wrap(v.secondPhase+v.dtSecond);
                } else if(mode==2) {
                    l=r=float(std::sin(6.283185307179586*v.phase+fm*std::sin(6.283185307179586*v.secondPhase)));
                    v.phase=wrap(v.phase+dt); v.secondPhase=wrap(v.secondPhase+std::min(.45,dt*c.v[FMRatio]));
                } else if(mode==3) {
                    if(v.countdown<=0) {
                        for(auto& g:v.grains) if(!g.active) {
                            v.seed^=v.seed<<13;v.seed^=v.seed>>17;v.seed^=v.seed<<5;
                            const double random=double(v.seed)/4294967295.0*2-1;
                            double pos=double(grainPosition)+random*c.v[GrainSpray]; pos-=std::floor(pos);
                            g.position=v.begin+pos*(v.end-v.begin); g.age=0; g.length=std::max(2,int(rate*c.v[GrainSize]*.001)); g.active=true;
                            break;
                        }
                        v.countdown+=rate/std::max(1.0f,c.v[GrainDensity]);
                    }
                    --v.countdown;
                    for(auto& g:v.grains) if(g.active) {
                        g.position=v.begin+std::fmod(g.position-v.begin,v.end-v.begin);
                        const auto index=size_t(g.position);
                        const auto next=double(index+1)<v.end ? index+1 : size_t(v.begin);
                        const float f=float(g.position-double(index)), window=bank->hann(float(g.age)/float(g.length-1))*.5f;
                        l+=(sample->left[index]+f*(sample->left[next]-sample->left[index]))*window;
                        r+=(sample->right[index]+f*(sample->right[next]-sample->right[index]))*window;
                        g.position+=v.sampleIncrement*multiplier;
                        if(++g.age>=g.length) g.active=false;
                    }
                }
                if(p.bypassTone) {v.filterL=l;v.filterR=r;}
                else {v.filterL+=fc*(l-v.filterL);v.filterR+=fc*(r-v.filterR);}
                sumL+=v.filterL*v.envelope*v.velocity;sumR+=v.filterR*v.envelope*v.velocity;
            }
            gain+=smooth*(p.gain-gain);
            left[i]+=std::tanh(sumL*gain*.5f); if(right) right[i]+=std::tanh(sumR*gain*.5f);
        }
    }
};
}
