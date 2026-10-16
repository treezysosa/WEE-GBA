#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/Engine.h"
#include "sampling/SampleLoader.h"
class WeeProcessor final : public juce::AudioProcessor, private juce::MidiKeyboardStateListener {
public:
    WeeProcessor();
    ~WeeProcessor() override { keyboard.removeListener(this); }
    void panic() noexcept { panicRequested.store(true); }
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "WEE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "INIT"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    bool loadPreset(const juce::File&);
    bool savePreset(const juce::File&);
    void loadSample(const juce::File&);
    void clearSample();
    juce::String sampleStatus() const { return loader.status(); }
    std::vector<float> sampleWaveform() const { return loader.waveform(); }
    bool sampleLoading() const { return loader.loading.load(); }
    uint64_t sampleRevision() const { return loader.completed.load(); }
    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboard;
    std::atomic<float> peak {0}, cpu {0};
    std::atomic<double> bpm {120};
    std::atomic<int> voiceCount {0};
private:
    void handleNoteOn(juce::MidiKeyboardState*, int, int note, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int, int note, float) override;
    struct KeyEvent { int note; float velocity; };
    std::array<KeyEvent, 256> keyEvents {};
    juce::AbstractFifo keyQueue {256};
    std::atomic<bool> panicRequested {false};
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    bool applyPatch(juce::ValueTree, const juce::File& base = {});
    juce::ValueTree patchState();
    juce::CriticalSection contentStateMutex; // Never acquired by processBlock.
    wee::Engine engine;
    wee::Sampler sampler;
    wee::SampleLoader loader;
    int previousSource = 0;
    std::array<std::atomic<float>*, 14> values;
    double rate = 44100;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WeeProcessor)
};
