#include "PluginEditor.h"
WeeEditor::WeeEditor(WeeProcessor& p) : AudioProcessorEditor(p), processor(p), keys(p.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard) {
    theme.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff9bdcad));
    theme.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe2ddf2));
    theme.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff423657));
    setLookAndFeel(&theme);
    setResizable(true, true); setResizeLimits(780, 470, 1560, 940); setSize(1000, 600);
    const char* ids[] = {"gain", "attack", "decay", "sustain", "release", "cutoff"};
    const char* names[] = {"OUTPUT", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "TONE / Hz"};
    for (size_t i = 0; i < knobs.size(); ++i) {
        auto& k = knobs[i]; addAndMakeVisible(k);
        k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 22);
        k.setName(names[i]); k.setNumDecimalPlacesToDisplay(i == 5 ? 0 : 3);
        labels[i].setText(names[i], juce::dontSendNotification); labels[i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(labels[i]);
        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, ids[i], k);
    }
    oscillator.addItemList({"SINE", "SAW", "PULSE"}, 1); addAndMakeVisible(oscillator);
    waveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "waveform", oscillator);
    for (auto* b : {&load, &save, &panic, &home}) addAndMakeVisible(b);
    load.onClick = [this] { choosePreset(false); }; save.onClick = [this] { choosePreset(true); };
    panic.onClick = [this] { processor.panic(); };
    home.onClick = [this] { for (auto* param : processor.getParameters()) { param->beginChangeGesture(); param->setValueNotifyingHost(param->getDefaultValue()); param->endChangeGesture(); } };
    addAndMakeVisible(keys); keys.setAvailableRange(24, 108); keys.setLowestVisibleKey(48);
    keys.setWantsKeyboardFocus(true);
    addAndMakeVisible(status); status.setJustificationType(juce::Justification::centred);
    startTimerHz(20);
}
void WeeEditor::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff17141e));
    g.setColour(juce::Colour(0xff66567f)); g.fillRoundedRectangle(r.reduced(8), 58);
    g.setColour(juce::Colour(0xff242a2c)); g.fillRoundedRectangle(r.reduced(80, 65), 22);
    g.setColour(juce::Colour(0xffb9e3b4)); g.setFont(float(getHeight()) * 0.05f);
    g.drawText("W E E   /   POCKET SYNTH", 40, 20, getWidth() - 80, 36, juce::Justification::centred);
    g.setColour(juce::Colours::lightgreen); g.fillEllipse(30, 35, 8, 8);
    g.setFont(12); g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.drawText("PHASE 1   /   32 VOICES   /   ORIGINAL SOUND ENGINE", 30, getHeight()-35, getWidth()-60, 22, juce::Justification::centred);
}
void WeeEditor::resized() {
    const auto w = getWidth(), h = getHeight();
    load.setBounds(95, 76, 120, 28); save.setBounds(225, 76, 120, 28);
    oscillator.setBounds(w-270, 76, 170, 28);
    status.setBounds(95, 112, w-190, 32);
    const int cell = (w-210)/6;
    for (int i = 0; i < 6; ++i) { labels[size_t(i)].setBounds(105+i*cell, 158, cell, 22); knobs[size_t(i)].setBounds(105+i*cell, 182, cell, h-365); }
    keys.setBounds(105, h-174, w-210, 96);
    home.setBounds(16, h/2-30, 80, 40); panic.setBounds(w-96, h/2-30, 80, 40);
}
void WeeEditor::timerCallback() {
    status.setText("BPM " + juce::String(processor.bpm.load(), 1) + "    VOICES " + juce::String(processor.voiceCount.load()) +
        "    CPU " + juce::String(processor.cpu.load(), 1) + "%    PEAK " + juce::String(juce::Decibels::gainToDecibels(processor.peak.load(), -100.0f), 1) + " dB", juce::dontSendNotification);
}
void WeeEditor::choosePreset(bool saving) {
    chooser = std::make_unique<juce::FileChooser>(saving ? "Save WEE patch" : "Load WEE patch", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.wee");
    juce::Component::SafePointer<WeeEditor> safe(this);
    chooser->launchAsync((saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles,
        [safe, saving](const juce::FileChooser& fc) {
            if (!safe || fc.getResult() == juce::File()) return;
            auto f = saving ? fc.getResult().withFileExtension("wee") : fc.getResult();
            const bool ok = saving ? safe->processor.savePreset(f) : safe->processor.loadPreset(f);
            if (!ok) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "WEE", "Patch operation failed. Check the file, schema version, and permissions.");
        });
}
