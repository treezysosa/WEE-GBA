#pragma once
#include "PluginProcessor.h"
class WeeEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit WeeEditor(WeeProcessor&);
    ~WeeEditor() override { stopTimer(); setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void choosePreset(bool save);
    WeeProcessor& processor;
    juce::LookAndFeel_V4 theme;
    juce::MidiKeyboardComponent keys;
    juce::ComboBox oscillator;
    std::array<juce::Slider, 6> knobs;
    std::array<juce::Label, 6> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 6> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttachment;
    juce::TextButton load {"LOAD PATCH"}, save {"SAVE PATCH"}, panic {"B / PANIC"}, home {"A / INIT"};
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WeeEditor)
};
