#include "dsp/LayerRack.h"
#include <iostream>
#include <memory>
#include <vector>
#include <cstdlib>
#include <new>
static thread_local bool inAudio=false;
static int allocations=0;
void* operator new(std::size_t n){if(inAudio)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void check(bool condition,const char* message){if(!condition){std::cerr<<message<<'\n';std::exit(1);}}
double energy(const std::vector<float>& data){double e=0;for(float x:data){check(std::isfinite(x) && std::abs(x)<=1,"finite bounded hybrid output");e+=x*x;}return e;}
std::array<wee::LayerConfig,wee::layerCount> configs(){std::array<wee::LayerConfig,wee::layerCount> c;for(size_t i=0;i<c.size();++i)c[i]=wee::LayerConfig(i);return c;}
void render(wee::LayerRack& rack,std::vector<float>& l,std::vector<float>& r,int block){
    std::fill(l.begin(),l.end(),0.0f);std::fill(r.begin(),r.end(),0.0f);
    inAudio=true;for(int offset=0;offset<int(l.size());offset+=block)rack.render(l.data()+offset,r.data()+offset,std::min(block,int(l.size())-offset));inAudio=false;
}
int main(){
    auto rack=std::make_unique<wee::LayerRack>();
    wee::SampleData sample;sample.rate=48000;sample.left.resize(48000);sample.right.resize(48000);
    for(size_t i=0;i<sample.left.size();++i){sample.left[i]=float(std::sin(double(i)*6.283185307179586*(i<24000?220:660)/48000))*.7f;sample.right[i]=sample.left[i]*.25f;}
    for(double rate:{44100.0,48000.0,96000.0})for(int block:{32,64,128,512,1024})for(int mode=0;mode<5;++mode){
        rack->prepare(rate);rack->setSample(&sample);auto c=configs();
        c[0].v[wee::Gain]=.6f;c[0].v[wee::Attack]=.001f;c[0].v[wee::Sustain]=1;
        c[0].v[wee::Source]=mode==1?1.0f:0.0f;c[0].v[wee::SynthMode]=mode<2?0.0f:float(mode-1);
        c[0].v[wee::Loop]=1;c[0].v[wee::WTPosition]=.65f;
        rack->configure(c,1,120);
        std::vector<float> l(size_t(std::max(block*20,int(rate*.25))),0),r(l.size(),0);
        inAudio=true;rack->noteOn(60,1);inAudio=false;render(*rack,l,r,block);
        check(energy(l)>.001,"each engine produces audio");check(rack->activeVoices()==1,"one voice in selected engine");
        rack->sustainPedal(true);rack->noteOff(60);render(*rack,l,r,block);check(rack->activeVoices()==1,"hybrid sustain");rack->sustainPedal(false);
        for(int b=0;b<6;++b)render(*rack,l,r,block);
        check(rack->activeVoices()==0,"hybrid release terminates");
        rack->allNotesOff(true);render(*rack,l,r,block);check(energy(l)==0,"panic silence");
    }
    // Wavetable mip limiting, morphing, and basic tuning for WT/FM.
    auto tables=std::make_unique<wee::WavetableBank>();tables->prepare();
    const float bandlimited=tables->lookup(.13,1,.2),expected=float(4/3.14159265358979323846*std::sin(.13*6.283185307179586));
    check(std::abs(bandlimited-expected)<.0001f,"high-note wavetable excludes unsafe harmonics");
    check(std::abs(tables->lookup(.17,0,.001)-tables->lookup(.17,1,.001))>.05f,"wavetable frames differ");
    for(int mode:{1,2}){
        rack->prepare(48000);auto c=configs();c[0].v[wee::SynthMode]=float(mode);c[0].v[wee::FMIndex]=0;c[0].v[wee::Sustain]=1;c[0].v[wee::Attack]=.001f;c[0].v[wee::Cutoff]=20000;
        rack->configure(c,1,120);rack->noteOn(69,1);std::vector<float> l(48000,0),r(48000,0);render(*rack,l,r,128);
        int crossings=0;for(size_t i=2401;i<l.size();++i)if(l[i-1]<=0 && l[i]>0)++crossings;
        check(std::abs(crossings-418)<=1,"WT/FM carrier A4 tuning");
    }
    {
        std::vector<float> reference(48000,0),r(48000,0),changed(48000,0);auto c=configs();c[0].v[wee::SynthMode]=2;c[0].v[wee::FMIndex]=0;
        rack->prepare(48000);rack->configure(c,1,120);rack->noteOn(60,1);render(*rack,reference,r,128);
        c[0].v[wee::FMIndex]=4;c[0].v[wee::FMRatio]=1.5f;rack->prepare(48000);rack->configure(c,1,120);rack->noteOn(60,1);render(*rack,changed,r,128);
        double difference=0;for(size_t i=0;i<changed.size();++i)difference+=std::abs(changed[i]-reference[i]);check(difference>10,"FM controls change spectrum");
    }
    // Eight independent voices/layers, stereo balance, routing and topology changes.
    rack->prepare(48000);auto c=configs();for(auto& l:c){l.v[wee::Enabled]=1;l.v[wee::SynthMode]=1;l.v[wee::Gain]=.1f;}
    rack->configure(c,1,120);inAudio=true;for(int i=0;i<40;++i)rack->noteOn(40+i,1);inAudio=false;
    check(rack->activeVoices()==256,"eight layers cap at 256 voices");
    std::vector<float> left(4800,0),right(4800,0);render(*rack,left,right,64);energy(left);
    c[7].v[wee::Enabled]=0;rack->configure(c,1,120);check(rack->activeVoices(7)==0 && rack->activeVoices(6)==32,"disable only one layer");
    rack->allNotesOff(true);c=configs();c[0].v[wee::SynthMode]=1;c[0].v[wee::Pan]=-1;rack->configure(c,1,120);rack->noteOn(60,1);render(*rack,left,right,128);check(energy(left)>.01 && energy(right)==0,"left balance");
    c[0].v[wee::Pan]=1;rack->configure(c,1,120);render(*rack,left,right,128);check(energy(right)>.01 && energy(left)==0,"right balance");
    c[0].v[wee::Pan]=0;c[0].v[wee::Route1Source]=3;c[0].v[wee::Route1Destination]=3;c[0].v[wee::Route1Depth]=-1;rack->configure(c,1,120);rack->modWheel(1);render(*rack,left,right,128);check(energy(left)==0,"wheel routes to amplitude");
    rack->modWheel(0);render(*rack,left,right,128);check(energy(left)>.01,"route returns to base");
    c[0].v[wee::Route1Source]=4;rack->configure(c,1,120);rack->aftertouch(1);render(*rack,left,right,128);check(energy(left)==0,"pressure routes to amplitude");
    c[0].v[wee::SynthMode]=2;rack->configure(c,1,120);check(rack->activeVoices()==0,"engine change stops old voices");
    // A changed transpose must not prevent release of the original MIDI note.
    c=configs();c[0].v[wee::Transpose]=12;rack->configure(c,1,120);rack->noteOn(60,1);c[0].v[wee::Transpose]=-12;rack->configure(c,1,120);rack->noteOff(60);for(int i=0;i<10;++i)render(*rack,left,right,128);check(rack->activeVoices()==0,"transposed note releases by original key");
    // Granular playback is silent without an asset and deterministic from reset.
    c=configs();c[0].v[wee::SynthMode]=3;c[0].v[wee::GrainPosition]=.2f;c[0].v[wee::GrainSpray]=0;
    rack->prepare(48000);rack->setSample(nullptr);rack->configure(c,1,120);rack->noteOn(60,1);render(*rack,left,right,64);check(energy(left)==0,"granular missing asset silence");
    rack->setSample(&sample);rack->noteOn(60,1);render(*rack,left,right,64);check(energy(left)>energy(right)*4,"granular stereo preservation");
    auto first=left;rack->prepare(48000);rack->setSample(&sample);rack->configure(c,1,120);rack->noteOn(60,1);render(*rack,left,right,128);check(first==left,"granular deterministic across block sizes");
    // Tempo-synced LFO quarters at 120 BPM vs 240 BPM.
    c[0].v[wee::LFOSync]=1;c[0].v[wee::LFOBeats]=2;wee::LFO lfo;
    check(std::abs(lfo.advance(6000,48000,c[0],120)-.70710678f)<.0001f,"LFO tempo rate");
    check(std::abs(lfo.advance(6000,48000,c[0],120)-.70710678f)<.0001f,"LFO phase continuity");
    lfo.reset();check(std::abs(lfo.advance(6000,48000,c[0],240)-1)<.0001f,"LFO follows tempo");
    // SVF response and stability across the complete cutoff/resonance range.
    for(int type:{1,2,3})for(double rate:{44100.0,96000.0}){
        wee::StereoFilter filter;filter.configure(type,1000,.5f,rate);double total=0;
        for(int i=0;i<int(rate);++i){float x=filter.process(1,0);check(std::isfinite(x),"filter finite DC");if(i>1000)total+=std::abs(x);}
        check(type==1 ? total>rate*.5 : total<rate*.00001,"multimode DC response");
        for(int i=0;i<5000;++i){filter.configure(type,i%2?20:20000,.95f,rate);check(std::isfinite(filter.process(i%2?.5f:-.5f,0)),"filter stable under modulation");}
    }
    check(allocations==0,"no hybrid audio-side allocations");
    std::cout<<"Hybrid tests passed: 75 engine/rate/buffer combinations, tuning, mipmaps, FM, granular, 8 layers, routing, filters, sync and allocation guard.\n";
}
