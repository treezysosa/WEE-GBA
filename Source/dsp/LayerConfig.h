#pragma once
#include "Sampler.h"
namespace wee {
constexpr size_t layerCount = 8;
enum Param : size_t {
    Gain, Attack, Decay, Sustain, Release, Cutoff, Waveform, Source, Root, Start, End, Loop, Low, High,
    Enabled, SynthMode, Pan, Transpose, WTPosition, OscMix, Detune, FMRatio, FMIndex,
    GrainPosition, GrainSize, GrainDensity, GrainSpray, FilterMode, Resonance,
    LFORate, LFOSync, LFOBeats, LFOShape,
    Route1Source, Route1Destination, Route1Depth, Route2Source, Route2Destination, Route2Depth,
    Route3Source, Route3Destination, Route3Depth, Route4Source, Route4Destination, Route4Depth, VocalMode, paramCount
};
enum class ParamType { continuous, integer, boolean, choice };
struct ParamSpec { const char* id; const char* name; float min, max, initial; ParamType type; const char* choices = ""; float skew = 1; };
inline constexpr std::array<ParamSpec, paramCount> specs {{
    {"gain","Layer level",0,1,.22f,ParamType::continuous},
    {"attack","Attack (s)",.001f,3,.01f,ParamType::continuous,"",.3f},
    {"decay","Decay (s)",.001f,3,.18f,ParamType::continuous,"",.3f},
    {"sustain","Sustain",0,1,.7f,ParamType::continuous},
    {"release","Release (s)",.001f,5,.35f,ParamType::continuous,"",.3f},
    {"cutoff","Cutoff (Hz)",20,20000,8000,ParamType::continuous,"",.25f},
    {"waveform","VA waveform",0,2,0,ParamType::choice,"Sine|Saw|Pulse"},
    {"source","Sound source",0,1,0,ParamType::choice,"Synth|Sample"},
    {"root","Sample root (MIDI)",0,127,60,ParamType::integer},
    {"sampleStart","Sample start (0..1)",0,1,0,ParamType::continuous},
    {"sampleEnd","Sample end (0..1)",0,1,1,ParamType::continuous},
    {"loop","Sample loop",0,1,0,ParamType::boolean},
    {"keyLow","Lowest MIDI key",0,127,0,ParamType::integer},
    {"keyHigh","Highest MIDI key",0,127,127,ParamType::integer},
    {"enabled","Layer enabled",0,1,1,ParamType::boolean},
    {"synthMode","Synth engine",0,3,0,ParamType::choice,"Virtual analog|Wavetable|FM (2 operators)|Granular (4 heads)"},
    {"pan","Stereo balance",-1,1,0,ParamType::continuous},
    {"transpose","Transpose (semitones)",-24,24,0,ParamType::integer},
    {"wtPosition","Wavetable position",0,1,0,ParamType::continuous},
    {"oscMix","WT oscillator 2 mix",0,1,0,ParamType::continuous},
    {"detune","WT osc 2 detune (cents)",-50,50,7,ParamType::continuous},
    {"fmRatio","FM modulator ratio",.25f,8,2,ParamType::continuous},
    {"fmIndex","FM index (radians)",0,8,1,ParamType::continuous},
    {"grainPosition","Grain position",0,1,.4f,ParamType::continuous},
    {"grainSize","Grain size (ms)",10,250,80,ParamType::continuous},
    {"grainDensity","Grains per second",1,40,12,ParamType::continuous},
    {"grainSpray","Grain position spread",0,1,.1f,ParamType::continuous},
    {"filterMode","Filter",0,3,0,ParamType::choice,"Legacy low-pass|Resonant low-pass|Resonant high-pass|Resonant band-pass"},
    {"resonance","Resonance",0,.95f,.1f,ParamType::continuous},
    {"lfoRate","LFO rate (Hz)",.05f,20,1,ParamType::continuous,"",.5f},
    {"lfoSync","LFO tempo sync",0,1,0,ParamType::boolean},
    {"lfoBeats","LFO cycle (beats)",0,6,2,ParamType::choice,"0.25|0.5|1|2|4|8|16"},
    {"lfoShape","LFO shape",0,2,0,ParamType::choice,"Sine|Triangle|Square"},
    {"mod1Source","Route 1 source",0,4,0,ParamType::choice,"Off|LFO|Last velocity|Mod wheel|Pressure"},
    {"mod1Destination","Route 1 destination",0,6,0,ParamType::choice,"Pitch|Cutoff|WT position|Amplitude|Pan|FM index|Grain position"},
    {"mod1Depth","Route 1 depth",-1,1,0,ParamType::continuous},
    {"mod2Source","Route 2 source",0,4,0,ParamType::choice,"Off|LFO|Last velocity|Mod wheel|Pressure"},
    {"mod2Destination","Route 2 destination",0,6,0,ParamType::choice,"Pitch|Cutoff|WT position|Amplitude|Pan|FM index|Grain position"},
    {"mod2Depth","Route 2 depth",-1,1,0,ParamType::continuous},
    {"mod3Source","Route 3 source",0,4,0,ParamType::choice,"Off|LFO|Last velocity|Mod wheel|Pressure"},
    {"mod3Destination","Route 3 destination",0,6,0,ParamType::choice,"Pitch|Cutoff|WT position|Amplitude|Pan|FM index|Grain position"},
    {"mod3Depth","Route 3 depth",-1,1,0,ParamType::continuous},
    {"mod4Source","Route 4 source",0,4,0,ParamType::choice,"Off|LFO|Last velocity|Mod wheel|Pressure"},
    {"mod4Destination","Route 4 destination",0,6,0,ParamType::choice,"Pitch|Cutoff|WT position|Amplitude|Pan|FM index|Grain position"},
    {"mod4Depth","Route 4 depth",-1,1,0,ParamType::continuous},
    {"vocalEnabled","Vocal chop / stretch playback",0,1,0,ParamType::boolean}
}};
struct LayerConfig {
    std::array<float, paramCount> v {};
    explicit LayerConfig(size_t layer=0) noexcept {
        for(size_t i=0;i<paramCount;++i) v[i]=specs[i].initial;
        v[Enabled]=layer==0 ? 1.0f : 0.0f;
    }
    Parameters envelope() const noexcept { return {v[Gain],v[Attack],v[Decay],v[Sustain],v[Release],v[Cutoff],int(v[Waveform])}; }
    SampleSettings mapping() const noexcept { return {int(v[Root]),int(v[Low]),int(v[High]),v[Start],v[End],v[Loop]>=.5f,int(v[Transpose])}; }
    int engine() const noexcept { return v[Source]>=.5f ? (v[VocalMode]>=.5f ? 5 : 1) : (int(v[SynthMode])==0 ? 0 : int(v[SynthMode])+1); }
};
}
