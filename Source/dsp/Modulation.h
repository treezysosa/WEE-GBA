#pragma once
#include "LayerConfig.h"
namespace wee {
class LFO {
    double phase=0;
public:
    void reset() noexcept { phase=0; }
    float advance(int count, double sampleRate, const LayerConfig& c, double bpm) noexcept {
        constexpr double divisions[] {.25,.5,1,2,4,8,16};
        const double hz=c.v[LFOSync]>=.5f ? std::clamp(bpm,20.0,400.0)/60/divisions[std::clamp(int(c.v[LFOBeats]),0,6)] : c.v[LFORate];
        double at=phase + double(count)*hz/sampleRate*.5; at-=std::floor(at);
        phase+=double(count)*hz/sampleRate; phase-=std::floor(phase);
        switch(int(c.v[LFOShape])) {
            case 1: return float(1-4*std::abs(at-.5));
            case 2: return at<.5 ? 1.0f : -1.0f;
            default: return float(std::sin(at*6.283185307179586));
        }
    }
};
struct ModValues { float pitch=0, cutoff=0, wt=0, amplitude=0, pan=0, fm=0, grain=0; };
inline ModValues routeModulation(const LayerConfig& c,float lfo,float velocity,float wheel,float pressure) noexcept {
    const float sources[] {0,lfo,velocity,wheel,pressure};
    std::array<float,7> result {};
    for(size_t r=0;r<4;++r) {
        const size_t base=Route1Source+r*3;
        const int source=std::clamp(int(c.v[base]),0,4), target=std::clamp(int(c.v[base+1]),0,6);
        result[size_t(target)]+=sources[source]*c.v[base+2];
    }
    return {std::clamp(result[0]*12,-24.0f,24.0f),std::clamp(result[1]*4,-6.0f,6.0f),result[2],result[3],result[4],result[5]*8,result[6]};
}
class StereoFilter {
    struct State { float ic1=0,ic2=0; };
    std::array<State,2> states {};
    float a1=1,a2=0,a3=0,k=2;
    int mode=0;
public:
    void reset() noexcept { states={}; }
    void configure(int type,float cutoff,float resonance,double sampleRate) noexcept {
        mode=type;
        if(!mode) return;
        const float g=std::tan(float(3.14159265358979323846*std::clamp(double(cutoff),20.0,sampleRate*.45)/sampleRate));
        k=2-1.9f*std::clamp(resonance,0.0f,.95f);
        a1=1/(1+g*(g+k)); a2=g*a1; a3=g*a2;
    }
    float process(float x,int channel) noexcept {
        if(!mode) return x;
        auto& s=states[size_t(channel)];
        const float v3=x-s.ic2, v1=a1*s.ic1+a2*v3, v2=s.ic2+a2*s.ic1+a3*v3;
        s.ic1=2*v1-s.ic1; s.ic2=2*v2-s.ic2;
        return mode==1 ? v2 : (mode==2 ? x-k*v1-v2 : v1);
    }
};
}
