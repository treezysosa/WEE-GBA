#include "PluginProcessor.h"
#include "ui/PluginEditor.h"
#include <charconv>
juce::String WeeProcessor::parameterId(size_t layer, size_t param) {
    return (layer == 0 ? juce::String() : "l" + juce::String(int(layer+1)) + "_") + wee::specs[param].id;
}
namespace {
bool numberValue(const juce::ValueTree& child, float& number) {
    const auto value = child.getProperty("value").toString().toStdString();
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && std::isfinite(number);
}
juce::ValueTree sliceTree(const wee::SliceDocument& document) {
    juce::ValueTree tree("SLICES");tree.setProperty("manual",document.manual?1:0,nullptr);
    for(int i=0;i<=document.map.count;++i){juce::ValueTree point("POINT");point.setProperty("value",document.map.points[size_t(i)],nullptr);tree.appendChild(point,nullptr);}return tree;
}
bool readSlices(const juce::ValueTree& tree,wee::SliceDocument& document) {
    if(!tree.hasType("SLICES") || tree.getNumChildren()<2 || tree.getNumChildren()>wee::maxSlices+1)return false;
    const auto manual=tree.getProperty("manual").toString();if(manual!="0" && manual!="1")return false;document.manual=manual=="1";document.map.count=tree.getNumChildren()-1;
    for(int i=0;i<tree.getNumChildren();++i){const auto child=tree.getChild(i);if(!child.hasType("POINT"))return false;const auto value=child.getProperty("value").toString().toStdString();double number=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || !std::isfinite(number))return false;document.map.points[size_t(i)]=number;}
    return document.map.valid();
}
bool migrateAndValidate(juce::ValueTree& tree) {
    if(!tree.hasType("WEE"))return false;
    const int schema=int(tree.getProperty("schema",0));
    const int expected=schema==1?7:(schema==2?14:(schema==3?361:(schema==4?int(wee::layerCount*wee::paramCount+wee::vocalParamCount+2):-1)));
    if(expected<0 || tree.getNumChildren()!=expected)return false;
    auto result=tree.createCopy();result.removeAllChildren(nullptr);size_t seen=0;
    auto parameter=[&](const juce::String& id,const wee::ParamSpec& spec,float initial,bool required,float& out){
        out=initial;if(required){int found=0;for(const auto& child:tree)if(child.hasType("PARAM") && child.getProperty("id").toString()==id){if(!numberValue(child,out))return false;++found;}if(found!=1 || out<spec.min || out>spec.max || (spec.type!=wee::ParamType::continuous && out!=std::floor(out)))return false;++seen;}
        juce::ValueTree child("PARAM");child.setProperty("id",id,nullptr);child.setProperty("value",out,nullptr);result.appendChild(child,nullptr);return true;
    };
    for(size_t layer=0;layer<wee::layerCount;++layer){
        const wee::LayerConfig defaults(layer);std::array<float,wee::paramCount> v {};
        for(size_t p=0;p<wee::paramCount;++p){const bool required=schema==4 || (schema==3 && p<45) || (schema<3 && layer==0 && int(p)<expected);if(!parameter(WeeProcessor::parameterId(layer,p),wee::specs[p],defaults.v[p],required,v[p]))return false;}
        if(v[wee::Start]>=v[wee::End] || v[wee::Low]>v[wee::High])return false;
    }
    const wee::ParamSpec master{"masterGain","Master",0,1,1,wee::ParamType::continuous};float unused=0;
    if(!parameter("masterGain",master,1,schema>=3,unused))return false;
    for(const auto& spec:wee::vocalSpecs)if(!parameter(spec.id,spec,spec.initial,schema==4,unused))return false;
    wee::SliceDocument document;
    if(schema==4){int found=0;for(const auto& child:tree)if(child.hasType("SLICES")){if(!readSlices(child,document))return false;++found;}if(found!=1)return false;++seen;}
    if(seen!=size_t(expected))return false;result.appendChild(sliceTree(document),nullptr);
    if(result.getProperty("samplePath").toString().length()>4096 || result.getProperty("sampleRelative").toString().length()>4096)return false;
    if(schema==1){result.removeProperty("samplePath",nullptr);result.removeProperty("sampleRelative",nullptr);}
    result.setProperty("schema",4,nullptr);tree=result;return true;
}
}
juce::AudioProcessorValueTreeState::ParameterLayout WeeProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout parameters;
    for(size_t layer=0;layer<wee::layerCount;++layer) for(size_t p=0;p<wee::paramCount;++p) {
        const auto& spec=wee::specs[p];
        const auto id=juce::ParameterID{parameterId(layer,p),1};
        const auto name="Layer " + juce::String(int(layer+1)) + " " + spec.name;
        const float initial=wee::LayerConfig(layer).v[p];
        switch(spec.type) {
            case wee::ParamType::continuous:
                parameters.add(std::make_unique<juce::AudioParameterFloat>(id,name,juce::NormalisableRange<float>(spec.min,spec.max,0.0f,spec.skew),initial)); break;
            case wee::ParamType::integer:
                parameters.add(std::make_unique<juce::AudioParameterInt>(id,name,int(spec.min),int(spec.max),int(initial))); break;
            case wee::ParamType::boolean:
                parameters.add(std::make_unique<juce::AudioParameterBool>(id,name,initial>=.5f)); break;
            case wee::ParamType::choice:
                parameters.add(std::make_unique<juce::AudioParameterChoice>(id,name,juce::StringArray::fromTokens(spec.choices,"|",""),int(initial))); break;
        }
    }
    for(const auto& spec:wee::vocalSpecs){
        const auto id=juce::ParameterID{spec.id,1};
        switch(spec.type){
            case wee::ParamType::continuous:parameters.add(std::make_unique<juce::AudioParameterFloat>(id,spec.name,juce::NormalisableRange<float>(spec.min,spec.max,0.0f,spec.skew),spec.initial));break;
            case wee::ParamType::integer:parameters.add(std::make_unique<juce::AudioParameterInt>(id,spec.name,int(spec.min),int(spec.max),int(spec.initial)));break;
            case wee::ParamType::boolean:parameters.add(std::make_unique<juce::AudioParameterBool>(id,spec.name,spec.initial>=.5f));break;
            case wee::ParamType::choice:parameters.add(std::make_unique<juce::AudioParameterChoice>(id,spec.name,juce::StringArray::fromTokens(spec.choices,"|",""),int(spec.initial)));break;
        }
    }
    parameters.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"masterGain",1},"Master level",0.0f,1.0f,1.0f));
    return parameters;
}
WeeProcessor::WeeProcessor() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    state(*this, nullptr, "WEE", layout()) {
    keyboard.addListener(this);
    for(size_t layer=0;layer<wee::layerCount;++layer) for(size_t p=0;p<wee::paramCount;++p) values[layer][p]=state.getRawParameterValue(parameterId(layer,p));
    masterGain=state.getRawParameterValue("masterGain");
    for(size_t p=0;p<wee::vocalParamCount;++p)vocalValues[p]=state.getRawParameterValue(wee::vocalSpecs[p].id);
    state.state.appendChild(sliceTree({}),nullptr);
    for(const auto* id:{"vocalFormant","vocalPreserve","vocalPitch"})state.addParameterListener(id,this);
}
bool WeeProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainInputChannelSet().isDisabled() && (b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || b.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}
WeeProcessor::~WeeProcessor(){for(const auto* id:{"vocalFormant","vocalPreserve","vocalPitch"})state.removeParameterListener(id,this);keyboard.removeListener(this);}
wee::VocalConfig WeeProcessor::vocalSettings()const noexcept{wee::VocalConfig c;for(size_t p=0;p<wee::vocalParamCount;++p)c.v[p]=vocalValues[p]->load();return c;}
void WeeProcessor::parameterChanged(const juce::String&,float){const float shift=vocalValues[wee::VFormant]->load()-(vocalValues[wee::VPreserve]->load()>=.5f?vocalValues[wee::VPitch]->load():0);loader.setFormantShift(shift);}
void WeeProcessor::prepareToPlay(double sr, int) { rate = sr; rack.prepare(sr);sequencer.reset(); }
void WeeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const auto start = juce::Time::getHighResolutionTicks();
    buffer.clear();
    if (loader.exchange.adopt()) rack.setSample(loader.exchange.current());
    bool hostAvailable=false,playing=false,hasPpq=false;double ppq=0;
    if(auto* host=getPlayHead()){hostAvailable=true;if(auto position=host->getPosition()){playing=position->getIsPlaying();if(auto tempo=position->getBpm())bpm.store(*tempo);if(auto beat=position->getPpqPosition()){ppq=*beat;hasPpq=true;}}}
    std::array<wee::LayerConfig, wee::layerCount> configs;
    for(size_t layer=0;layer<wee::layerCount;++layer) for(size_t p=0;p<wee::paramCount;++p) configs[layer].v[p]=values[layer][p]->load();
    rack.configure(configs,masterGain->load(),bpm.load());
    const auto vocal=vocalSettings();rack.configureVocal(vocal);
    if(panicRequested.exchange(false)){rack.allNotesOff(true);sequencer.reset();}
    const auto* current=loader.exchange.current();
    const bool sampleReady=!loader.sourceLoading.load() && current && current->valid();
    const bool vocalReady=!loader.busy() && current && current->generation==loader.completed.load();
    if(sequencer.begin(vocal,buffer.getNumSamples(),rate,bpm.load(),hostAvailable,playing,hasPpq,ppq,current?current->slices.count:1))rack.stopVocalLane(1);
    auto noteOn=[&](int note,float velocity){rack.noteOn(note,velocity,sampleReady,vocalReady);};
    auto noteOff = [&](int note) { rack.noteOff(note); };
    const int ready = keyQueue.getNumReady();
    int first, n1, second, n2;
    keyQueue.prepareToRead(ready, first, n1, second, n2);
    auto key=[&](int index){const auto e=keyEvents[size_t(index)];if(e.kind==1)rack.previewSlice(e.note,e.layer,vocalReady);else if(e.kind==2)rack.stopVocalLane(2);else if(e.velocity>0)noteOn(e.note,e.velocity);else noteOff(e.note);};
    for (int i = 0; i < n1; ++i) key(first + i);
    for (int i = 0; i < n2; ++i) key(second + i);
    keyQueue.finishedRead(n1 + n2);
    auto render=[&](int,int to){
        sequencer.until(to,[&](int from,int end){if(end>from)rack.render(buffer.getWritePointer(0,from),buffer.getNumChannels()>1?buffer.getWritePointer(1,from):nullptr,end-from);},
            [&](int note,float velocity){rack.sequenceOn(note,velocity,vocalReady);},[&](int note){rack.sequenceOff(note);});
    };
    int cursor = 0;
    for (const auto metadata : midi) {
        int pos = juce::jlimit(cursor, buffer.getNumSamples(), metadata.samplePosition);
        render(cursor, pos); cursor = pos;
        // Read short MIDI messages directly: constructing a MidiMessage for an
        // arbitrary SysEx payload could allocate on the audio thread.
        if (metadata.numBytes < 1) continue;
        const auto* bytes = metadata.data;
        const auto kind = bytes[0] & 0xf0;
        if (kind == 0xd0 && metadata.numBytes >= 2) { rack.aftertouch(float(bytes[1] & 127) / 127); continue; }
        if (metadata.numBytes < 3) continue;
        const int d1 = bytes[1] & 127, d2 = bytes[2] & 127;
        if (kind == 0x90 && d2) noteOn(d1, float(d2) / 127.0f);
        else if (kind == 0x80 || kind == 0x90) noteOff(d1);
        else if (kind == 0xe0) rack.pitchWheel(d1 | (d2 << 7));
        else if (kind == 0xb0 && d1 == 64) rack.sustainPedal(d2 >= 64);
        else if (kind == 0xb0 && d1 == 1) rack.modWheel(float(d2)/127);
        else if (kind == 0xa0) rack.aftertouch(float(d2)/127);
        else if (kind == 0xb0 && d1 == 120) rack.allNotesOff(true);
        else if (kind == 0xb0 && d1 == 123) rack.allNotesOff();
    }
    render(cursor, buffer.getNumSamples()); midi.clear();
    peak.store(buffer.getNumSamples() ? buffer.getMagnitude(0, buffer.getNumSamples()) : 0);
    voiceCount.store(rack.activeVoices());
    for(size_t layer=0;layer<wee::layerCount;++layer) layerVoices[layer].store(rack.activeVoices(layer));
    if (buffer.getNumSamples()) cpu.store(float(juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - start) * rate / buffer.getNumSamples() * 100));
}
juce::ValueTree WeeProcessor::patchState() {
    const juce::ScopedLock lock(contentStateMutex);
    auto tree = state.copyState(); tree.setProperty("schema",4,nullptr);
    wee::SliceDocument doc;if(readSlices(tree.getChildWithName("SLICES"),doc) && !doc.manual){doc.map=loader.slices();tree.removeChild(tree.getChildWithName("SLICES"),nullptr);tree.appendChild(sliceTree(doc),nullptr);}return tree;
}
void WeeProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = patchState().createXml()) copyXmlToBinary(*xml, dest);
}
bool WeeProcessor::applyPatch(juce::ValueTree tree, const juce::File& base) {
    const juce::ScopedLock lock(contentStateMutex);
    if (!migrateAndValidate(tree)) return false;
    auto path = tree.getProperty("samplePath").toString();
    const auto relative = tree.getProperty("sampleRelative").toString();
    // File-based presets can relocate with their sample folder. Host state uses
    // its stored absolute path; missing files are reported by the background loader.
    if (base != juce::File() && relative.isNotEmpty()) {
        const auto candidate = base.getChildFile(relative);
        if (candidate.existsAsFile() || path.isEmpty()) path = candidate.getFullPathName();
    }
    if (path.isNotEmpty() && !juce::File::isAbsolutePath(path)) return false;
    tree.setProperty("samplePath", path, nullptr); tree.removeProperty("sampleRelative", nullptr);
    wee::SliceDocument document;if(!readSlices(tree.getChildWithName("SLICES"),document))return false;
    state.replaceState(tree);panic();loader.request(path,document,vocalValues[wee::VSensitivity]->load());return true;
}
void WeeProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) applyPatch(juce::ValueTree::fromXml(*xml));
}
bool WeeProcessor::savePreset(const juce::File& file) {
    auto tree = patchState();
    if (!migrateAndValidate(tree)) return false;
    const auto path = tree.getProperty("samplePath").toString();
    if (path.isNotEmpty()) tree.setProperty("sampleRelative", juce::File(path).getRelativePathFrom(file.getParentDirectory()), nullptr);
    auto xml = tree.createXml(); juce::TemporaryFile temp(file);
    return xml && temp.getFile().replaceWithText(xml->toString()) && temp.overwriteTargetFileWithTemporary();
}
bool WeeProcessor::loadPreset(const juce::File& file) {
    if (file.getSize() > 1024 * 1024) return false;
    auto xml = juce::XmlDocument::parse(file);
    return xml && applyPatch(juce::ValueTree::fromXml(*xml), file.getParentDirectory());
}
void WeeProcessor::loadSample(const juce::File& file, size_t layer, bool vocal) {
    if(layer>=wee::layerCount) return;
    const juce::ScopedLock lock(contentStateMutex);
    const auto path=file.getFullPathName();const auto previousPath=state.state.getProperty("samplePath").toString();
    state.state.setProperty("samplePath",path,nullptr);
    const bool granular = values[layer][wee::Source]->load()<.5f && int(values[layer][wee::SynthMode]->load())==3;
    auto enable=[&](size_t p,float value) {auto* param=state.getParameter(parameterId(layer,p));param->beginChangeGesture();param->setValueNotifyingHost(param->convertTo0to1(value));param->endChangeGesture();};
    if(vocal){enable(wee::Source,1);enable(wee::VocalMode,1);}else if(!granular) enable(wee::Source,1);
    enable(wee::Enabled,1);
    wee::SliceDocument doc;readSlices(state.state.getChildWithName("SLICES"),doc);
    // Preserve manual edits on reload/missing-file recovery; new recordings reanalyse.
    const bool recovering=previousPath.isNotEmpty() && (previousPath==path || !juce::File(previousPath).existsAsFile());
    if(!doc.manual || !recovering)doc={};state.state.removeChild(state.state.getChildWithName("SLICES"),nullptr);state.state.appendChild(sliceTree(doc),nullptr);
    panic();loader.request(path,doc,vocalValues[wee::VSensitivity]->load());
}
void WeeProcessor::clearSample() {
    const juce::ScopedLock lock(contentStateMutex);
    state.state.setProperty("samplePath","",nullptr);state.state.removeChild(state.state.getChildWithName("SLICES"),nullptr);state.state.appendChild(sliceTree({}),nullptr);panic();loader.request("");
}
juce::AudioProcessorEditor* WeeProcessor::createEditor() { return new WeeEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WeeProcessor(); }

