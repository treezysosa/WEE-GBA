#include "PluginEditor.h"
WeeParameterControl::WeeParameterControl(WeeProcessor& p,size_t l,size_t param,const juce::String& name) : processor(p),layer(l),parameter(param) {
    const auto& spec=wee::specs[param];
    initialise(WeeProcessor::parameterId(l,param),name.isEmpty() ? juce::String(spec.name) : name,spec.type,spec.choices);
}
WeeParameterControl::WeeParameterControl(WeeProcessor& p,const juce::String& id,const juce::String& name) : processor(p) {
    initialise(id,name,wee::ParamType::continuous,"");
}
void WeeParameterControl::initialise(const juce::String& id,const juce::String& name,wee::ParamType type,const char* items) {
    addAndMakeVisible(label);label.setText(name,juce::dontSendNotification);label.setJustificationType(juce::Justification::centredLeft);
    if(type==wee::ParamType::choice) {
        addAndMakeVisible(choices);choices.addItemList(juce::StringArray::fromTokens(items,"|",""),1);choices.setName(name);
        choiceAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state,id,choices);
    } else if(type==wee::ParamType::boolean) {
        addAndMakeVisible(toggle);toggle.setName(name);
        buttonAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,id,toggle);
    } else {
        addAndMakeVisible(slider);slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,70,24);slider.setName(name);
        slider.setNumDecimalPlacesToDisplay(type==wee::ParamType::integer ? 0 : 3);
        sliderAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,id,slider);
    }
}
void WeeParameterControl::resized() {
    label.setBounds(4,0,getWidth()-8,22);
    auto bounds=juce::Rectangle<int>(4,24,getWidth()-12,getHeight()-26);
    slider.setBounds(bounds);choices.setBounds(bounds);toggle.setBounds(bounds);
}
void WeeParameterControl::refresh() {
    if(parameter>=wee::paramCount) return;
    auto read=[&](size_t p){return processor.state.getRawParameterValue(WeeProcessor::parameterId(layer,p))->load();};
    const bool sample=read(wee::Source)>=.5f;const int mode=int(read(wee::SynthMode));
    bool enabled=true;
    switch(parameter) {
        case wee::Waveform:enabled=!sample && mode==0;break;
        case wee::SynthMode:enabled=!sample;break;
        case wee::WTPosition:case wee::OscMix:case wee::Detune:enabled=!sample && mode==1;break;
        case wee::FMRatio:case wee::FMIndex:enabled=!sample && mode==2;break;
        case wee::GrainPosition:case wee::GrainSize:case wee::GrainDensity:case wee::GrainSpray:enabled=!sample && mode==3;break;
        case wee::Root:case wee::Start:case wee::End:enabled=sample || mode==3;break;
        case wee::Loop:enabled=sample;break;
        case wee::Resonance:enabled=read(wee::FilterMode)>=.5f;break;
        case wee::LFORate:enabled=read(wee::LFOSync)<.5f;break;
        case wee::LFOBeats:enabled=read(wee::LFOSync)>=.5f;break;
        default:break;
    }
    setEnabled(enabled);
}
WeeEditor::WeeEditor(WeeProcessor& p) : AudioProcessorEditor(p),processor(p),keys(p.keyboard,juce::MidiKeyboardComponent::horizontalKeyboard) {
    theme.setColour(juce::Slider::trackColourId,juce::Colour(0xff9bdcad));
    theme.setColour(juce::Slider::thumbColourId,juce::Colour(0xffe2ddf2));
    theme.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff423657));
    setLookAndFeel(&theme);
    setResizable(true,true);setResizeLimits(1000,800,1600,1280);setSize(1100,880);
    for(auto* b:{&load,&save,&panic,&home,&previous,&next,&sampleLoad,&sampleClear}) addAndMakeVisible(b);
    load.onClick=[this]{choosePreset(false);};save.onClick=[this]{choosePreset(true);};
    sampleLoad.onClick=[this]{chooseSample();};sampleClear.onClick=[this]{processor.clearSample();};
    panic.onClick=[this]{processor.panic();};
    home.onClick=[this]{processor.clearSample();for(auto* p:processor.getParameters()){p->beginChangeGesture();p->setValueNotifyingHost(p->getDefaultValue());p->endChangeGesture();}};
    for(size_t i=0;i<wee::layerCount;++i) {
        layerChoice.addItem("LAYER " + juce::String(int(i+1)),int(i+1));
        auto& button=layerButtons[i];button.setButtonText(juce::String(int(i+1)));button.setName("Enable layer " + juce::String(int(i+1)));addAndMakeVisible(button);
        layerAttachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,WeeProcessor::parameterId(i,wee::Enabled),button);
    }
    addAndMakeVisible(layerChoice);layerChoice.setSelectedId(1,juce::dontSendNotification);layerChoice.setName("Edit layer");
    layerChoice.onChange=[this]{selectedLayer=size_t(layerChoice.getSelectedId()-1);rebuildPage();};
    previous.onClick=[this]{layerChoice.setSelectedId(int((selectedLayer+7)%8)+1);};
    next.onClick=[this]{layerChoice.setSelectedId(int((selectedLayer+1)%8)+1);};
    pageChoice.addItemList({"SYNTH","SAMPLE / GRANULAR","MODULATION","LAYER MIXER"},1);
    pageChoice.setSelectedId(1,juce::dontSendNotification);pageChoice.setName("Instrument page");addAndMakeVisible(pageChoice);
    pageChoice.onChange=[this]{page=pageChoice.getSelectedId();rebuildPage();};
    addAndMakeVisible(viewport);viewport.setViewedComponent(&panel,false);viewport.setScrollBarsShown(true,false);
    addAndMakeVisible(keys);keys.setAvailableRange(0,127);keys.setLowestVisibleKey(48);keys.setWantsKeyboardFocus(true);
    for(auto* label:{&status,&sampleInfo,&hint}){addAndMakeVisible(label);label->setJustificationType(juce::Justification::centred);}
    hint.setText("Enable layers 1–8 above. Edit one layer at a time; all sample/grain layers share the loaded recording.",juce::dontSendNotification);
    waveform=p.sampleWaveform();revision=p.sampleRevision();rebuildPage();startTimerHz(15);
}
void WeeEditor::rebuildPage() {
    controls.clear();
    auto add=[&](size_t param){auto c=std::make_unique<WeeParameterControl>(processor,selectedLayer,param);panel.addAndMakeVisible(*c);controls.push_back(std::move(c));};
    if(page==1) for(auto p:{wee::Source,wee::SynthMode,wee::Waveform,wee::Gain,wee::Pan,wee::Transpose,wee::Attack,wee::Decay,wee::Sustain,wee::Release,wee::Cutoff,wee::FilterMode,wee::Resonance,wee::WTPosition,wee::OscMix,wee::Detune,wee::FMRatio,wee::FMIndex}) add(p);
    else if(page==2) for(auto p:{wee::Source,wee::Root,wee::Loop,wee::Start,wee::End,wee::GrainPosition,wee::Low,wee::High,wee::GrainSize,wee::GrainDensity,wee::GrainSpray}) add(p);
    else if(page==3) {
        for(auto p:{wee::LFORate,wee::LFOSync,wee::LFOBeats,wee::LFOShape}) add(p);
        for(size_t p=wee::Route1Source;p<wee::paramCount;++p) add(p);
    } else {
        auto master=std::make_unique<WeeParameterControl>(processor,"masterGain","MASTER LEVEL");panel.addAndMakeVisible(*master);controls.push_back(std::move(master));
        for(size_t l=0;l<wee::layerCount;++l) for(auto p:{wee::Gain,wee::Pan}) {
            auto c=std::make_unique<WeeParameterControl>(processor,l,p,"Layer " + juce::String(int(l+1)) + " " + wee::specs[p].name);
            panel.addAndMakeVisible(*c);controls.push_back(std::move(c));
        }
    }
    viewport.setViewPosition(0,0);arrangeControls();for(auto& c:controls)c->refresh();
}
void WeeEditor::arrangeControls() {
    const int width=std::max(300,viewport.getWidth()-20),cell=width/(page==4?2:3);
    const int rowHeight=juce::roundToInt(float(getHeight())/800*64);
    int maxRow=0;
    for(size_t i=0;i<controls.size();++i) {
        int row=int(i/3),col=int(i%3);
        if(page==3 && i>=4) {row=2+int((i-4)/3);col=int((i-4)%3);}
        if(page==4 && i>=1) {row=1+int((i-1)/2);col=int((i-1)%2);}
        controls[i]->setBounds(col*cell,row*rowHeight,cell-6,rowHeight-4);maxRow=std::max(maxRow,row);
    }
    panel.setSize(width,(maxRow+1)*rowHeight);
}
void WeeEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff17141e));g.setColour(juce::Colour(0xff66567f));g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(8),58);
    g.addTransform(juce::AffineTransform::scale(float(getWidth())/1000,float(getHeight())/800));
    g.setColour(juce::Colour(0xff242a2c));g.fillRoundedRectangle(85,65,830,680,22);
    g.setColour(juce::Colour(0xffb9e3b4));g.setFont(27);g.drawText("W E E   /   HYBRID CARTRIDGE",40,20,920,36,juce::Justification::centred);
    g.setColour(juce::Colours::lightgreen);g.fillEllipse(30,35,8,8);
    const juce::Rectangle<float> display(110,210,780,48);g.setColour(juce::Colour(0xff111e1a));g.fillRoundedRectangle(display,6);g.setColour(juce::Colour(0xff9bdcad));
    for(size_t i=0;i<waveform.size();++i){float x=display.getX()+float(i)/float(waveform.size())*display.getWidth();float y=std::min(1.0f,waveform[i])*21;g.drawVerticalLine(int(x),display.getCentreY()-y,display.getCentreY()+y);}
    const auto start=processor.state.getRawParameterValue(WeeProcessor::parameterId(selectedLayer,wee::Start))->load(),end=processor.state.getRawParameterValue(WeeProcessor::parameterId(selectedLayer,wee::End))->load();
    g.setColour(juce::Colour(0xffffcf83));g.drawVerticalLine(int(display.getX()+start*display.getWidth()),display.getY(),display.getBottom());g.drawVerticalLine(int(display.getX()+end*display.getWidth()),display.getY(),display.getBottom());
    g.setFont(12);g.setColour(juce::Colours::white.withAlpha(.7f));g.drawText("PHASE 3   /   8 LAYERS   /   VA + SAMPLE + WAVETABLE + FM + GRANULAR",30,765,940,22,juce::Justification::centred);
}
void WeeEditor::resized() {
    auto place=[&](juce::Component& c,int x,int y,int w,int h){c.setBounds(juce::roundToInt(float(x)*float(getWidth())/1000),juce::roundToInt(float(y)*float(getHeight())/800),juce::roundToInt(float(w)*float(getWidth())/1000),juce::roundToInt(float(h)*float(getHeight())/800));};
    place(load,105,78,120,28);place(save,235,78,120,28);place(layerChoice,510,78,150,28);place(pageChoice,670,78,220,28);
    place(status,105,110,785,26);place(sampleLoad,110,144,230,28);place(sampleClear,350,144,200,28);
    for(size_t i=0;i<wee::layerCount;++i)place(layerButtons[i],566+int(i)*40,144,40,28);
    place(sampleInfo,110,178,780,28);place(viewport,110,270,780,356);
    place(hint,110,630,780,32);place(keys,110,666,780,64);
    place(home,8,340,80,40);place(panic,912,340,80,40);place(previous,8,390,80,32);place(next,912,390,80,32);arrangeControls();
}
void WeeEditor::timerCallback() {
    status.setText("BPM " + juce::String(processor.bpm.load(),1)+"   VOICES "+juce::String(processor.voiceCount.load())+"   L"+juce::String(int(selectedLayer+1))+" "+juce::String(processor.layerVoices[selectedLayer].load())+"   CPU "+juce::String(processor.cpu.load(),1)+"%   PEAK "+juce::String(juce::Decibels::gainToDecibels(processor.peak.load(),-100.0f),1)+" dB",juce::dontSendNotification);
    sampleInfo.setText(processor.sampleStatus(),juce::dontSendNotification);
    if(revision!=processor.sampleRevision()){revision=processor.sampleRevision();waveform=processor.sampleWaveform();}
    for(auto& c:controls)c->refresh();repaint();
}
void WeeEditor::chooseSample() {
    if(chooser)return;
    chooser=std::make_unique<juce::FileChooser>("Load or relocate the shared sample",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.wav;*.aif;*.aiff;*.flac");
    juce::Component::SafePointer<WeeEditor> safe(this);const size_t layer=selectedLayer;
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,layer](const juce::FileChooser& fc){if(!safe)return;if(fc.getResult()!=juce::File())safe->processor.loadSample(fc.getResult(),layer);juce::MessageManager::callAsync([safe]{if(safe)safe->chooser.reset();});});
}
void WeeEditor::choosePreset(bool saving) {
    if(chooser)return;
    chooser=std::make_unique<juce::FileChooser>(saving?"Save WEE patch":"Load WEE patch",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.wee");
    juce::Component::SafePointer<WeeEditor> safe(this);
    chooser->launchAsync((saving?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode)|juce::FileBrowserComponent::canSelectFiles,[safe,saving](const juce::FileChooser& fc){
        if(!safe)return;
        if(fc.getResult()!=juce::File()) {
            auto file=saving?fc.getResult().withFileExtension("wee"):fc.getResult();const bool ok=saving?safe->processor.savePreset(file):safe->processor.loadPreset(file);
            if(!ok)juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"WEE","Patch operation failed. Check schema, all layer trim/key ranges, and permissions.");
        }
        juce::MessageManager::callAsync([safe]{if(safe)safe->chooser.reset();});
    });
}
bool WeeEditor::isInterestedInFileDrag(const juce::StringArray& files){return files.size()==1 && juce::File(files[0]).hasFileExtension("wav;aif;aiff;flac;wee");}
void WeeEditor::filesDropped(const juce::StringArray& files,int,int){
    if(!isInterestedInFileDrag(files))return;const juce::File file(files[0]);
    if(file.hasFileExtension("wee")){if(!processor.loadPreset(file))juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"WEE","Unable to load patch.");}
    else processor.loadSample(file,selectedLayer);
}
