#pragma once
#include "PluginProcessor.h"
#include "VocalWaveform.h"
#include "GeneratorPanel.h"
class WeeParameterControl final : public juce::Component {
public:
    WeeParameterControl(WeeProcessor&,size_t layer,size_t parameter,const juce::String& name = {});
    WeeParameterControl(WeeProcessor&,const juce::String& id,const juce::String& name);
    WeeParameterControl(WeeProcessor&,const wee::ParamSpec&);
    void resized() override;
    void refresh();
private:
    void initialise(const juce::String&,const juce::String&,wee::ParamType,const char* choices);
    WeeProcessor& processor;
    size_t layer=0,parameter=wee::paramCount;
    juce::Label label;
    juce::Slider slider;
    juce::ComboBox choices;
    juce::ToggleButton toggle {"ON"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> choiceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
};
class WeeEditor final : public juce::AudioProcessorEditor, private juce::Timer, public juce::FileDragAndDropTarget {
public:
    explicit WeeEditor(WeeProcessor&);
    ~WeeEditor() override { stopTimer(); viewport.setViewedComponent(nullptr,false); setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
private:
    void timerCallback() override;
    void choosePreset(bool save);
    void chooseSample();
    void chooseExport(bool slice);
    void updateSliceSelection();
    void rebuildPage();
    void arrangeControls();
    WeeProcessor& processor;
    juce::LookAndFeel_V4 theme;
    juce::MidiKeyboardComponent keys;
    juce::Viewport viewport;
    juce::Component panel;
    GeneratorPanel generatorPanel;
    std::vector<std::unique_ptr<WeeParameterControl>> controls;
    juce::ComboBox layerChoice,pageChoice;
    std::array<juce::ToggleButton,wee::layerCount> layerButtons;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>,wee::layerCount> layerAttachments;
    juce::TextButton load {"LOAD PATCH"},save {"SAVE PATCH"},panic {"B / PANIC"},home {"A / INIT"};
    juce::TextButton previous {"< LAYER"},next {"LAYER >"};
    juce::TextButton sampleLoad {"LOAD SAMPLE / RELOCATE"},sampleClear {"CLEAR SHARED SAMPLE"};
    juce::Label status,sampleInfo,hint;
    VocalWaveform sliceWave;
    juce::ComboBox sliceChoice;
    juce::Slider sliceStart,sliceEnd;
    juce::Label startCaption,endCaption;
    int displayedBase=-1;
    juce::TextButton useVocal {"USE VOCAL LAYER"},detect {"DETECT"},split {"SPLIT"},remove {"REMOVE"},preview {"PREVIEW"},stop {"STOP"},exportClip {"EXPORT CLIP"},exportChop {"EXPORT CHOP"};
    bool updatingSlices=false,draggingBoundary=false;
    std::vector<float> waveform;
    uint64_t revision=0;
    size_t selectedLayer=0;
    int page=1;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WeeEditor)
};
