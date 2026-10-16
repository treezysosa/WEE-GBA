#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
namespace wee {
constexpr int maxSlices=32;
struct SliceMap {
    int count=1;
    std::array<double,maxSlices+1> points {0,1};
    bool valid() const noexcept {
        if(count<1 || count>maxSlices || points[0]!=0 || points[size_t(count)]!=1) return false;
        for(int i=1;i<=count;++i) if(!std::isfinite(points[size_t(i)]) || points[size_t(i)]-points[size_t(i-1)]<.000001) return false;
        return true;
    }
    int sliceAt(double position) const noexcept {for(int i=0;i<count;++i) if(position<points[size_t(i+1)]) return i;return count-1;}
    bool add(double point) noexcept {
        if(count>=maxSlices || point<=0 || point>=1 || !std::isfinite(point)) return false;
        const int at=sliceAt(point)+1;
        if(point-points[size_t(at-1)]<.000001 || points[size_t(at)]-point<.000001) return false;
        for(int i=count+1;i>at;--i)points[size_t(i)]=points[size_t(i-1)];
        points[size_t(at)]=point;++count;return true;
    }
    bool remove(int marker) noexcept {if(marker<=0 || marker>=count) return false;for(int i=marker;i<count;++i)points[size_t(i)]=points[size_t(i+1)];--count;return true;}
};
struct SliceDocument { SliceMap map; bool manual=false; };
}
