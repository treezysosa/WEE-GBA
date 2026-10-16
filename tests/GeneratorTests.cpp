#include "generation/PatternPlayer.h"
#include "generation/MidiExport.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <new>
static bool audio=false;static size_t allocations=0;
void* operator new(std::size_t n){if(audio)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
void check(bool ok,const char* text){if(!ok){std::cerr<<text<<'\n';std::exit(1);}}
bool equal(const wee::Pattern& a,const wee::Pattern& b){if(a.count!=b.count || a.bars!=b.bars || a.mode!=b.mode || a.chords!=b.chords)return false;for(int i=0;i<a.count;++i){const auto& x=a.notes[size_t(i)];const auto& y=b.notes[size_t(i)];if(x.beat!=y.beat || x.duration!=y.duration || x.note!=y.note || x.velocity!=y.velocity || x.degree!=y.degree)return false;}return true;}
uint32_t var(const std::vector<uint8_t>& b,size_t& i){uint32_t value=0;for(int n=0;n<4;++n){check(i<b.size(),"MIDI variable bounds");const auto c=b[i++];value=(value<<7)|(c&127);if(!(c&128))return value;}check(false,"invalid MIDI variable");return 0;}
int main(){
    for(int style=0;style<14;++style)for(int mode=0;mode<4;++mode)for(int bars:{1,2,4,8,16})for(int scale=0;scale<7;++scale){
        wee::GeneratorSettings c;c.style=style;c.mode=mode;c.bars=bars;c.scale=scale;c.seed=uint32_t(17+style*31+mode*997);c.root=11;c.complexity=85;c.density=90;c.humanize=100;c.swing=100;c.variation=100;
        const auto a=wee::generatePattern(c,nullptr,7,60),b=wee::generatePattern(c,nullptr,7,60);check(a.valid() && a.count>0,"all style/mode/length/scale bounds");check(equal(a,b),"same seed/settings deterministic");
        for(int i=0;i<a.count;++i){const auto& n=a.notes[size_t(i)];if(mode==3)check(n.note>=60 && n.note<67,"chop keys use available slices");else{bool found=false;for(int d=0;d<36;++d)found|=n.note==wee::scalePitch(c,d);check(found,"notes remain in key/scale");}}
    }
    wee::GeneratorSettings c;c.bars=8;const auto first=wee::generatePattern(c);++c.seed;const auto next=wee::generatePattern(c);check(!equal(first,next),"next seed varies output");
    c.lockMelody=true;c.lockRhythm=true;c.density=1;c.swing=100;c.humanize=100;const auto locked=wee::generatePattern(c,&first);check(locked.count==first.count,"locks retain event count");for(int i=0;i<first.count;++i){check(locked.notes[size_t(i)].degree==first.notes[size_t(i)].degree,"melody lock degrees");check(locked.notes[size_t(i)].beat==first.notes[size_t(i)].beat && locked.notes[size_t(i)].duration==first.notes[size_t(i)].duration,"rhythm lock exact timing");}
    c.bars=16;const auto longer=wee::generatePattern(c,&first);check(longer.valid() && longer.count==first.count,"locked rhythm rescales on bar change");check(longer.notes[1].beat==first.notes[1].beat*2,"locked timing length ratio");
    c.lockKey=true;const auto random=wee::randomSettings(c);check(random.root==c.root && random.scale==c.scale,"key lock survives randomize");c.lockKey=false;bool changed=false;for(uint32_t s=1;s<30;++s){c.seed=s;const auto r=wee::randomSettings(c);changed|=r.root!=c.root || r.scale!=c.scale;}check(changed,"unlocked random key");
    auto invalid=first;invalid.notes[0].duration=std::numeric_limits<double>::quiet_NaN();check(!invalid.valid(),"reject nonfinite pattern");invalid=first;invalid.count=1025;check(!invalid.valid(),"reject oversized pattern");
    // Exact event samples and identical scheduling across callback partitions/rates.
    for(double rate:{44100.0,48000.0,96000.0})for(int block:{37,128,511,1024}){
        wee::GeneratorSettings settings;settings.bars=1;settings.humanize=0;settings.swing=0;settings.density=100;auto pattern=wee::generatePattern(settings);wee::PatternPlayer player;
        const int total=int(rate*4.25);int seen=0,rendered=0;
        for(int offset=0;offset<total;offset+=block){const int n=std::min(block,total-offset);int cursor=0;audio=true;player.begin(&pattern,true,n,rate,120,false,true,false,0);player.until(n,[&](int from,int to){check(from==cursor,"contiguous render ranges");cursor=to;rendered+=to-from;},[&](const wee::PatternEvent& e){const auto& expected=pattern.events[size_t(seen%(pattern.count*2))];const int cycle=seen/(pattern.count*2);const auto sample=int64_t(std::ceil((cycle*pattern.beats()+expected.beat)*rate/2-1e-7));check(int64_t(offset+cursor)==sample && e.on==expected.on && e.note==expected.note,"sample exact scheduled event");++seen;});audio=false;}
        check(seen>pattern.count*4 && rendered==total,"loop wraps and renders all frames");audio=true;check(player.begin(&pattern,false,block,rate,120,false,false,false,0),"disable clears player lane");player.reset();check(!player.begin(&pattern,true,block,rate,120,true,false,true,0),"stopped host does not start");player.begin(&pattern,true,block,rate,120,true,true,true,0);check(player.begin(&pattern,true,block,rate,120,true,true,true,3),"seek clears lane");audio=false;
    }
    check(allocations==0,"no pattern playback allocation");
    // Parse the entire SMF track independently, including all note pairs and exact end tick.
    auto bytes=wee::patternMidi(first,123);check(bytes.size()>30 && bytes[0]=='M' && bytes[8]==0 && bytes[9]==0 && bytes[12]==3 && bytes[13]==0xc0,"format 0 SMF / 960 PPQ");
    const uint32_t trackSize=uint32_t(bytes[18])<<24|uint32_t(bytes[19])<<16|uint32_t(bytes[20])<<8|bytes[21];check(trackSize+22==bytes.size(),"MIDI track length");size_t i=22;uint32_t tick=0;int ons=0,offs=0;bool tempo=false,end=false;std::array<int,128> held {};
    while(i<bytes.size()){tick+=var(bytes,i);const int status=bytes[i++];if(status==0xff){const auto type=bytes[i++];const auto n=var(bytes,i);check(i+n<=bytes.size(),"MIDI meta length");if(type==0x51){check(n==3 && (int(bytes[i])<<16|int(bytes[i+1])<<8|bytes[i+2])==60000000/123,"MIDI tempo");tempo=true;}if(type==0x2f){check(n==0 && tick==first.bars*4*960,"exact MIDI loop end");end=true;}i+=n;}else{check((status==0x90 || status==0x80) && i+2<=bytes.size(),"MIDI note encoding");const auto pitch=bytes[i++],v=bytes[i++];if(status==0x90){check(v>0 && v<=127,"MIDI velocity");++ons;++held[pitch];}else{++offs;--held[pitch];check(held[pitch]>=0,"note-off follows note-on");}}}
    for(auto h:held)check(h==0,"all MIDI notes released");check(tempo && end && ons==first.count && offs==first.count,"complete MIDI export");
    std::cout<<"Generator tests passed: 1960 style/mode/length/scale combinations, seeds/locks, bounds, sample-exact loops/transport, zero playback allocation and parsed MIDI.\n";
}
