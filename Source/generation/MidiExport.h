#pragma once
#include "Pattern.h"
#include <vector>
namespace wee {
inline std::vector<uint8_t> patternMidi(const Pattern& p,int bpm){
    if(!p.valid() || p.count==0 || bpm<20 || bpm>400)return {};
    std::vector<uint8_t> track;
    auto variable=[&](uint32_t n){uint8_t bytes[4];int count=0;bytes[count++]=uint8_t(n&127);while((n>>=7)!=0)bytes[count++]=uint8_t((n&127)|128);while(count)track.push_back(bytes[--count]);};
    const int micros=60000000/bpm;track={0,0xff,0x51,3,uint8_t(micros>>16),uint8_t(micros>>8),uint8_t(micros),0,0xff,0x58,4,4,2,24,8};
    auto prepared=p;prepared.prepare();uint32_t previous=0;
    for(int i=0;i<p.count*2;++i){const auto& e=prepared.events[size_t(i)];const auto tick=uint32_t(std::llround(e.beat*960));variable(tick-previous);previous=tick;track.push_back(e.on?0x90:0x80);track.push_back(uint8_t(e.note));track.push_back(uint8_t(e.velocity));}
    variable(uint32_t(p.beats()*960)-previous);track.insert(track.end(),{0xff,0x2f,0});
    const auto n=uint32_t(track.size());std::vector<uint8_t> file{'M','T','h','d',0,0,0,6,0,0,0,1,3,0xc0,'M','T','r','k',uint8_t(n>>24),uint8_t(n>>16),uint8_t(n>>8),uint8_t(n)};file.insert(file.end(),track.begin(),track.end());return file;
}
}
