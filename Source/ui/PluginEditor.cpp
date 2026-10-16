#include "PluginEditor.h"
WeeParameterControl::WeeParameterControl(WeeProcessor& p,size_t l,size_t param,const juce::String& name) : processor(p),layer(l),parameter(param) {
    const auto& spec=wee::specs[param];
    initialise(WeeProcessor::parameterId(l,param),name.isEmpty() ? juce::String(spec.name) : name,spec.type,spec.choices);
}
WeeParameterControl::WeeParameterControl(WeeProcessor& p,const juce::String& id,const juce::String& name) : processor(p) {
    initialise(id,name,wee::ParamType::continuous,"");
}
WeeParameterControl::WeeParameterControl(WeeProcessor& p,const wee::ParamSpec& spec):processor(p){initialise(spec.id,spec.name,spec.type,spec.choices);}
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
        case wee::Loop:case wee::VocalMode:enabled=sample;break;
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
    panic.onClick=[this]{processor.stopSequencer();processor.stopPreview();processor.panic();};
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
    pageChoice.addItemList({"SYNTH","SAMPLE / GRANULAR","MODULATION","LAYER MIXER","VOCAL LAB"},1);
    pageChoice.setSelectedId(1,juce::dontSendNotification);pageChoice.setName("Instrument page");addAndMakeVisible(pageChoice);
    pageChoice.onChange=[this]{page=pageChoice.getSelectedId();rebuildPage();};
    addAndMakeVisible(viewport);viewport.setViewedComponent(&panel,false);viewport.setScrollBarsShown(true,false);
    addAndMakeVisible(keys);keys.setAvailableRange(0,127);keys.setLowestVisibleKey(48);keys.setWantsKeyboardFocus(true);
    for(auto* label:{&status,&sampleInfo,&hint}){addAndMakeVisible(label);label->setJustificationType(juce::Justification::centred);}
    hint.setText("Enable layers 1–8 above. Edit one layer at a time; all sample/grain layers share the loaded recording.",juce::dontSendNotification);
    for(auto* b:{&useVocal,&detect,&split,&remove,&preview,&stop,&exportClip,&exportChop})addAndMakeVisible(b);
    addAndMakeVisible(sliceWave);addAndMakeVisible(sliceChoice);addAndMakeVisible(sliceStart);addAndMakeVisible(sliceEnd);
    useVocal.onClick=[this]{processor.activateVocalLayer(selectedLayer);};detect.onClick=[this]{processor.detectChops();};
    split.onClick=[this]{sliceWave.split();};remove.onClick=[this]{sliceWave.remove();};
    preview.onClick=[this]{processor.previewSlice(sliceWave.selected,selectedLayer);};stop.onClick=[this]{processor.stopPreview();processor.stopSequencer();};
    exportClip.onClick=[this]{chooseExport(false);};exportChop.onClick=[this]{chooseExport(true);};
    sliceWave.edited=[this](const wee::SliceMap& map){processor.setSliceMarkers(map);updateSliceSelection();};
    sliceWave.selection=[this](int){updateSliceSelection();};sliceWave.preview=[this]{processor.previewSlice(sliceWave.selected,selectedLayer);};
    sliceChoice.onChange=[this]{if(!updatingSlices)sliceWave.select(std::max(0,sliceChoice.getSelectedId()-1));};
    for(auto* slider:{&sliceStart,&sliceEnd}){slider->setRange(0,1,.000001);slider->setNumDecimalPlacesToDisplay(6);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,82,24);slider->onDragStart=[this]{draggingBoundary=true;};slider->onDragEnd=[this]{draggingBoundary=false;processor.setSliceMarkers(sliceWave.markers);};}
    sliceStart.setName("Selected slice start normalized");sliceEnd.setName("Selected slice end normalized");
    auto editBoundary=[this](bool start){if(updatingSlices)return;auto map=sliceWave.markers;const int index=sliceWave.selected+(start?0:1);if(index<=0 || index>=map.count)return;map.points[size_t(index)]=std::clamp(start?sliceStart.getValue():sliceEnd.getValue(),map.points[size_t(index-1)]+.000001,map.points[size_t(index+1)]-.000001);sliceWave.markers=map;sliceWave.repaint();if(!draggingBoundary)processor.setSliceMarkers(map);};
    sliceStart.onValueChange=[editBoundary]{editBoundary(true);};sliceEnd.onValueChange=[editBoundary]{editBoundary(false);};
    waveform=p.sampleWaveform();sliceWave.peaks=waveform;sliceWave.markers=p.sliceMarkers();revision=p.sampleRevision();updateSliceSelection();rebuildPage();startTimerHz(15);
}
void WeeEditor::rebuildPage() {
    controls.clear();
    auto add=[&](size_t param){auto c=std::make_unique<WeeParameterControl>(processor,selectedLayer,param);panel.addAndMakeVisible(*c);controls.push_back(std::move(c));};
    if(page==1) for(auto p:{wee::Source,wee::SynthMode,wee::Waveform,wee::Gain,wee::Pan,wee::Transpose,wee::Attack,wee::Decay,wee::Sustain,wee::Release,wee::Cutoff,wee::FilterMode,wee::Resonance,wee::WTPosition,wee::OscMix,wee::Detune,wee::FMRatio,wee::FMIndex}) add(p);
    else if(page==2) for(auto p:{wee::Source,wee::VocalMode,wee::Root,wee::Loop,wee::Start,wee::End,wee::GrainPosition,wee::Low,wee::High,wee::GrainSize,wee::GrainDensity,wee::GrainSpray}) add(p);
    else if(page==3) {
        for(auto p:{wee::LFORate,wee::LFOSync,wee::LFOBeats,wee::LFOShape}) add(p);
        for(size_t p=wee::Route1Source;p<wee::VocalMode;++p) add(p);
    } else if(page==4) {
        auto master=std::make_unique<WeeParameterControl>(processor,"masterGain","MASTER LEVEL");panel.addAndMakeVisible(*master);controls.push_back(std::move(master));
        for(size_t l=0;l<wee::layerCount;++l) for(auto p:{wee::Gain,wee::Pan}) {
            auto c=std::make_unique<WeeParameterControl>(processor,l,p,"Layer " + juce::String(int(l+1)) + " " + wee::specs[p].name);
            panel.addAndMakeVisible(*c);controls.push_back(std::move(c));
        }
    }
    if(page==5){for(const auto& spec:wee::vocalSpecs){auto c=std::make_unique<WeeParameterControl>(processor,spec);panel.addAndMakeVisible(*c);controls.push_back(std::move(c));}}
    for(auto* c:{static_cast<juce::Component*>(&sliceWave),static_cast<juce::Component*>(&sliceChoice),static_cast<juce::Component*>(&sliceStart),static_cast<juce::Component*>(&sliceEnd),static_cast<juce::Component*>(&useVocal),static_cast<juce::Component*>(&detect),static_cast<juce::Component*>(&split),static_cast<juce::Component*>(&remove),static_cast<juce::Component*>(&preview),static_cast<juce::Component*>(&stop),static_cast<juce::Component*>(&exportClip),static_cast<juce::Component*>(&exportChop)})c->setVisible(page==5);
    hint.setText(page==5?"Double-click waveform to split; drag a line; right-click/Delete to remove. Formants rebuild in background.":"Enable layers 1–8 above. All sample/grain/vocal layers share the recording.",juce::dontSendNotification);
    viewport.setViewPosition(0,0);resized();for(auto& c:controls)c->refresh();
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
    g.setColour(juce::Colour(0xffb9e3b4));g.setFont(27);g.drawText("W E E   /   VOCAL CARTRIDGE",40,20,920,36,juce::Justification::centred);
    g.setColour(juce::Colours::lightgreen);g.fillEllipse(30,35,8,8);
    const juce::Rectangle<float> display(110,210,780,48);g.setColour(juce::Colour(0xff111e1a));g.fillRoundedRectangle(display,6);g.setColour(juce::Colour(0xff9bdcad));
    for(size_t i=0;i<waveform.size();++i){float x=display.getX()+float(i)/float(waveform.size())*display.getWidth();float y=std::min(1.0f,waveform[i])*21;g.drawVerticalLine(int(x),display.getCentreY()-y,display.getCentreY()+y);}
    const auto start=processor.state.getRawParameterValue(WeeProcessor::parameterId(selectedLayer,wee::Start))->load(),end=processor.state.getRawParameterValue(WeeProcessor::parameterId(selectedLayer,wee::End))->load();
    g.setColour(juce::Colour(0xffffcf83));g.drawVerticalLine(int(display.getX()+start*display.getWidth()),display.getY(),display.getBottom());g.drawVerticalLine(int(display.getX()+end*display.getWidth()),display.getY(),display.getBottom());
    g.setFont(12);g.setColour(juce::Colours::white.withAlpha(.7f));g.drawText("PHASE 4   /   VOCAL CHOPS + PITCH / TIME + SPECTRAL FORMANTS",30,765,940,22,juce::Justification::centred);
}
void WeeEditor::resized() {
    auto place=[&](juce::Component& c,int x,int y,int w,int h){c.setBounds(juce::roundToInt(float(x)*float(getWidth())/1000),juce::roundToInt(float(y)*float(getHeight())/800),juce::roundToInt(float(w)*float(getWidth())/1000),juce::roundToInt(float(h)*float(getHeight())/800));};
    place(load,105,78,120,28);place(save,235,78,120,28);place(layerChoice,510,78,150,28);place(pageChoice,670,78,220,28);
    place(status,105,110,785,26);place(sampleLoad,110,144,230,28);place(sampleClear,350,144,200,28);
    for(size_t i=0;i<wee::layerCount;++i)place(layerButtons[i],566+int(i)*40,144,40,28);
    place(sampleInfo,110,178,780,28);place(viewport,110,page==5?384:270,780,page==5?242:356);
    place(sliceWave,110,210,780,90);place(useVocal,110,306,150,26);place(detect,266,306,75,26);place(split,347,306,65,26);place(remove,418,306,80,26);place(preview,504,306,85,26);place(stop,595,306,65,26);place(exportClip,666,306,105,26);place(exportChop,777,306,113,26);
    place(sliceChoice,110,342,240,30);place(sliceStart,362,342,250,30);place(sliceEnd,624,342,266,30);
    place(hint,110,630,780,32);place(keys,110,666,780,64);
    place(home,8,340,80,40);place(panic,912,340,80,40);place(previous,8,390,80,32);place(next,912,390,80,32);arrangeControls();
}
void WeeEditor::timerCallback() {
    status.setText("BPM " + juce::String(processor.bpm.load(),1)+"   VOICES "+juce::String(processor.voiceCount.load())+"   L"+juce::String(int(selectedLayer+1))+" "+juce::String(processor.layerVoices[selectedLayer].load())+"   CPU "+juce::String(processor.cpu.load(),1)+"%   PEAK "+juce::String(juce::Decibels::gainToDecibels(processor.peak.load(),-100.0f),1)+" dB",juce::dontSendNotification);
    const auto info=processor.sampleStatus()+(page==5 && processor.exportStatus().isNotEmpty()?" | "+processor.exportStatus():juce::String());sampleInfo.setText(info,juce::dontSendNotification);sampleInfo.setTooltip(info);
    if(revision!=processor.sampleRevision()){revision=processor.sampleRevision();waveform=processor.sampleWaveform();sliceWave.peaks=waveform;sliceWave.markers=processor.sliceMarkers();sliceWave.selected=std::min(sliceWave.selected,sliceWave.markers.count-1);updateSliceSelection();}
    const bool ready=!processor.sampleLoading() && !waveform.empty();sliceWave.setEnabled(ready);detect.setEnabled(ready);split.setEnabled(ready && sliceWave.markers.count<wee::maxSlices);remove.setEnabled(ready && sliceWave.markers.count>1);preview.setEnabled(ready);exportClip.setEnabled(ready && !processor.exporting());exportChop.setEnabled(ready && !processor.exporting());
    sliceStart.setEnabled(ready && sliceWave.selected>0);sliceEnd.setEnabled(ready && sliceWave.selected<sliceWave.markers.count-1);
    for(auto& c:controls)c->refresh();repaint();
}
void WeeEditor::chooseSample() {
    if(chooser)return;
    chooser=std::make_unique<juce::FileChooser>("Load or relocate the shared sample",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.wav;*.aif;*.aiff;*.flac");
    juce::Component::SafePointer<WeeEditor> safe(this);const size_t layer=selectedLayer;const bool vocal=page==5;
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,layer,vocal](const juce::FileChooser& fc){if(!safe)return;if(fc.getResult()!=juce::File())safe->processor.loadSample(fc.getResult(),layer,vocal);juce::MessageManager::callAsync([safe]{if(safe)safe->chooser.reset();});});
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
    else processor.loadSample(file,selectedLayer,page==5);
}

