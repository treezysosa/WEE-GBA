#pragma once
#include "VocalConfig.h"
#include <limits>
namespace wee {
class ChopSequencer {
    VocalConfig config;
    double freePpq=0,start=0,perSample=0,stepBeats=.25,gatePpq=0,expected=0;
    int cursor=0,blockSize=0,held=-1,sliceCount=1;
    int64_t nextStep=0,lastStep=std::numeric_limits<int64_t>::min();
    bool running=false,active=false,clear=false;
public:
    void reset()noexcept{freePpq=expected=0;running=active=false;held=-1;lastStep=std::numeric_limits<int64_t>::min();}
    // Returns true when a transport/config discontinuity requires stopping lane 1.
    bool begin(const VocalConfig& c,int samples,double rate,double bpm,bool host,bool playing,bool hasPpq,double ppq,int slices)noexcept{
        constexpr double divisions[]{.25,.5,1,.125};
        const double beats=divisions[std::clamp(int(c.v[VSeqDivision]),0,3)];
        active=c.v[VSeqEnabled]>=.5f && (!host || playing);clear=false;
        start=host && hasPpq ? ppq : freePpq;
        if(active && !running && !(host && hasPpq))start=0;
        if(!active || !running || beats!=stepBeats || (host && hasPpq && std::abs(start-expected)>.02)){
            clear=running || held>=0;held=-1;lastStep=std::numeric_limits<int64_t>::min();
        }
        config=c;stepBeats=beats;perSample=std::clamp(bpm,20.0,400.0)/60/std::max(8000.0,rate);cursor=0;blockSize=samples;sliceCount=slices;
        nextStep=int64_t(std::ceil(start/stepBeats-1e-8));if(nextStep==lastStep)++nextStep;
        running=active;expected=start+samples*perSample;if(active)freePpq=expected;
        return clear;
    }
    template<class Render,class On,class Off>void until(int end,Render&& render,On&& on,Off&& off)noexcept{
        end=std::clamp(end,cursor,blockSize);
        if(!active){if(end>cursor)render(cursor,end);cursor=end;return;}
        auto sampleAt=[&](double ppq){const double frame=(ppq-start)/perSample;return int(std::clamp(std::ceil(frame-1e-7),0.0,double(blockSize)));};
        while(cursor<end){
            const double nextPpq=double(nextStep)*stepBeats;
            const int stepAt=sampleAt(nextPpq),offAt=held>=0?sampleAt(gatePpq):blockSize;
            const int event=std::min(stepAt,offAt);
            if(event>=end){render(cursor,end);cursor=end;break;}
            if(event>cursor){render(cursor,event);cursor=event;}
            if(held>=0 && offAt<=stepAt){off(held);held=-1;}
            if(stepAt<=offAt){
                if(held>=0){off(held);held=-1;}
                const int length=std::clamp(int(config.v[VSeqLength]),1,16);
                const int step=int((nextStep%length+length)%length),chop=int(config.v[VStep1+size_t(step)])-1;
                if(chop>=0 && chop<sliceCount){held=int(config.v[VBase])+chop;on(held,config.v[VSeqVelocity]);gatePpq=nextPpq+stepBeats*config.v[VSeqGate];}
                lastStep=nextStep;++nextStep;
            }
        }
    }
};
}
