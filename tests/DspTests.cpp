#include "dsp/Engine.h"
#include <iostream>
#include <cstdlib>
#include <vector>
#include <limits>
void check(bool ok, const char* why) { if (!ok) { std::cerr << why << '\n'; std::exit(1); } }
int main() {
    for (double rate : {44100.0, 48000.0, 96000.0}) for (int block : {32, 64, 128, 512, 1024}) {
        wee::Engine e; e.prepare(rate); wee::Parameters p;
        std::vector<float> l(size_t(block), 0), r(size_t(block), 0);
        e.render(l.data(), r.data(), block, p);
        check(l == std::vector<float>(size_t(block), 0), "idle silence");
        for (int wave = 0; wave < 3; ++wave) {
            e.reset(); p.waveform = wave; e.noteOn(69, 1); double energy = 0;
            for (int b = 0; b < 100; ++b) {
                std::fill(l.begin(), l.end(), 0); std::fill(r.begin(), r.end(), 0);
                e.render(l.data(), r.data(), block, p);
                for (size_t i = 0; i < l.size(); ++i) { check(std::isfinite(l[i]) && std::abs(l[i]) <= 1, "finite bounded output"); check(l[i] == r[i], "stereo equality"); energy += l[i]*l[i]; }
            }
            check(energy > 0.01, "audible oscillator");
            e.sustainPedal(true); e.noteOff(69);
            e.render(l.data(), r.data(), block, p); check(e.activeVoices() == 1, "sustain holds");
            e.sustainPedal(false);
            for (int b = 0; b < int(rate/block)+2; ++b) { std::fill(l.begin(), l.end(), 0); e.render(l.data(), nullptr, block, p); }
            check(e.activeVoices() == 0, "release terminates");
        }
        for (int i = 0; i < 100; ++i) e.noteOn(20+i%88, 1);
        check(e.activeVoices() == 32, "bounded stealing"); e.allNotesOff(true); check(e.activeVoices() == 0, "panic");
    }
    wee::Engine e; e.prepare(48000); wee::Parameters p; p.attack=0.001f; p.sustain=1; p.waveform=0; p.cutoff=20000;
    std::vector<float> data(48000, 0); e.noteOn(69,1); e.render(data.data(),nullptr,48000,p);
    int crossings=0; for (size_t i=2401;i<data.size();++i) if(data[i-1]<=0 && data[i]>0) ++crossings;
    check(std::abs(crossings-418)<=1,"A4 pitch accuracy");
    e.reset(); e.pitchWheel(16383); e.noteOn(69,1); std::fill(data.begin(),data.end(),0); e.render(data.data(),nullptr,48000,p);
    crossings=0; for(size_t i=2401;i<data.size();++i) if(data[i-1]<=0 && data[i]>0) ++crossings;
    check(std::abs(crossings-469)<=1,"two semitone pitch bend");
    std::cout << "DSP tests passed: 45 buffer/rate/wave combinations, sustain, release, stealing, panic, pitch.\n";
}
