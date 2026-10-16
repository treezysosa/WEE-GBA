#pragma once
#include <array>
#include <cmath>
#include <algorithm>
namespace wee {
class WavetableBank {
    static constexpr int size=1024, levels=10, frames=4;
    std::array<float, size*levels*frames> data {};
    std::array<float,size> window {};
    bool prepared=false;
    static constexpr double pi=3.14159265358979323846;
public:
    // Fourier band-limited mip levels are created once outside playback.
    void prepare() {
        if(prepared) return;
        for(int i=0;i<size;++i) window[size_t(i)]=float(.5-.5*std::cos(2*pi*i/(size-1)));
        for(int level=0;level<levels;++level) for(int i=0;i<size;++i) {
            const double phase=2*pi*i/size;
            double triangle=0,saw=0,pulse=0;
            const int harmonics=std::min(511,1<<level);
            for(int h=1;h<=harmonics;++h) {
                const double sine=std::sin(phase*h);
                saw-=2/pi*sine/h;
                if(h%2) { triangle+=8/(pi*pi)*((h%4==1)?1:-1)*sine/(h*h); pulse+=4/pi*sine/h; }
            }
            data[size_t((0*levels+level)*size+i)]=float(std::sin(phase));
            data[size_t((1*levels+level)*size+i)]=float(triangle);
            data[size_t((2*levels+level)*size+i)]=float(saw);
            data[size_t((3*levels+level)*size+i)]=float(pulse);
        }
        prepared=true;
    }
    static int levelFor(double increment) noexcept {
        int level=0;
        const double allowed=.45/std::max(.00000001,increment);
        while(level<levels-1 && double(1<<(level+1))<=allowed) ++level;
        return level;
    }
    float lookup(double phase, float position, double increment) const noexcept { return lookupLevel(phase,position,levelFor(increment)); }
    float lookupLevel(double phase, float position, int level) const noexcept {
        const float frame=std::clamp(position,0.0f,1.0f)*3;
        const int f0=std::min(2,int(frame)); const float mix=frame-float(f0);
        phase-=std::floor(phase);
        const double offset=phase*size;
        const int index=int(offset); const float fraction=float(offset-index);
        auto read=[&](int f) {
            const auto base=size_t((f*levels+level)*size);
            const auto a=data[base+size_t(index)], b=data[base+size_t((index+1)%size)];
            return a+fraction*(b-a);
        };
        const float a=read(f0); return a+mix*(read(f0+1)-a);
    }
    float hann(float phase) const noexcept {
        const float offset=std::clamp(phase,0.0f,1.0f)*(size-1);
        const int i=std::min(size-2,int(offset));
        return window[size_t(i)]+(offset-float(i))*(window[size_t(i+1)]-window[size_t(i)]);
    }
};
}
