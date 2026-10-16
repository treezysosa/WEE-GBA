#include "PluginProcessor.h"
#include <iostream>
#include <thread>
void check(bool condition, const char* why) { if (!condition) { std::cerr << why << '\n'; std::exit(1); } }
void set(WeeProcessor& p, const char* id, float value) {
    auto* param = p.state.getParameter(id); param->setValueNotifyingHost(param->convertTo0to1(value));
}
void waitForLoad(WeeProcessor& p) {
    const auto until = juce::Time::getMillisecondCounterHiRes()+10000;
    while(p.sampleLoading() && juce::Time::getMillisecondCounterHiRes()<until) juce::Thread::sleep(2);
    check(!p.sampleLoading(), "loader timeout");
}
float play(WeeProcessor& p, int note = 60) {
    juce::AudioBuffer<float> buffer(2,512); juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1,note,1.0f),0);
    p.processBlock(buffer,midi);
    return buffer.getMagnitude(0,512);
}
class TestPlayHead final:public juce::AudioPlayHead {
public:
    PositionInfo position;
    juce::Optional<PositionInfo> getPosition()const override{return position;}
};
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File content(WEE_TEST_CONTENT_DIR);
    auto originalOwner=std::make_unique<WeeProcessor>(); auto& original=*originalOwner; original.prepareToPlay(48000,512);
    set(original,"gain",0.67f);
    juce::MemoryBlock saved; original.getStateInformation(saved);
    auto restoredOwner=std::make_unique<WeeProcessor>(); auto& restored=*restoredOwner; restored.prepareToPlay(48000,512); restored.setStateInformation(saved.getData(),int(saved.getSize()));
    check(std::abs(restored.state.getRawParameterValue("gain")->load()-0.67f)<0.0001f,"host state round trip");
    const char invalid[]="invalid"; restored.setStateInformation(invalid,sizeof invalid);
    check(std::abs(restored.state.getRawParameterValue("gain")->load()-0.67f)<0.0001f,"invalid binary leaves state intact");
    auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("wee-state-test","");
    check(dir.createDirectory().wasOk(),"create temporary directory");
    auto file=dir.getChildFile("patch.wee");
    check(original.savePreset(file) && restored.loadPreset(file),"preset round trip");
    set(restored,"l8_enabled",1);set(restored,"l8_synthMode",2);
    // Old Phase 1 preset explicitly migrates added parameters to defaults.
    set(restored,"source",1); set(restored,"root",72);
    check(restored.loadPreset(content.getChildFile("Presets/Soft Orbit.wee")),"schema 1 migration");
    check(restored.state.getRawParameterValue("source")->load()==0 && restored.state.getRawParameterValue("root")->load()==60,"migration defaults");
    check(restored.state.getRawParameterValue("l8_enabled")->load()==0,"migration disables added layers");
    check(restored.loadPreset(content.getChildFile("Presets/Pocket Reed.wee")),"schema 2 migration");waitForLoad(restored);
    check(restored.state.getRawParameterValue("source")->load()==1 && restored.state.getRawParameterValue("synthMode")->load()==0,"schema 2 source retained");
    check(!restored.loadPreset(content.getChildFile("not-present.wee")),"missing preset rejected");
    const auto sample=content.getChildFile("Samples/Pocket Reed.wav"); check(sample.existsAsFile(),"generate demo samples before state tests");
    original.loadSample(sample); waitForLoad(original);
    check(original.sampleWaveform().size()==256,"waveform preview"); check(play(original)>0.001f,"decoded WAV is playable");
    set(original,"loop",1); set(original,"root",64); set(original,"sampleStart",0.1f); set(original,"sampleEnd",0.8f);
    original.getStateInformation(saved); restored.setStateInformation(saved.getData(),int(saved.getSize())); waitForLoad(restored);
    check(restored.state.state.getProperty("samplePath").toString()==sample.getFullPathName(),"host sample path persistence");
    check(restored.state.getRawParameterValue("root")->load()==64 && restored.state.getRawParameterValue("loop")->load()==1,"sample settings recall");
    check(play(restored,64)>0.001f,"host-recalled sample playable");
    // Save and move the preset/sample folder, deleting the original relative target.
    auto local=dir.getChildFile("source.wav"); check(sample.copyFileTo(local),"copy sample"); original.loadSample(local); waitForLoad(original);
    check(original.savePreset(file),"sample preset save");
    auto moved=dir.getChildFile("moved"); moved.createDirectory();
    check(file.moveFileTo(moved.getChildFile("patch.wee")) && local.moveFileTo(moved.getChildFile("source.wav")),"move content folder");
    check(restored.loadPreset(moved.getChildFile("patch.wee")),"load moved preset"); waitForLoad(restored);
    check(restored.state.state.getProperty("samplePath").toString()==moved.getChildFile("source.wav").getFullPathName(),"relative path recovery");
    check(play(restored,64)>0.001f,"moved sample audible");
    // Missing/corrupt files replace the sample with silence; no oscillator fallback.
    restored.loadSample(dir.getChildFile("missing.wav")); waitForLoad(restored);
    check(restored.sampleStatus().contains("Cannot read"),"missing file message"); check(play(restored)==0,"missing file silence");
    auto bad=dir.getChildFile("corrupt.wav"); bad.replaceWithText("not a wave file"); restored.loadSample(bad); waitForLoad(restored); check(play(restored)==0,"corrupt file silence");
    restored.loadSample(sample); waitForLoad(restored); check(play(restored)>0.001f,"manual relocation recovers");
    set(original,"sampleStart",0.9f);
    check(!original.savePreset(file),"invalid trim cannot overwrite a valid preset");
    set(original,"sampleStart",0.1f);
    // Reject invalid mappings, unknown schemas, nonfinite values and duplicate IDs.
    check(original.savePreset(file),"create validation fixture");
    const auto xml=juce::XmlDocument::parse(file); check(xml!=nullptr,"parse fixture");
    auto reject = [&](juce::ValueTree tree) {
        auto text=tree.createXml(); check(text && bad.replaceWithText(text->toString()),"write invalid preset");
        check(!restored.loadPreset(bad),"invalid preset rejected");
    };
    auto tree=juce::ValueTree::fromXml(*xml); tree.setProperty("schema",99,nullptr); reject(tree);
    tree=juce::ValueTree::fromXml(*xml); tree.getChildWithProperty("id","sampleStart").setProperty("value",1,nullptr); reject(tree);
    tree=juce::ValueTree::fromXml(*xml); tree.getChildWithProperty("id","gain").setProperty("value","nan",nullptr); reject(tree);
    tree=juce::ValueTree::fromXml(*xml); tree.getChildWithProperty("id","gain").setProperty("id","attack",nullptr); reject(tree);
    // Repeated requests and playback exercise handoff and destruction with active voices.
    for(int i=0;i<80;++i) {
        restored.loadSample(i%3==0 ? bad : sample);
        play(restored);
    }
    restored.loadSample(sample); waitForLoad(restored); check(play(restored)>0.001f,"last sample request wins");
    // AIFF and FLAC codec round trips through the same worker.
    for (int formatIndex=0;formatIndex<2;++formatIndex) {
        std::unique_ptr<juce::AudioFormat> format;
        if(formatIndex==0) format=std::make_unique<juce::AiffAudioFormat>(); else format=std::make_unique<juce::FlacAudioFormat>();
        auto encoded=dir.getChildFile(formatIndex==0 ? "test.aiff" : "test.flac");
        auto stream=encoded.createOutputStream(); check(stream!=nullptr,"codec stream");
        std::unique_ptr<juce::AudioFormatWriter> writer(format->createWriterFor(stream.get(),44100,1,16,{},0));
        check(writer!=nullptr,"codec writer"); stream.release();
        juce::AudioBuffer<float> audio(1,4410); for(int i=0;i<4410;++i) audio.setSample(0,i,float(std::sin(i*0.04))*0.5f);
        check(writer->writeFromAudioSampleBuffer(audio,0,4410),"encode fixture"); writer.reset();
        restored.loadSample(encoded); waitForLoad(restored); check(play(restored)>0.001f,"mono AIFF/FLAC playback");
    }
    restored.clearSample(); waitForLoad(restored); check(play(restored)==0,"clear sample");
    // Hybrid presets, 8-layer recall, automation and short pressure MIDI messages.
    check(restored.loadPreset(content.getChildFile("Presets/Eightfold Nebula.wee")),"eight-layer preset");waitForLoad(restored);
    check(play(restored)>0.001f && restored.voiceCount.load()==8,"all eight layers audible/active");
    check(restored.savePreset(file),"schema 3 file save");
    check(original.loadPreset(file),"schema 3 multilayer recall");waitForLoad(original);
    check(original.state.getRawParameterValue("l8_enabled")->load()==1 && original.state.getRawParameterValue("l8_transpose")->load()==19,"layer 8 settings recalled");
    original.getStateInformation(saved);restored.setStateInformation(saved.getData(),int(saved.getSize()));waitForLoad(restored);
    check(play(restored)>0.001f && restored.layerVoices[7].load()==1,"host restores layer 8");
    check(restored.loadPreset(content.getChildFile("Presets/Grain Lantern.wee")),"granular hybrid preset");waitForLoad(restored);play(restored);
    check(restored.voiceCount.load()==2,"granular and wavetable layering");
    check(restored.loadPreset(content.getChildFile("Presets/Prism Pocket.wee")),"wavetable preset");waitForLoad(restored);
    set(restored,"mod1Source",4);set(restored,"mod1Destination",3);set(restored,"mod1Depth",-1);
    restored.panic();
    juce::AudioBuffer<float> midiAudio(2,512);juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1,60,1.0f),128);
    midi.addEvent(juce::MidiMessage::channelPressureChange(1,127),256);
    restored.processBlock(midiAudio,midi);
    check(midiAudio.getMagnitude(0,0,128)==0,"sample-positioned note begins after offset");
    check(midiAudio.getMagnitude(0,128,128)>0.00001f,"wavetable MIDI event sounds");
    check(midiAudio.getMagnitude(0,256,256)==0,"two-byte pressure MIDI controls routed amplitude");
    auto layerTree=juce::ValueTree::fromXml(*juce::XmlDocument::parse(file));layerTree.getChildWithProperty("id","l8_pan").setProperty("value",2,nullptr);reject(layerTree);
    // Vocal laboratory assets, edited boundaries, processing state and WAV export.
    // The preceding pressure-routing assertion deliberately muted amplitude.
    set(restored,"mod1Depth",0);
    auto syllables=content.getChildFile("Samples/Neon Syllables.wav");check(syllables.existsAsFile(),"synthetic vowel fixture");
    restored.loadSample(syllables,0,true);waitForLoad(restored);check(restored.sliceMarkers().count>=4,"background transient analysis");
    wee::SliceMap cuts;cuts.add(.25);cuts.add(.5);cuts.add(.75);check(restored.setSliceMarkers(cuts),"manual slice map accepted");waitForLoad(restored);check(restored.sliceMarkers().count==4,"manual map prepared");
    set(restored,"vocalStretch",1.5f);set(restored,"vocalPitch",5);set(restored,"vocalFormant",3);set(restored,"vocalPreserve",1);waitForLoad(restored);
    auto audible=[&](WeeProcessor& p,int note){p.panic();juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer events;float magnitude=0;for(int i=0;i<50;++i){if(i==0)events.addEvent(juce::MidiMessage::noteOn(1,note,1.0f),0);p.processBlock(audio,events);magnitude=std::max(magnitude,audio.getMagnitude(0,512));}return magnitude;};
    check(audible(restored,60)>.001f,"processed vocal chop audio");
    check(restored.savePreset(file),"schema 4 edited vocal preset save");check(original.loadPreset(file),"schema 4 vocal preset load");waitForLoad(original);
    check(original.sliceMarkers().count==4 && original.state.getRawParameterValue("vocalEnabled")->load()==1 && original.state.getRawParameterValue("vocalPitch")->load()==5,"vocal file state recall");
    restored.getStateInformation(saved);original.setStateInformation(saved.getData(),int(saved.getSize()));waitForLoad(original);check(original.sliceMarkers().points[2]==.5,"manual markers in host state");
    // Missing-file relocation retains manually edited slice boundaries.
    auto recovery=juce::ValueTree::fromXml(*juce::XmlDocument::parse(file));recovery.setProperty("samplePath",dir.getChildFile("missing-syllables.wav").getFullPathName(),nullptr);recovery.removeProperty("sampleRelative",nullptr);
    auto recoveryFile=dir.getChildFile("recover.wee");check(recoveryFile.replaceWithText(recovery.createXml()->toString()),"recovery fixture");
    check(original.loadPreset(recoveryFile),"missing vocal preset accepted with reference");waitForLoad(original);check(original.sliceMarkers().count==1,"missing asset publishes empty content");
    original.loadSample(syllables,0,true);waitForLoad(original);check(original.sliceMarkers().count==4,"manual boundaries survive relocation");
    original.previewSlice(2,0);juce::AudioBuffer<float> audition(2,512);juce::MidiBuffer empty;original.processBlock(audition,empty);check(original.voiceCount.load()>=1,"queued chop preview");original.stopPreview();
    auto rendered=dir.getChildFile("processed.wav");restored.exportVocal(rendered);
    const auto exportDeadline=juce::Time::getMillisecondCounterHiRes()+20000;while(restored.exporting() && juce::Time::getMillisecondCounterHiRes()<exportDeadline)juce::Thread::sleep(2);
    check(!restored.exporting() && rendered.existsAsFile(),"background processed WAV export");
    juce::AudioFormatManager readers;readers.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> renderReader(readers.createReaderFor(rendered));
    check(renderReader && renderReader->numChannels==2 && renderReader->bitsPerSample==24 && std::abs(double(renderReader->lengthInSamples)-132300)<3,"processed export duration/channels/format");renderReader.reset();
    check(restored.loadPreset(content.getChildFile("Presets/Neon Chop Sequence.wee")),"chop sequence factory preset");waitForLoad(restored);
    restored.panic();float sequenceAudio=0;for(int i=0;i<100;++i){restored.processBlock(audition,empty);sequenceAudio=std::max(sequenceAudio,audition.getMagnitude(0,512));}check(sequenceAudio>.001f,"standalone free-clock chop sequence renders without MIDI");restored.stopSequencer();
    TestPlayHead transport;transport.position.setBpm(120);transport.position.setPpqPosition(0);transport.position.setIsPlaying(false);restored.setPlayHead(&transport);set(restored,"chopSequence",1);
    restored.processBlock(audition,empty);check(audition.getMagnitude(0,512)==0,"host transport stop is silent");
    transport.position.setIsPlaying(true);double hostBeat=0;float hostAudio=0;
    for(int i=0;i<40;++i){transport.position.setPpqPosition(hostBeat);restored.processBlock(audition,empty);hostAudio=std::max(hostAudio,audition.getMagnitude(0,512));hostBeat+=512.0/48000*2;}
    check(hostAudio>.001f,"host PPQ clock plays sequence");transport.position.setIsPlaying(false);restored.processBlock(audition,empty);check(audition.getMagnitude(0,512)==0,"host stop clears sequence voices");restored.setPlayHead(nullptr);restored.stopSequencer();
    auto vocalTree=juce::ValueTree::fromXml(*juce::XmlDocument::parse(file));vocalTree.getChildWithName("SLICES").getChild(2).setProperty("value",-.5,nullptr);reject(vocalTree);
    check(!restored.setSliceMarkers(wee::SliceMap{0,{}}),"invalid manual slice count rejected");
    // Build and visit all editor pages; native mouse/DAW interaction remains manual.
    {std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor());juce::ComboBox* pages=nullptr;for(int i=0;i<editor->getNumChildComponents();++i)if(auto* box=dynamic_cast<juce::ComboBox*>(editor->getChildComponent(i)))if(box->getName()=="Instrument page")pages=box;check(pages!=nullptr,"page selector exists");for(int p=1;p<=5;++p)pages->setSelectedId(p,juce::sendNotificationSync);editor->setSize(1000,800);}
    original.clearSample();waitForLoad(original);restored.clearSample();waitForLoad(restored);dir.deleteRecursively();
    std::cout<<"State, migration, sample decoding, relocation, malformed input, replacement and codec tests passed.\n";
}