void WeeProcessor::enqueueKey(const KeyEvent& event){int first,n1,second,n2;keyQueue.prepareToWrite(1,first,n1,second,n2);if(n1)keyEvents[size_t(first)]=event;else panicRequested.store(true);keyQueue.finishedWrite(n1);}
void WeeProcessor::handleNoteOn(juce::MidiKeyboardState*,int,int note,float velocity){enqueueKey({note,velocity,0,0});}
void WeeProcessor::handleNoteOff(juce::MidiKeyboardState* k,int channel,int note,float){handleNoteOn(k,channel,note,0);}
void WeeProcessor::activateVocalLayer(size_t layer){if(layer>=wee::layerCount)return;for(auto p:{wee::Enabled,wee::Source,wee::VocalMode}){auto* parameter=state.getParameter(parameterId(layer,p));parameter->beginChangeGesture();parameter->setValueNotifyingHost(1);parameter->endChangeGesture();}}
void WeeProcessor::previewSlice(int slice,size_t layer){if(slice<0 || slice>=wee::maxSlices || layer>=wee::layerCount)return;activateVocalLayer(layer);enqueueKey({slice,1,1,layer});}
void WeeProcessor::stopPreview(){enqueueKey({0,0,2,0});}
void WeeProcessor::stopSequencer(){auto* p=state.getParameter("chopSequence");p->beginChangeGesture();p->setValueNotifyingHost(0);p->endChangeGesture();}
bool WeeProcessor::setSliceMarkers(const wee::SliceMap& map){if(!map.valid())return false;const juce::ScopedLock lock(contentStateMutex);wee::SliceDocument document{map,true};state.state.removeChild(state.state.getChildWithName("SLICES"),nullptr);state.state.appendChild(sliceTree(document),nullptr);loader.request(state.state.getProperty("samplePath").toString(),document,vocalValues[wee::VSensitivity]->load(),false);return true;}
void WeeProcessor::detectChops(){const juce::ScopedLock lock(contentStateMutex);state.state.removeChild(state.state.getChildWithName("SLICES"),nullptr);state.state.appendChild(sliceTree({}),nullptr);loader.request(state.state.getProperty("samplePath").toString(),{},vocalValues[wee::VSensitivity]->load(),false);}
void WeeProcessor::exportVocal(const juce::File& file,int slice){loader.exportAudio(file,vocalSettings(),bpm.load(),slice);}
