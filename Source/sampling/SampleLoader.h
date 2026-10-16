#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include "dsp/Sampler.h"
#include "SampleExchange.h"
namespace wee {
class SampleLoader final : private juce::Thread {
public:
    SampleLoader() : Thread("WEE sample loader") { startThread(); }
    ~SampleLoader() override { signalThreadShouldExit(); notify(); stopThread(-1); }
    // These methods are never called by processBlock.
    void request(const juce::String& path) {
        const juce::ScopedLock lock(mutex);
        requestedPath = path; ++requestId; loading.store(true); message = "Loading sample...";
        notify();
    }
    juce::String status() const { const juce::ScopedLock lock(mutex); return message; }
    std::vector<float> waveform() const { const juce::ScopedLock lock(mutex); return peaks; }
    std::atomic<bool> loading {false};
    std::atomic<uint64_t> completed {0};
    SampleExchange<SampleData> exchange;
private:
    mutable juce::CriticalSection mutex;
    juce::String requestedPath, message {"No sample loaded"};
    std::vector<float> peaks;
    uint64_t requestId = 0, handledId = 0;
    void run() override;
};
}
