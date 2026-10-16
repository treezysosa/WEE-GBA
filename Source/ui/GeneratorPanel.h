#pragma once
#include "PluginProcessor.h"
class GeneratorPanel final:public juce::Component {
    class Roll final:public juce::Component {
    public:
        wee::Pattern pattern;
        void paint(juce::Graphics& g)override{
            g.fillAll(juce::Colour(0xff13231c));const float width=float(getWidth()),height=float(getHeight());
            for(int bar=0;bar<=pattern.bars;++bar){g.setColour(juce::Colour(0xff4e6c56));const float x=width*bar/pattern.bars;g.drawLine(x,0,x,height);}
            int low=127,high=0;for(int i=0;i<pattern.count;++i){low=std::min(low,pattern.notes[size_t(i)].note);high=std::max(high,pattern.notes[size_t(i)].note);}if(!pattern.count){g.setColour(juce::Colours::white);g.drawText("Choose controls, then GENERATE",getLocalBounds(),juce::Justification::centred);return;}
            const int range=std::max(12,high-low+3);for(int i=0;i<pattern.count;++i){const auto& n=pattern.notes[size_t(i)];g.setColour(juce::Colour(0xffb5edbb).withAlpha(n.velocity/127.0f));g.fillRect(float(n.beat/pattern.beats())*width,height-((n.note-low+2)*height/range),std::max(2.0f,float(n.duration/pattern.beats())*width),std::max(2.0f,height/range-1));}
        }
    } roll;
    WeeProcessor& processor;
    wee::GeneratorSettings settings;
    bool updating=false;
    uint64_t revision=uint64_t(-1);
    juce::ComboBox style,mode,bars,root,scale,octave;
    juce::Slider seed,complexity,humanize,swing,density,variation,tempo;
    std::array<juce::Label,13> labels;
    juce::ToggleButton melodyLock{"Lock melody"},rhythmLock{"Lock rhythm"},keyLock{"Lock key"},soundLock{"Lock sound"},sync{"Host BPM / transport"};
    juce::TextButton generate{"GENERATE"},regenerate{"NEXT SEED"},randomize{"RANDOMIZE"},play{"PLAY LOOP"},stop{"STOP LOOP"},midi{"EXPORT MIDI"},wav{"EXPORT WAV"};
    juce::Label summary,exportInfo;
    std::unique_ptr<juce::FileChooser> chooser;
    void commit(){if(!updating){settings.style=style.getSelectedId()-1;settings.mode=mode.getSelectedId()-1;constexpr int lengths[]{1,2,4,8,16};settings.bars=lengths[juce::jlimit(0,4,bars.getSelectedId()-1)];settings.root=root.getSelectedId()-1;settings.scale=scale.getSelectedId()-1;settings.octave=octave.getSelectedId()+1;settings.seed=uint32_t(seed.getValue());settings.complexity=int(complexity.getValue());settings.humanize=int(humanize.getValue());settings.swing=int(swing.getValue());settings.density=int(density.getValue());settings.variation=int(variation.getValue());settings.tempo=int(tempo.getValue());settings.lockMelody=melodyLock.getToggleState();settings.lockRhythm=rhythmLock.getToggleState();settings.lockKey=keyLock.getToggleState();settings.lockSound=soundLock.getToggleState();settings.hostSync=sync.getToggleState();processor.setGeneratorSettings(settings);}}
    void choose(bool isMidi){if(chooser)return;const auto ext=isMidi?".mid":".wav";chooser=std::make_unique<juce::FileChooser>("Export generated loop",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("WEE-loop"+juce::String(ext)),"*"+juce::String(ext));juce::Component::SafePointer<GeneratorPanel> safe(this);chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting|juce::FileBrowserComponent::canSelectFiles,[safe,isMidi](const juce::FileChooser& fc){if(!safe)return;if(fc.getResult()!=juce::File() && !safe->processor.exportGenerated(fc.getResult().withFileExtension(isMidi?"mid":"wav"),isMidi))juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"WEE","Generate notes first and wait for sample preparation before WAV export.");juce::MessageManager::callAsync([safe]{if(safe)safe->chooser.reset();});});}
