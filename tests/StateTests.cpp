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
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File content(WEE_TEST_CONTENT_DIR);
    WeeProcessor original; original.prepareToPlay(48000,512);
    set(original,"gain",0.67f);
    juce::MemoryBlock saved; original.getStateInformation(saved);
    WeeProcessor restored; restored.prepareToPlay(48000,512); restored.setStateInformation(saved.getData(),int(saved.getSize()));
    check(std::abs(restored.state.getRawParameterValue("gain")->load()-0.67f)<0.0001f,"host state round trip");
    const char invalid[]="invalid"; restored.setStateInformation(invalid,sizeof invalid);
    check(std::abs(restored.state.getRawParameterValue("gain")->load()-0.67f)<0.0001f,"invalid binary leaves state intact");
    auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("wee-state-test","");
    check(dir.createDirectory().wasOk(),"create temporary directory");
    auto file=dir.getChildFile("patch.wee");
    check(original.savePreset(file) && restored.loadPreset(file),"preset round trip");
    // Old Phase 1 preset explicitly migrates added parameters to defaults.
    set(restored,"source",1); set(restored,"root",72);
    check(restored.loadPreset(content.getChildFile("Presets/Soft Orbit.wee")),"schema 1 migration");
    check(restored.state.getRawParameterValue("source")->load()==0 && restored.state.getRawParameterValue("root")->load()==60,"migration defaults");
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
    original.clearSample(); waitForLoad(original); dir.deleteRecursively();
    std::cout<<"State, migration, sample decoding, relocation, malformed input, replacement and codec tests passed.\n";
}
