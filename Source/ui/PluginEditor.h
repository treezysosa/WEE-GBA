#pragma once
#include "PluginProcessor.h"
class WeeEditor final : public juce::AudioProcessorEditor, private juce::Timer, public juce::FileDragAndDropTarget {
public:
    explicit WeeEditor(WeeProcessor&);
    ~WeeEditor() override { stopTimer(); setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
private:
    void timerCallback() override;
    void choosePreset(bool save);
    void chooseSample();
    WeeProcessor& processor;
    juce::LookAndFeel_V4 theme;
    juce::MidiKeyboardComponent keys;
    juce::ComboBox oscillator, source;
    std::array<juce::Slider, 11> knobs;
    std::array<juce::Label, 11> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 11> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttachment, sourceAttachment;
    juce::ToggleButton loop {"LOOP TRIMMED REGION"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopAttachment;
    juce::TextButton load {"LOAD PATCH"}, save {"SAVE PATCH"}, panic {"B / PANIC"}, home {"A / INIT"};
    juce::TextButton sampleLoad {"LOAD SAMPLE / RELOCATE"}, sampleClear {"CLEAR SAMPLE"};
    juce::Label status, sampleInfo, hint;
    std::vector<float> waveform;
    uint64_t revision = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WeeEditor)
};
