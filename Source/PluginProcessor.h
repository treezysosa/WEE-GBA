#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/LayerRack.h"
#include "sampling/SampleLoader.h"
#include "vocal/ChopSequencer.h"
class WeeProcessor final : public juce::AudioProcessor, private juce::MidiKeyboardStateListener, private juce::AudioProcessorValueTreeState::Listener {
public:
    WeeProcessor();
    ~WeeProcessor() override;
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
    double getTailLengthSeconds() const override { return 6.25; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "INIT"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    bool loadPreset(const juce::File&);
    bool savePreset(const juce::File&);
    void loadSample(const juce::File&, size_t layer = 0, bool vocal = false);
    void activateVocalLayer(size_t layer);
    void detectChops();
    bool setSliceMarkers(const wee::SliceMap&);
    wee::SliceMap sliceMarkers() const { return loader.slices(); }
    void previewSlice(int slice,size_t layer);
    void stopPreview();
    void stopSequencer();
    void exportVocal(const juce::File&,int slice=-1);
    juce::String exportStatus()const{return loader.exportStatus();}
    bool exporting()const{return loader.exporting.load();}
    juce::File lastExport()const{return loader.lastExport();}
    wee::VocalConfig vocalSettings()const noexcept;
    static juce::String parameterId(size_t layer, size_t param);
    void clearSample();
    juce::String sampleStatus() const { return loader.status(); }
    std::vector<float> sampleWaveform() const { return loader.waveform(); }
    bool sampleLoading() const { return loader.busy(); }
    uint64_t sampleRevision() const { return loader.completed.load(); }
    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboard;
    std::atomic<float> peak {0}, cpu {0};
    std::atomic<double> bpm {120};
    std::atomic<int> voiceCount {0};
    std::array<std::atomic<int>, wee::layerCount> layerVoices {};
private:
    void handleNoteOn(juce::MidiKeyboardState*, int, int note, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int, int note, float) override;
    struct KeyEvent { int note=0; float velocity=0; int kind=0; size_t layer=0; };
    void enqueueKey(const KeyEvent&);
    void parameterChanged(const juce::String&,float) override;
    std::array<KeyEvent, 256> keyEvents {};
    juce::AbstractFifo keyQueue {256};
    std::atomic<bool> panicRequested {false};
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    bool applyPatch(juce::ValueTree, const juce::File& base = {});
    juce::ValueTree patchState();
    juce::CriticalSection contentStateMutex; // Never acquired by processBlock.
    wee::LayerRack rack;
    wee::SampleLoader loader;
    wee::ChopSequencer sequencer;
    std::array<std::atomic<float>*,wee::vocalParamCount> vocalValues {};
    std::array<std::array<std::atomic<float>*, wee::paramCount>, wee::layerCount> values;
    std::atomic<float>* masterGain = nullptr;
    double rate = 44100;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WeeProcessor)
};
