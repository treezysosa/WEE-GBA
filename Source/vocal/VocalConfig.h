#pragma once
#include "dsp/LayerConfig.h"
namespace wee {
enum VocalParam:size_t { VPitch,VStretch,VFormant,VPreserve,VSourceBpm,VSync,VChopMap,VBase,VReverse,VSensitivity,VSeqEnabled,VSeqDivision,VSeqGate,VSeqVelocity,VSeqLength,VStep1,vocalParamCount=VStep1+16 };
inline constexpr std::array<ParamSpec,vocalParamCount> vocalSpecs {{
    {"vocalPitch","Vocal pitch (semitones)",-12,12,0,ParamType::continuous},
    {"vocalStretch","Vocal duration multiplier",.5f,2,1,ParamType::continuous},
    {"vocalFormant","Formant shift (semitones)",-6,6,0,ParamType::continuous},
    {"vocalPreserve","Compensate fixed-pitch formants",0,1,0,ParamType::boolean},
    {"vocalSourceBpm","Recording BPM",40,240,120,ParamType::continuous},
    {"vocalSync","Stretch to host BPM",0,1,0,ParamType::boolean},
    {"vocalChopMap","Map slices to MIDI keys",0,1,1,ParamType::boolean},
    {"vocalBase","First chop MIDI key",0,96,60,ParamType::integer},
    {"vocalReverse","Reverse vocal playback",0,1,0,ParamType::boolean},
    {"sliceSensitivity","Detection sensitivity (then DETECT)",0,1,.5f,ParamType::continuous},
    {"chopSequence","Run chop sequencer",0,1,0,ParamType::boolean},
    {"chopDivision","Chop step duration",0,3,0,ParamType::choice,"1/16 note|1/8 note|1/4 note|1/32 note"},
    {"chopGate","Chop gate fraction",.05f,1,.5f,ParamType::continuous},
    {"chopVelocity","Chop velocity",.05f,1,.8f,ParamType::continuous},
    {"chopLength","Sequence step count",1,16,16,ParamType::integer},
    {"chopStep1","Step 01 (0 rest, 1–32 chop)",0,32,1,ParamType::integer},
    {"chopStep2","Step 02 (0 rest, 1–32 chop)",0,32,0,ParamType::integer},
    {"chopStep3","Step 03 (0 rest, 1–32 chop)",0,32,2,ParamType::integer},
    {"chopStep4","Step 04 (0 rest, 1–32 chop)",0,32,0,ParamType::integer},
    {"chopStep5","Step 05 (0 rest, 1–32 chop)",0,32,3,ParamType::integer},
    {"chopStep6","Step 06 (0 rest, 1–32 chop)",0,32,0,ParamType::integer},
    {"chopStep7","Step 07 (0 rest, 1–32 chop)",0,32,4,ParamType::integer},
    {"chopStep8","Step 08 (0 rest, 1–32 chop)",0,32,0,ParamType::integer},
    {"chopStep9","Step 09 (0 rest, 1–32 chop)",0,32,1,ParamType::integer},
    {"chopStep10","Step 10 (0 rest, 1–32 chop)",0,32,0,ParamType::integer},
    {"chopStep11","Step 11 (0 rest, 1–32 chop)",0,32,3,ParamType::integer},
    {"chopStep12","Step 12 (0 rest, 1–32 chop)",0,32,0,ParamType::integer},
    {"chopStep13","Step 13 (0 rest, 1–32 chop)",0,32,2,ParamType::integer},
    {"chopStep14","Step 14 (0 rest, 1–32 chop)",0,32,0,ParamType::integer},
    {"chopStep15","Step 15 (0 rest, 1–32 chop)",0,32,4,ParamType::integer},
    {"chopStep16","Step 16 (0 rest, 1–32 chop)",0,32,0,ParamType::integer}
}};
struct VocalConfig {
    std::array<float,vocalParamCount> v {};
    VocalConfig() noexcept {for(size_t i=0;i<v.size();++i)v[i]=vocalSpecs[i].initial;}
    double stretch(double bpm) const noexcept {return std::clamp(double(v[VStretch])*(v[VSync]>=.5f ? v[VSourceBpm]/std::clamp(bpm,20.0,400.0) : 1),.25,4.0);}
};
}
