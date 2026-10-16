#include "dsp/Sampler.h"
#include "sampling/SampleExchange.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <new>
static thread_local bool audioThread = false;
static std::atomic<int> audioAllocations {0}, audioDestructions {0};
void* operator new(std::size_t n) {
    if (audioThread) ++audioAllocations;
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void check(bool ok, const char* why) { if (!ok) { std::cerr << why << '\n'; std::exit(1); } }
double energy(const std::vector<float>& data) { double n=0; for (auto x:data) { check(std::isfinite(x) && std::abs(x)<=1,"finite bounded output"); n+=x*x; } return n; }
struct TrackedAsset { int id; std::vector<int> data; explicit TrackedAsset(int n):id(n),data(64,n){} ~TrackedAsset(){ if(audioThread) ++audioDestructions; } };
int main() {
    wee::SampleData asset; asset.rate=48000; asset.left.resize(4800); asset.right.resize(4800);
    for(size_t i=0;i<asset.left.size();++i) { asset.left[i]=float(std::sin(double(i)*6.28318530718*400/48000)); asset.right[i]=asset.left[i]*0.25f; }
    wee::Parameters p; p.attack=0.001f; p.sustain=1; p.cutoff=20000; p.gain=0.5f;
    for(double rate : {44100.0,48000.0,96000.0}) for(int block : {32,64,128,512,1024}) {
        wee::Sampler s; s.prepare(rate); s.setSample(&asset); wee::SampleSettings settings; settings.loop=true;
        std::vector<float> l(size_t(block),0),r(size_t(block),0);
        s.noteOn(60,1,settings);
        double totalL=0,totalR=0;
        audioThread=true;
        for(int b=0;b<200;++b) { std::fill(l.begin(),l.end(),0.0f); std::fill(r.begin(),r.end(),0.0f); s.render(l.data(),r.data(),block,p); totalL+=energy(l); totalR+=energy(r); }
        audioThread=false;
        check(totalL>totalR*4 && totalR>0.001,"independent stereo channels"); check(s.activeVoices()==1,"loop stays active");
        s.sustainPedal(true); s.noteOff(60); s.render(l.data(),nullptr,block,p); check(s.activeVoices()==1,"sustain holds sample"); s.sustainPedal(false);
        for(int b=0;b<int(rate/block)+2;++b) { std::fill(l.begin(),l.end(),0.0f); s.render(l.data(),nullptr,block,p); }
        check(s.activeVoices()==0,"sample release ends");
        settings.low=50; settings.high=70; s.noteOn(40,1,settings); check(s.activeVoices()==0,"key range rejects");
        settings.start=0.9f; settings.end=0.1f; s.noteOn(60,1,settings); check(s.activeVoices()==0,"invalid trim rejects");
        settings.start=0; settings.end=1; settings.low=0; settings.high=127;
        for(int i=0;i<100;++i) s.noteOn(20+i%88,1,settings);
        check(s.activeVoices()==32,"sample voice stealing"); s.allNotesOff(true); check(s.activeVoices()==0,"sample panic");
        settings.loop=false; s.noteOn(60,1,settings);
        for(int b=0;b<int(rate/block)+2;++b) { std::fill(l.begin(),l.end(),0.0f); s.render(l.data(),nullptr,block,p); }
        check(s.activeVoices()==0,"one shot terminates");
    }
    // Root note/rate/pitch relationships, including trim shortening and bend.
    for (int note : {60,72}) {
        wee::Sampler s; s.prepare(48000); s.setSample(&asset); wee::SampleSettings settings; settings.loop=true;
        std::vector<float> data(48000,0); s.noteOn(note,1,settings); s.render(data.data(),nullptr,48000,p);
        int count=0; for(size_t i=2401;i<data.size();++i) if(data[i-1]<=0 && data[i]>0) ++count;
        check(std::abs(count-(note==60?380:760))<=1,"sample root/octave pitch");
    }
    {
        wee::SampleData trim; trim.rate=48000; trim.left.resize(4800,0.8f); trim.right=trim.left;
        for(size_t i=0;i<2400;++i) trim.left[i]=trim.right[i]=0;
        wee::Sampler s; s.prepare(48000); s.setSample(&trim); wee::SampleSettings settings; settings.end=0.5f;
        std::vector<float> data(9600,0); s.noteOn(60,1,settings); s.render(data.data(),nullptr,int(data.size()),p);
        check(energy(data)==0,"trim excludes second half");
        settings.start=0.5f; settings.end=1; s.noteOn(60,1,settings); s.render(data.data(),nullptr,int(data.size()),p); check(energy(data)>1,"trim starts in second half");
        s.setSample(nullptr); std::fill(data.begin(),data.end(),0.0f); s.noteOn(60,1,settings); s.render(data.data(),nullptr,int(data.size()),p); check(energy(data)==0,"missing asset silence");
    }
    {
        wee::SampleExchange<TrackedAsset> exchange;
        std::atomic<bool> done{false};
        std::thread producer([&] { for(int i=1;i<=20000;++i) exchange.publish(std::make_unique<TrackedAsset>(i)); done.store(true); });
        int last=0;
        audioThread=true;
        while(!done.load()) {
            if(exchange.adopt()) { auto* a=exchange.current(); check(a->id>=last,"monotonic publication"); last=a->id; for(auto x:a->data) check(x==a->id,"asset lifetime under replacement"); }
        }
        audioThread=false; producer.join(); exchange.collect(); exchange.adopt();
        check(exchange.current() && exchange.current()->id==20000,"latest request wins");
    }
    check(audioAllocations.load()==0,"no audio-side allocations"); check(audioDestructions.load()==0,"no audio-side asset destruction");
    std::cout<<"Sampler tests passed: rates/buffers, stereo, pitch, trim, loop, sustain, stealing, silence, 20000 concurrent publications, allocation/destruction guards.\n";
}
