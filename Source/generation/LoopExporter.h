#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "dsp/LayerRack.h"
#include "vocal/VocalAnalysis.h"
#include "PatternPlayer.h"
#include "MidiExport.h"
namespace wee {
struct LoopExportJob {
    GeneratorDocument document;
    std::array<LayerConfig,layerCount> layers;
    VocalConfig vocal;
    SliceMap slices;
    juce::File destination;
    juce::String samplePath;
    float master=1;double bpm=120;bool midi=false;
};
class LoopExporter final:private juce::Thread {
    mutable juce::CriticalSection mutex;
    std::unique_ptr<LoopExportJob> pending;
    std::atomic<uint64_t> serial{0};juce::String message;
    void run()override;
public:
    std::atomic<bool> busy{false};
    LoopExporter():Thread("WEE loop export"){startThread();}
    ~LoopExporter()override{signalThreadShouldExit();notify();stopThread(-1);}
    void submit(std::unique_ptr<LoopExportJob> job){const juce::ScopedLock lock(mutex);pending=std::move(job);serial.fetch_add(1);busy.store(true);message="Exporting generated pattern...";notify();}
    juce::String status()const{const juce::ScopedLock lock(mutex);return message;}
};
}
