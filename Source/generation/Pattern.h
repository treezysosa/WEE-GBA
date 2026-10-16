#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
namespace wee {
inline constexpr int maxPatternNotes=1024;
inline constexpr const char* styleNames="Pop|Dark pop|Indie pop|Soul|R&B|Trap|Lo-fi|City pop|Ambient|Horror|Retro video game|Cinematic|Rock|Dream pop";
inline constexpr const char* scaleNames="Major|Natural minor|Dorian|Mixolydian|Minor pentatonic|Harmonic minor|Phrygian";
inline constexpr const char* modeNames="Melody|Chords|Arpeggio|Vocal chops";
struct GeneratorSettings {
    uint32_t seed=12345;
    int style=0,mode=0,bars=4,root=0,scale=0,octave=4;
    int complexity=40,humanize=10,swing=0,density=55,variation=35,tempo=120;
    bool lockMelody=false,lockRhythm=false,lockKey=true,lockSound=true,hostSync=true;
    bool valid()const noexcept{return seed<=2147483647u && style>=0 && style<14 && mode>=0 && mode<4 && (bars==1 || bars==2 || bars==4 || bars==8 || bars==16) && root>=0 && root<12 && scale>=0 && scale<7 && octave>=2 && octave<=6 && complexity>=0 && complexity<=100 && humanize>=0 && humanize<=100 && swing>=0 && swing<=100 && density>=0 && density<=100 && variation>=0 && variation<=100 && tempo>=40 && tempo<=240;}
};
struct PatternNote {double beat=0,duration=.25;int note=60,velocity=100,degree=0;};
struct PatternEvent {double beat=0;int note=60,velocity=0;bool on=false;int degree=0;};
struct Pattern {
    int count=0,bars=4,mode=0;
    std::array<int,16> chords {};
    std::array<PatternNote,maxPatternNotes> notes {};
    std::array<PatternEvent,maxPatternNotes*2> events {};
    double beats()const noexcept{return bars*4.0;}
    bool valid()const noexcept {
        if(count<0 || count>maxPatternNotes || (bars!=1 && bars!=2 && bars!=4 && bars!=8 && bars!=16) || mode<0 || mode>3)return false;
        for(auto c:chords)if(c<0 || c>6)return false;
        double previous=-1;
        for(int i=0;i<count;++i){const auto& n=notes[size_t(i)];if(!std::isfinite(n.beat) || !std::isfinite(n.duration) || n.beat<0 || n.beat<previous || n.duration<.001 || n.beat+n.duration>beats()+1e-7 || n.note<0 || n.note>127 || n.velocity<1 || n.velocity>127 || n.degree<0 || n.degree>35 || (mode==3 && n.degree>31))return false;previous=n.beat;}return true;
    }
    void prepare()noexcept {
        for(int i=0;i<count;++i){const auto& n=notes[size_t(i)];events[size_t(i*2)]={n.beat,n.note,n.velocity,true,n.degree};events[size_t(i*2+1)]={n.beat+n.duration,n.note,0,false,n.degree};}
        std::sort(events.begin(),events.begin()+count*2,[](const auto& a,const auto& b){if(a.beat!=b.beat)return a.beat<b.beat;if(a.on!=b.on)return !a.on;return a.note<b.note;});
    }
};
struct GeneratorDocument {GeneratorSettings settings;Pattern pattern;};
// Independent reproducible streams: no standard-library distribution/version dependence.
struct PatternRng {
    uint32_t state;
    uint32_t next()noexcept{state+=0x9e3779b9u;auto z=state;z=(z^(z>>16))*0x21f0aaadu;z=(z^(z>>15))*0x735a2d97u;return z^(z>>15);}
    int pick(int n)noexcept{return int(next()%uint32_t(std::max(1,n)));}
};
inline constexpr int scales[7][7]={{0,2,4,5,7,9,11},{0,2,3,5,7,8,10},{0,2,3,5,7,9,10},{0,2,4,5,7,9,10},{0,3,5,7,10,0,0},{0,2,3,5,7,8,11},{0,1,3,5,7,8,10}};
inline int scalePitch(const GeneratorSettings& c,int degree)noexcept{const int size=c.scale==4?5:7;int pitch=(c.octave+1)*12+c.root+12*(degree/size)+scales[c.scale][degree%size];while(pitch>127)pitch-=12;return std::max(0,pitch);}
inline GeneratorSettings randomSettings(GeneratorSettings c)noexcept{
    PatternRng r{c.seed^0xf5e9a321u};c.style=r.pick(14);c.complexity=20+r.pick(70);c.density=25+r.pick(65);c.variation=r.pick(101);c.humanize=r.pick(41);c.swing=r.pick(51);
    if(!c.lockKey){c.root=r.pick(12);c.scale=r.pick(7);}return c;
}
inline Pattern generatePattern(const GeneratorSettings& c,const Pattern* previous=nullptr,int slices=4,int base=60) {
    Pattern out;out.bars=c.bars;out.mode=c.mode;if(!c.valid())return out;
    PatternRng rhythm{c.seed^0x581a7b29u},melody{c.seed^0xc23791b5u},feel{c.seed^0x912ba03du};
    constexpr int progressions[8][4]={{0,4,5,3},{0,5,3,4},{0,3,4,0},{1,4,0,5},{0,2,5,3},{0,6,5,6},{0,3,0,4},{0,5,1,4}};
    constexpr int styleProgression[14]={0,5,1,3,7,5,4,7,1,5,6,2,6,4};
    constexpr int spacing[14]={2,2,2,2,2,1,2,2,8,4,1,4,2,4};
    constexpr int gate[14]={75,55,65,85,65,35,70,70,95,90,45,95,65,90};
    const bool old=previous && previous->count>0 && previous->valid() && previous->mode==c.mode;
    for(int b=0;b<c.bars;++b){int degree=progressions[styleProgression[c.style]][b%4];if(b>=4 && melody.pick(100)<c.variation)degree=melody.pick(7);if(c.mode==1 && b>0 && melody.pick(100)<c.variation/3)degree=(degree+1)%7;out.chords[size_t(b)]=old && c.lockMelody?previous->chords[size_t(b%previous->bars)]:degree;}
    auto append=[&](double beat,double duration,int degree,int velocity){if(out.count>=maxPatternNotes)return;auto& n=out.notes[size_t(out.count++)];n={beat,std::min(duration,out.beats()-beat),c.mode==3?std::clamp(base+degree,0,127):scalePitch(c,degree),velocity,degree};};
    if(c.lockRhythm && old){
        const double ratio=out.beats()/previous->beats();
        for(int i=0;i<previous->count;++i){const auto& n=previous->notes[size_t(i)];int degree=c.lockMelody?n.degree:out.chords[size_t(std::min(c.bars-1,int(n.beat*ratio/4)))]+2*melody.pick(c.complexity>60?4:3);if(c.mode==3)degree=(c.lockMelody?n.degree:melody.pick(std::clamp(slices,1,32)))%std::clamp(slices,1,32);append(n.beat*ratio,n.duration*ratio,degree,n.velocity);}
    }else{
        const int unit=c.mode==1?16:(c.complexity>70?1:spacing[c.style]);
        int ordinal=0;
        for(int tick=0;tick<c.bars*16;tick+=unit){
            if(c.mode!=1 && tick%16!=0 && rhythm.pick(100)>=c.density)continue;
            double beat=tick*.25;
            if(c.mode!=1){if(tick%2)beat+=c.swing/100.0*.115;beat+=(feel.pick(2001)/1000.0-1)*c.humanize/100.0*.035;beat=std::clamp(beat,0.0,out.beats()-.01);}
            const double duration=c.mode==1?3.8:std::max(.05,unit*.25*gate[c.style]/100.0);
            const int chord=out.chords[size_t(tick/16)],voices=c.mode==1?(c.complexity>60?4:3):1;
            for(int voice=0;voice<voices;++voice){
                int degree=chord+(c.mode==1?voice*2:(c.mode==2?(tick/unit)%4*2:2*melody.pick(c.complexity>60?4:3)));
                if(c.mode==0 && melody.pick(100)<c.complexity)degree=chord+melody.pick(c.scale==4?5:7);
                if((c.mode==0 || c.mode==2) && melody.pick(100)<c.variation)degree=std::clamp(degree+melody.pick(3)-1,0,20);
                if(c.mode==3)degree=(ordinal>0 && melody.pick(100)>=c.variation)?out.notes[size_t(out.count-1)].degree:melody.pick(std::clamp(slices,1,32));
                if(old && c.lockMelody)degree=previous->notes[size_t(ordinal%previous->count)].degree;
                if(c.mode==3)degree%=std::clamp(slices,1,32);
                const int velocity=std::clamp(92+(tick%4==0?12:0)+(feel.pick(31)-15)*c.humanize/100,1,127);
                append(beat,duration,degree,velocity);++ordinal;
            }
        }
    }
    std::stable_sort(out.notes.begin(),out.notes.begin()+out.count,[](auto& a,auto& b){return a.beat<b.beat;});
    // Keep a small gap before the next onset, avoiding same-key note-off collisions.
    for(int i=0;i<out.count;++i){auto& n=out.notes[size_t(i)];for(int j=i+1;j<out.count;++j)if(out.notes[size_t(j)].beat>n.beat+1e-7){n.duration=std::min(n.duration,(out.notes[size_t(j)].beat-n.beat)*.98);break;}}
    out.prepare();return out;
}
}
