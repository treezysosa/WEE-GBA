#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include "vocal/VocalAnalysis.h"
#include "vocal/VocalEngine.h"
#include "SampleExchange.h"
namespace wee {
class SampleLoader final : private juce::Thread {
public:
    SampleLoader():Thread("WEE sample and vocal worker"){startThread();}
    ~SampleLoader()override{signalThreadShouldExit();notify();stopThread(-1);}
    void request(const juce::String& path,const SliceDocument& document={},float sensitivity=.5f,bool force=true){
        const juce::ScopedLock lock(mutex);
        if(force || path!=requestedPath)sourceLoading.store(true);
        requestedPath=path;requestedDocument=document;requestedSensitivity=sensitivity;forceDecode=force;
        ++requestId;requestSignal.store(requestId);loading.store(true);message="Loading / analysing sample...";notify();
    }
    // May be called from an audio-thread parameter listener: atomics only.
    void setFormantShift(float shift)noexcept{shift=std::isfinite(shift)?std::clamp(shift,-18.0f,18.0f):0;if(targetShift.exchange(shift)!=shift)shiftRevision.fetch_add(1);}
    bool busy()const noexcept{return loading.load() || shiftRevision.load()!=handledShift.load();}
    juce::String status()const{const juce::ScopedLock lock(mutex);return busy()?juce::String("Preparing sample / spectral formants..."):message;}
    std::vector<float> waveform()const{const juce::ScopedLock lock(mutex);return peaks;}
    SliceMap slices()const{const juce::ScopedLock lock(mutex);return previewSlices;}
    void exportAudio(const juce::File& destination,const VocalConfig& settings,double bpm,int slice){
        const juce::ScopedLock lock(mutex);exportPath=destination;exportConfig=settings;exportBpm=bpm;exportSlice=slice;exportSource=requestId;
        ++exportId;exporting.store(true);exportMessage="Rendering processed WAV...";notify();
    }
    juce::String exportStatus()const{const juce::ScopedLock lock(mutex);return exportMessage;}
    juce::File lastExport()const{const juce::ScopedLock lock(mutex);return lastExportFile;}
    std::atomic<bool> loading{false},sourceLoading{false},exporting{false};
    std::atomic<uint64_t> completed{0};
    SampleExchange<SampleData> exchange;
private:
    mutable juce::CriticalSection mutex;
    juce::String requestedPath,message{"No sample loaded"},exportMessage;
    SliceDocument requestedDocument;float requestedSensitivity=.5f;bool forceDecode=true;
    std::vector<float> peaks;SliceMap previewSlices;
    uint64_t requestId=0,handledId=0,exportId=0,handledExport=0,exportSource=0;
    std::atomic<uint64_t> requestSignal{0},shiftRevision{0},handledShift{0};
    std::atomic<float> targetShift{0};
    juce::File exportPath,lastExportFile;VocalConfig exportConfig;double exportBpm=120;int exportSlice=-1;
    void run()override;
};
}