void WeeEditor::updateSliceSelection(){
    updatingSlices=true;sliceChoice.clear(juce::dontSendNotification);
    for(int i=0;i<sliceWave.markers.count;++i)sliceChoice.addItem("Chop "+juce::String(i+1)+" / MIDI "+juce::String(int(processor.vocalSettings().v[wee::VBase])+i),i+1);
    sliceWave.selected=juce::jlimit(0,sliceWave.markers.count-1,sliceWave.selected);sliceChoice.setSelectedId(sliceWave.selected+1,juce::dontSendNotification);
    sliceStart.setValue(sliceWave.markers.points[size_t(sliceWave.selected)],juce::dontSendNotification);sliceEnd.setValue(sliceWave.markers.points[size_t(sliceWave.selected+1)],juce::dontSendNotification);updatingSlices=false;sliceWave.repaint();
}
void WeeEditor::chooseExport(bool slice){
    if(chooser)return;chooser=std::make_unique<juce::FileChooser>(slice?"Export processed chop WAV":"Export processed clip WAV",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("WEE-processed.wav"),"*.wav");
    juce::Component::SafePointer<WeeEditor> safe(this);const int index=slice?sliceWave.selected:-1;
    chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting|juce::FileBrowserComponent::canSelectFiles,[safe,index](const juce::FileChooser& fc){if(!safe)return;if(fc.getResult()!=juce::File())safe->processor.exportVocal(fc.getResult().withFileExtension("wav"),index);juce::MessageManager::callAsync([safe]{if(safe)safe->chooser.reset();});});
}
