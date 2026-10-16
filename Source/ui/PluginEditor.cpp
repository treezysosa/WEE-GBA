#include "PluginEditor.h"
WeeEditor::WeeEditor(WeeProcessor& p) : AudioProcessorEditor(p), processor(p), keys(p.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard) {
    theme.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff9bdcad));
    theme.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe2ddf2));
    theme.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff423657));
    setLookAndFeel(&theme);
    setResizable(true, true); setResizeLimits(1000, 720, 1600, 1152); setSize(1000, 720);
    const char* ids[] = {"gain", "attack", "decay", "sustain", "release", "cutoff", "sampleStart", "sampleEnd", "root", "keyLow", "keyHigh"};
    const char* names[] = {"OUTPUT", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "TONE / Hz", "START 0..1", "END 0..1", "ROOT MIDI", "LOW KEY", "HIGH KEY"};
    for (size_t i = 0; i < knobs.size(); ++i) {
        auto& k = knobs[i]; addAndMakeVisible(k);
        k.setSliderStyle(i < 6 ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearHorizontal);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 22);
        k.setName(names[i]); k.setNumDecimalPlacesToDisplay(i == 5 || i >= 8 ? 0 : 3);
        k.setTooltip(i >= 6 ? "Sample mapping and trim are captured when a new note starts." : names[i]);
        labels[i].setText(names[i], juce::dontSendNotification); labels[i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(labels[i]);
        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, ids[i], k);
    }
    oscillator.addItemList({"SINE", "SAW", "PULSE"}, 1); addAndMakeVisible(oscillator); oscillator.setName("Oscillator waveform");
    source.addItemList({"OSCILLATOR", "SAMPLE"}, 1); addAndMakeVisible(source); source.setName("Sound source");
    waveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "waveform", oscillator);
    sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "source", source);
    addAndMakeVisible(loop); loopAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "loop", loop);
    for (auto* b : {&load, &save, &panic, &home, &sampleLoad, &sampleClear}) addAndMakeVisible(b);
    load.onClick = [this] { choosePreset(false); }; save.onClick = [this] { choosePreset(true); };
    sampleLoad.onClick = [this] { chooseSample(); }; sampleClear.onClick = [this] { processor.clearSample(); };
    panic.onClick = [this] { processor.panic(); };
    home.onClick = [this] {
        processor.clearSample();
        for (auto* param : processor.getParameters()) { param->beginChangeGesture(); param->setValueNotifyingHost(param->getDefaultValue()); param->endChangeGesture(); }
    };
    addAndMakeVisible(keys); keys.setAvailableRange(0, 127); keys.setLowestVisibleKey(48); keys.setWantsKeyboardFocus(true);
    for (auto* label : {&status, &sampleInfo, &hint}) { addAndMakeVisible(label); label->setJustificationType(juce::Justification::centred); }
    hint.setText("Drop WAV / AIFF / FLAC here. Play keys to audition. Trim and mapping apply to new notes.", juce::dontSendNotification);
    waveform = p.sampleWaveform(); revision = p.sampleRevision();
    startTimerHz(20);
}
void WeeEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff17141e));
    g.setColour(juce::Colour(0xff66567f)); g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(8), 58);
    g.addTransform(juce::AffineTransform::scale(float(getWidth())/1000, float(getHeight())/720));
    g.setColour(juce::Colour(0xff242a2c)); g.fillRoundedRectangle(85, 65, 830, 595, 22);
    g.setColour(juce::Colour(0xffb9e3b4)); g.setFont(27);
    g.drawText("W E E   /   SAMPLE CARTRIDGE", 40, 20, 920, 36, juce::Justification::centred);
    g.setColour(juce::Colours::lightgreen); g.fillEllipse(30, 35, 8, 8);
    const juce::Rectangle<float> display(110, 210, 780, 62);
    g.setColour(juce::Colour(0xff111e1a)); g.fillRoundedRectangle(display, 6);
    g.setColour(juce::Colour(0xff9bdcad));
    for (size_t i = 0; i < waveform.size(); ++i) {
        float x = display.getX() + float(i) / float(waveform.size()) * display.getWidth();
        float y = std::min(1.0f, waveform[i]) * 27;
        g.drawVerticalLine(int(x), display.getCentreY()-y, display.getCentreY()+y);
    }
    const float start = processor.state.getRawParameterValue("sampleStart")->load();
    const float end = processor.state.getRawParameterValue("sampleEnd")->load();
    g.setColour(juce::Colour(0xffffcf83));
    g.drawVerticalLine(int(display.getX() + start * display.getWidth()), display.getY(), display.getBottom());
    g.drawVerticalLine(int(display.getX() + end * display.getWidth()), display.getY(), display.getBottom());
    g.setFont(12); g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.drawText("PHASE 2   /   32 VOICES   /   ORIGINAL SAMPLE + SYNTH ENGINE", 30, 681, 940, 22, juce::Justification::centred);
}
void WeeEditor::resized() {
    auto place = [&](juce::Component& c, int x, int y, int w, int h) {
        c.setBounds(juce::roundToInt(float(x)*float(getWidth())/1000), juce::roundToInt(float(y)*float(getHeight())/720),
                    juce::roundToInt(float(w)*float(getWidth())/1000), juce::roundToInt(float(h)*float(getHeight())/720));
    };
    place(load,105,78,120,28); place(save,235,78,120,28); place(source,560,78,160,28); place(oscillator,730,78,160,28);
    place(status,105,110,785,26); place(sampleLoad,110,144,230,28); place(sampleClear,350,144,140,28); place(loop,560,144,320,28);
    place(sampleInfo,110,178,780,28);
    for (int i=0;i<6;++i) { place(labels[size_t(i)],110+i*130,282,130,22); place(knobs[size_t(i)],110+i*130,304,125,97); }
    for (int i=6;i<11;++i) { place(labels[size_t(i)],110+(i-6)*156,416,152,22); place(knobs[size_t(i)],110+(i-6)*156,440,152,65); }
    place(hint,110,515,780,30); place(keys,110,554,780,90);
    place(home,8,335,80,40); place(panic,912,335,80,40);
}
void WeeEditor::timerCallback() {
    status.setText("BPM " + juce::String(processor.bpm.load(), 1) + "    VOICES " + juce::String(processor.voiceCount.load()) +
        "    CPU " + juce::String(processor.cpu.load(), 1) + "%    PEAK " + juce::String(juce::Decibels::gainToDecibels(processor.peak.load(), -100.0f), 1) + " dB", juce::dontSendNotification);
    sampleInfo.setText(processor.sampleStatus(), juce::dontSendNotification);
    if (revision != processor.sampleRevision()) { revision = processor.sampleRevision(); waveform = processor.sampleWaveform(); }
    const bool sampling = source.getSelectedId() == 2;
    oscillator.setEnabled(!sampling); loop.setEnabled(sampling);
    for (size_t i=6;i<knobs.size();++i) knobs[i].setEnabled(sampling);
    repaint();
}
void WeeEditor::chooseSample() {
    if (chooser) return;
    chooser = std::make_unique<juce::FileChooser>("Load or relocate a sample", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.wav;*.aif;*.aiff;*.flac");
    juce::Component::SafePointer<WeeEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& fc) {
            if (!safe) return;
            if (fc.getResult() != juce::File()) safe->processor.loadSample(fc.getResult());
            // Destroy the chooser after its callback has returned.
            juce::MessageManager::callAsync([safe] { if (safe) safe->chooser.reset(); });
        });
}
void WeeEditor::choosePreset(bool saving) {
    if (chooser) return;
    chooser = std::make_unique<juce::FileChooser>(saving ? "Save WEE patch" : "Load WEE patch", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.wee");
    juce::Component::SafePointer<WeeEditor> safe(this);
    chooser->launchAsync((saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles,
        [safe, saving](const juce::FileChooser& fc) {
            if (!safe) return;
            if (fc.getResult() != juce::File()) {
                auto f = saving ? fc.getResult().withFileExtension("wee") : fc.getResult();
                const bool ok = saving ? safe->processor.savePreset(f) : safe->processor.loadPreset(f);
                if (!ok) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "WEE", "Patch operation failed. Check schema, trim/key ranges and permissions.");
            }
            juce::MessageManager::callAsync([safe] { if (safe) safe->chooser.reset(); });
        });
}
bool WeeEditor::isInterestedInFileDrag(const juce::StringArray& files) {
    return files.size() == 1 && juce::File(files[0]).hasFileExtension("wav;aif;aiff;flac;wee");
}
void WeeEditor::filesDropped(const juce::StringArray& files, int, int) {
    if (!isInterestedInFileDrag(files)) return;
    const juce::File file(files[0]);
    if (file.hasFileExtension("wee")) {
        if (!processor.loadPreset(file)) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "WEE", "Unable to load patch.");
    } else processor.loadSample(file);
}
