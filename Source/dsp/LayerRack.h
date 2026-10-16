#pragma once
#include "HybridSynth.h"
#include "Modulation.h"
namespace wee {
class LayerRack {
    struct Layer {
        LayerConfig config;
        Engine analog;
        Sampler sampler;
        HybridSynth hybrid;
        StereoFilter filter;
        LFO lfo;
        float velocity=0;
        int previousEngine=-1;
        bool wasEnabled=false;
        void stop(bool immediate) noexcept {
            analog.allNotesOff(immediate); sampler.allNotesOff(immediate); hybrid.allNotesOff(immediate);
            if(immediate) filter.reset();
        }
    };
    WavetableBank tables;
    std::array<Layer,layerCount> layers;
    double rate=44100,tempo=120;
    float wheel=0,pressure=0,master=1,currentMaster=1;
    int bendValue=8192;
    bool pedal=false;
public:
    LayerRack() { for(size_t i=0;i<layerCount;++i) layers[i].config=LayerConfig(i); }
    void prepare(double sr) {
        rate=std::max(8000.0,sr); tables.prepare(); wheel=pressure=0; bendValue=8192;pedal=false;currentMaster=master;
        for(auto& l:layers) {l.analog.prepare(rate);l.sampler.prepare(rate);l.hybrid.prepare(rate,tables);l.filter.reset();l.lfo.reset();l.previousEngine=-1;l.wasEnabled=false;}
    }
    void setSample(const SampleData* sample) noexcept {
        for(auto& l:layers) {
            l.sampler.setSample(sample);l.hybrid.setSample(sample,l.config.engine()==4);
            l.sampler.pitchWheel(bendValue);l.hybrid.pitchWheel(bendValue);
            l.sampler.sustainPedal(pedal);l.hybrid.sustainPedal(pedal);
        }
    }
    void configure(const std::array<LayerConfig,layerCount>& settings,float masterGain,double bpm) noexcept {
        master=std::clamp(masterGain,0.0f,1.0f);tempo=bpm;
        for(size_t i=0;i<layerCount;++i) {
            auto& l=layers[i]; const bool enabled=settings[i].v[Enabled]>=.5f;
            if(l.previousEngine!=settings[i].engine() || l.wasEnabled!=enabled) {
                l.stop(true);
                l.analog.pitchWheel(bendValue);l.sampler.pitchWheel(bendValue);l.hybrid.pitchWheel(bendValue);
                l.analog.sustainPedal(pedal);l.sampler.sustainPedal(pedal);l.hybrid.sustainPedal(pedal);
            }
            l.config=settings[i];l.previousEngine=settings[i].engine();l.wasEnabled=enabled;
        }
    }
    void noteOn(int note,float velocity,bool sampleReady=true) noexcept {
        if(velocity<=0) {noteOff(note);return;}
        for(auto& l:layers) {
            const auto& c=l.config;
            if(c.v[Enabled]<.5f || note<c.v[Low] || note>c.v[High]) continue;
            l.velocity=velocity;
            switch(c.engine()) {
                case 0:l.analog.noteOn(note,velocity,int(c.v[Transpose]));break;
                case 1:if(sampleReady) l.sampler.noteOn(note,velocity,c.mapping());break;
                case 2:case 3:l.hybrid.noteOn(note,velocity,c);break;
                case 4:if(sampleReady) l.hybrid.noteOn(note,velocity,c);break;
            }
        }
    }
    void noteOff(int note) noexcept {for(auto& l:layers){l.analog.noteOff(note);l.sampler.noteOff(note);l.hybrid.noteOff(note);}}
    void allNotesOff(bool immediate=false) noexcept {pedal=false;for(auto& l:layers) l.stop(immediate);}
    void pitchWheel(int value) noexcept {bendValue=value;for(auto& l:layers){l.analog.pitchWheel(value);l.sampler.pitchWheel(value);l.hybrid.pitchWheel(value);}}
    void sustainPedal(bool down) noexcept {pedal=down;for(auto& l:layers){l.analog.sustainPedal(down);l.sampler.sustainPedal(down);l.hybrid.sustainPedal(down);}}
    void modWheel(float value) noexcept {wheel=std::clamp(value,0.0f,1.0f);}
    void aftertouch(float value) noexcept {pressure=std::clamp(value,0.0f,1.0f);}
    int activeVoices(size_t layer) const noexcept { const auto& l=layers[layer];return l.analog.activeVoices()+l.sampler.activeVoices()+l.hybrid.activeVoices(); }
    int activeVoices() const noexcept {int n=0;for(size_t i=0;i<layerCount;++i)n+=activeVoices(i);return n;}
    void render(float* left,float* right,int count) noexcept {
        const float smooth=1-std::exp(float(-1/(rate*.005)));
        for(int offset=0;offset<count;offset+=16) {
            const int n=std::min(16,count-offset);
            std::array<float,16> sumL{},sumR{};
            for(auto& l:layers) {
                const auto& c=l.config;if(c.v[Enabled]<.5f) continue;
                const auto m=routeModulation(c,l.lfo.advance(n,rate,c,tempo),l.velocity,wheel,pressure);
                auto p=c.envelope();p.cutoff=std::clamp(p.cutoff*std::pow(2.0f,m.cutoff),20.0f,20000.0f);p.bypassTone=c.v[FilterMode]>=.5f;
                const auto pitch=std::pow(2.0,m.pitch/12.0);
                std::array<float,16> a{},b{};
                if(c.engine()==0) {l.analog.pitchMultiplier(pitch);l.analog.render(a.data(),b.data(),n,p);}
                else if(c.engine()==1) {l.sampler.pitchMultiplier(pitch);l.sampler.render(a.data(),b.data(),n,p);}
                else l.hybrid.render(a.data(),b.data(),n,p,c,m.pitch,std::clamp(c.v[WTPosition]+m.wt,0.0f,1.0f),std::clamp(c.v[FMIndex]+m.fm,0.0f,8.0f),std::clamp(c.v[GrainPosition]+m.grain,0.0f,1.0f));
                l.filter.configure(int(c.v[FilterMode]),p.cutoff,c.v[Resonance],rate);
                const float pan=std::clamp(c.v[Pan]+m.pan,-1.0f,1.0f), amplitude=std::clamp(1+m.amplitude,0.0f,2.0f);
                const float balanceL=std::sqrt(1-std::max(0.0f,pan))*amplitude,balanceR=std::sqrt(1+std::min(0.0f,pan))*amplitude;
                for(int i=0;i<n;++i){sumL[size_t(i)]+=l.filter.process(a[size_t(i)],0)*balanceL;sumR[size_t(i)]+=l.filter.process(b[size_t(i)],1)*balanceR;}
            }
            for(int i=0;i<n;++i){
                currentMaster+=smooth*(master-currentMaster);
                const float a=std::clamp(sumL[size_t(i)]*currentMaster,-1.0f,1.0f),b=std::clamp(sumR[size_t(i)]*currentMaster,-1.0f,1.0f);
                left[offset+i]+=right ? a : (a+b)*.5f;if(right)right[offset+i]+=b;
            }
        }
    }
};
}
