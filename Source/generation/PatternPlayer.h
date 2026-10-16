#pragma once
#include "Pattern.h"
namespace wee {
class PatternPlayer {
    const Pattern* pattern=nullptr;
    double expected=0,freeBeat=0,start=0,perSample=0;
    int index=0,cursor=0,block=0;int64_t cycle=0;bool running=false;
public:
    void reset()noexcept{pattern=nullptr;running=false;expected=freeBeat=0;}
    bool begin(const Pattern* p,bool enabled,int samples,double rate,double bpm,bool host,bool playing,bool hasPpq,double ppq)noexcept{
        const bool active=enabled && p && p->count>0 && (!host || playing);
        start=host && hasPpq?ppq:freeBeat;if(active && !running && !(host && hasPpq))start=0;
        const bool discontinuity=p!=pattern || !running || !active || std::abs(start-expected)>.002;
        const bool clear=running && discontinuity;
        pattern=p;cursor=0;block=samples;perSample=std::clamp(bpm,20.0,400.0)/60/std::max(8000.0,rate);
        if(active && discontinuity){cycle=int64_t(std::floor(start/p->beats()));const double phase=start-double(cycle)*p->beats();index=0;while(index<p->count*2 && p->events[size_t(index)].beat<phase-1e-8)++index;if(index==p->count*2){index=0;++cycle;}}
        running=active;expected=start+samples*perSample;if(active)freeBeat=expected;return clear;
    }
    template<class Render,class Note>void until(int end,Render&& render,Note&& note)noexcept{
        end=std::clamp(end,cursor,block);
        if(!running){if(end>cursor)render(cursor,end);cursor=end;return;}
        while(cursor<end){const auto& event=pattern->events[size_t(index)];const double beat=double(cycle)*pattern->beats()+event.beat;const int at=int(std::clamp(std::ceil((beat-start)/perSample-1e-7),0.0,double(block)));
            if(at>=end){render(cursor,end);cursor=end;break;}if(at>cursor){render(cursor,at);cursor=at;}note(event);
            if(++index>=pattern->count*2){index=0;++cycle;}
        }
    }
};
}