public:
    explicit GeneratorPanel(WeeProcessor& p):processor(p){
        style.addItemList(juce::StringArray::fromTokens(wee::styleNames,"|",""),1);mode.addItemList(juce::StringArray::fromTokens(wee::modeNames,"|",""),1);bars.addItemList({"1 bar","2 bars","4 bars","8 bars","16 bars"},1);root.addItemList({"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},1);scale.addItemList(juce::StringArray::fromTokens(wee::scaleNames,"|",""),1);octave.addItemList({"2","3","4","5","6"},1);
        for(auto* box:{&style,&mode,&bars,&root,&scale,&octave}){addAndMakeVisible(box);box->onChange=[this]{commit();};}
        for(auto* slider:{&seed,&complexity,&humanize,&swing,&density,&variation,&tempo}){addAndMakeVisible(slider);slider->setRange(0,100,1);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,100,24);slider->onValueChange=[this]{commit();};}seed.setRange(0,2147483647,1);tempo.setRange(40,240,1);
        const char* names[]{"Style","Output mode","Loop length (4/4)","Root","Scale","Octave","Seed (exact replay)","Complexity","Humanization","Swing","Note density","Variation","Manual BPM"};for(int i=0;i<13;++i){labels[size_t(i)].setText(names[i],juce::dontSendNotification);addAndMakeVisible(labels[size_t(i)]);}
        for(auto* button:{&melodyLock,&rhythmLock,&keyLock,&soundLock,&sync}){addAndMakeVisible(button);button->onClick=[this]{commit();};}
        for(auto* button:{&generate,&regenerate,&randomize,&play,&stop,&midi,&wav})addAndMakeVisible(button);
        generate.onClick=[this]{commit();processor.generateLoop();refresh();};regenerate.onClick=[this]{commit();processor.generateLoop(1);refresh();};randomize.onClick=[this]{commit();processor.generateLoop(2);refresh();};play.onClick=[this]{processor.stopSequencer();processor.playGenerated(true);};stop.onClick=[this]{processor.playGenerated(false);};midi.onClick=[this]{choose(true);};wav.onClick=[this]{choose(false);};
        addAndMakeVisible(roll);addAndMakeVisible(summary);addAndMakeVisible(exportInfo);refresh();
    }
    void refresh(){
        if(revision!=processor.generatorRevision()){
            revision=processor.generatorRevision();const auto doc=processor.generatorDocument();settings=doc.settings;roll.pattern=doc.pattern;roll.repaint();updating=true;
            style.setSelectedId(settings.style+1,juce::dontSendNotification);mode.setSelectedId(settings.mode+1,juce::dontSendNotification);int length=1;for(int n=settings.bars;n>1;n/=2)++length;bars.setSelectedId(length,juce::dontSendNotification);root.setSelectedId(settings.root+1,juce::dontSendNotification);scale.setSelectedId(settings.scale+1,juce::dontSendNotification);octave.setSelectedId(settings.octave-1,juce::dontSendNotification);
            seed.setValue(settings.seed,juce::dontSendNotification);complexity.setValue(settings.complexity,juce::dontSendNotification);humanize.setValue(settings.humanize,juce::dontSendNotification);swing.setValue(settings.swing,juce::dontSendNotification);density.setValue(settings.density,juce::dontSendNotification);variation.setValue(settings.variation,juce::dontSendNotification);tempo.setValue(settings.tempo,juce::dontSendNotification);
            melodyLock.setToggleState(settings.lockMelody,juce::dontSendNotification);rhythmLock.setToggleState(settings.lockRhythm,juce::dontSendNotification);keyLock.setToggleState(settings.lockKey,juce::dontSendNotification);soundLock.setToggleState(settings.lockSound,juce::dontSendNotification);sync.setToggleState(settings.hostSync,juce::dontSendNotification);updating=false;
            juce::String chords;for(int b=0;b<doc.pattern.bars;++b)chords+=juce::String(doc.pattern.chords[size_t(b)]+1)+(b+1<doc.pattern.bars?" / ":"");summary.setText(juce::String(doc.pattern.count)+" notes | "+juce::String(doc.pattern.bars)+" bars | progression degrees: "+chords,juce::dontSendNotification);
        }
        play.setButtonText(processor.generatedPlaying()?"ARMED / PLAYING":"PLAY LOOP");play.setEnabled(roll.pattern.count>0);midi.setEnabled(roll.pattern.count>0 && !processor.loopExporting());wav.setEnabled(roll.pattern.count>0 && !processor.loopExporting() && !processor.sampleLoading());exportInfo.setText(processor.loopExportStatus(),juce::dontSendNotification);tempo.setEnabled(!settings.hostSync);
    }
    void resized()override{
        const int w=getWidth(),cell=w/3;const int action=w/7;int index=0;for(auto* b:{&generate,&regenerate,&randomize,&play,&stop,&midi,&wav})b->setBounds(index++*action,0,action-3,30);
        roll.setBounds(0,38,w,125);summary.setBounds(0,166,w,30);exportInfo.setBounds(0,195,w,28);
        std::array<juce::Component*,13> widgets{&style,&mode,&bars,&root,&scale,&octave,&seed,&complexity,&humanize,&swing,&density,&variation,&tempo};for(size_t i=0;i<widgets.size();++i){const int x=int(i%3)*cell,y=230+int(i/3)*64;labels[i].setBounds(x,y,cell-8,20);widgets[i]->setBounds(x,y+22,cell-8,30);}
        int i=0;for(auto* b:{&melodyLock,&rhythmLock,&keyLock,&soundLock,&sync}){b->setBounds((i%3)*cell,560+(i/3)*36,cell-8,30);++i;}
    }
};
