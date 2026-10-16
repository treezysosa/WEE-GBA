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
bool migrateAndValidate(juce::ValueTree& tree) {
    if (!tree.hasType("WEE")) return false;
    const int schema = int(tree.getProperty("schema", 0));
    const int expected = schema == 1 ? 7 : (schema == 2 ? 14 : (schema == 3 ? int(wee::layerCount * wee::paramCount + 1) : -1));
    if (tree.getNumChildren() != expected || expected < 0) return false;
    juce::ValueTree result = tree.createCopy(); result.removeAllChildren(nullptr);
    size_t seen = 0;
    for (size_t layer = 0; layer < wee::layerCount; ++layer) {
        std::array<float, wee::paramCount> values {};
        const wee::LayerConfig defaults(layer);
        for (size_t p = 0; p < wee::paramCount; ++p) {
            const auto id = WeeProcessor::parameterId(layer,p);
            const auto& spec = wee::specs[p];
            float number = defaults.v[p];
            if (schema == 3 || (layer == 0 && int(p) < expected)) {
                int found = 0;
                for (const auto& child : tree) if (child.hasType("PARAM") && child.getProperty("id").toString() == id) {
                    if (!numberValue(child, number)) return false;
                    ++found;
                }
                if (found != 1 || number < spec.min || number > spec.max || (spec.type != wee::ParamType::continuous && number != std::floor(number))) return false;
                ++seen;
            }
            values[p] = number;
            juce::ValueTree child("PARAM"); child.setProperty("id",id,nullptr); child.setProperty("value",number,nullptr); result.appendChild(child,nullptr);
        }
        if (values[wee::Start] >= values[wee::End] || values[wee::Low] > values[wee::High]) return false;
    }
    float master = 1;
    if (schema == 3) {
        int found = 0;
        for (const auto& child : tree) if (child.hasType("PARAM") && child.getProperty("id").toString() == "masterGain") {
            if (!numberValue(child, master) || master < 0 || master > 1) return false;
            ++found;
        }
        if (found != 1) return false;
        ++seen;
    }
    if (seen != size_t(expected)) return false;
    juce::ValueTree global("PARAM"); global.setProperty("id","masterGain",nullptr); global.setProperty("value",master,nullptr); result.appendChild(global,nullptr);
    if (result.getProperty("samplePath").toString().length() > 4096 || result.getProperty("sampleRelative").toString().length() > 4096) return false;
    if (schema == 1) {result.removeProperty("samplePath",nullptr);result.removeProperty("sampleRelative",nullptr);}
    result.setProperty("schema",3,nullptr); tree = result;
    return true;
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
    parameters.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"masterGain",1},"Master level",0.0f,1.0f,1.0f));
    return parameters;
}
WeeProcessor::WeeProcessor() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    state(*this, nullptr, "WEE", layout()) {
    keyboard.addListener(this);
    for(size_t layer=0;layer<wee::layerCount;++layer) for(size_t p=0;p<wee::paramCount;++p) values[layer][p]=state.getRawParameterValue(parameterId(layer,p));
    masterGain=state.getRawParameterValue("masterGain");
}
bool WeeProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainInputChannelSet().isDisabled() && (b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || b.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}
void WeeProcessor::prepareToPlay(double sr, int) { rate = sr; rack.prepare(sr); }
void WeeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const auto start = juce::Time::getHighResolutionTicks();
    buffer.clear();
    if (loader.exchange.adopt()) rack.setSample(loader.exchange.current());
    if (auto* host = getPlayHead()) if (auto position = host->getPosition()) if (auto tempo = position->getBpm()) bpm.store(*tempo);
    std::array<wee::LayerConfig, wee::layerCount> configs;
    for(size_t layer=0;layer<wee::layerCount;++layer) for(size_t p=0;p<wee::paramCount;++p) configs[layer].v[p]=values[layer][p]->load();
    rack.configure(configs,masterGain->load(),bpm.load());
    if (panicRequested.exchange(false)) rack.allNotesOff(true);
    const auto* current = loader.exchange.current();
    const bool sampleReady = !loader.loading.load() && current && current->generation == loader.completed.load();
    auto noteOn = [&](int note, float velocity) { rack.noteOn(note,velocity,sampleReady); };
    auto noteOff = [&](int note) { rack.noteOff(note); };
    const int ready = keyQueue.getNumReady();
    int first, n1, second, n2;
    keyQueue.prepareToRead(ready, first, n1, second, n2);
    auto key = [&](int index) { const auto e = keyEvents[size_t(index)]; if (e.velocity > 0) noteOn(e.note, e.velocity); else noteOff(e.note); };
    for (int i = 0; i < n1; ++i) key(first + i);
    for (int i = 0; i < n2; ++i) key(second + i);
    keyQueue.finishedRead(n1 + n2);
    auto render = [&](int from, int to) {
        if (to > from) {
            auto* left = buffer.getWritePointer(0, from);
            auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, from) : nullptr;
            rack.render(left,right,to-from);
        }
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
    auto tree = state.copyState(); tree.setProperty("schema", 3, nullptr); return tree;
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
    state.replaceState(tree); panic(); loader.request(path); return true;
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
void WeeProcessor::loadSample(const juce::File& file, size_t layer) {
    if(layer>=wee::layerCount) return;
    const juce::ScopedLock lock(contentStateMutex);
    const auto path = file.getFullPathName();
    state.state.setProperty("samplePath", path, nullptr);
    const bool granular = values[layer][wee::Source]->load()<.5f && int(values[layer][wee::SynthMode]->load())==3;
    auto enable=[&](size_t p,float value) {auto* param=state.getParameter(parameterId(layer,p));param->beginChangeGesture();param->setValueNotifyingHost(param->convertTo0to1(value));param->endChangeGesture();};
    if(!granular) enable(wee::Source,1);
    enable(wee::Enabled,1);
    panic(); loader.request(path);
}
void WeeProcessor::clearSample() {
    const juce::ScopedLock lock(contentStateMutex);
    state.state.setProperty("samplePath", "", nullptr); panic(); loader.request("");
}
juce::AudioProcessorEditor* WeeProcessor::createEditor() { return new WeeEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WeeProcessor(); }

void WeeProcessor::handleNoteOn(juce::MidiKeyboardState*, int, int note, float velocity) {
    int first, n1, second, n2;
    keyQueue.prepareToWrite(1, first, n1, second, n2);
    if (n1) keyEvents[size_t(first)] = {note, velocity};
    else panicRequested.store(true);
    keyQueue.finishedWrite(n1);
}
void WeeProcessor::handleNoteOff(juce::MidiKeyboardState* k, int channel, int note, float) { handleNoteOn(k, channel, note, 0); }
