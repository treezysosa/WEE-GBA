#include "PluginProcessor.h"
#include "ui/PluginEditor.h"
#include <charconv>
namespace {
constexpr const char* ids[] = {"gain", "attack", "decay", "sustain", "release", "cutoff", "waveform", "source", "root", "sampleStart", "sampleEnd", "loop", "keyLow", "keyHigh"};
bool migrateAndValidate(juce::ValueTree& tree, juce::AudioProcessorValueTreeState& state) {
    if (!tree.hasType("WEE")) return false;
    const int schema = int(tree.getProperty("schema", 0));
    if ((schema != 1 && schema != 2) || tree.getNumChildren() != (schema == 1 ? 7 : 14)) return false;
    for (int i = 0; i < (schema == 1 ? 7 : 14); ++i) {
        int found = 0;
        for (const auto& child : tree) if (child.hasType("PARAM") && child.getProperty("id").toString() == ids[i]) {
            const auto value = child.getProperty("value").toString().toStdString();
            float number = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
            auto* parameter = state.getParameter(ids[i]);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !std::isfinite(number) || !parameter) return false;
            const auto range = parameter->getNormalisableRange();
            if (number < range.start || number > range.end || (range.interval >= 1 && number != std::floor(number))) return false;
            ++found;
        }
        if (found != 1) return false;
    }
    if (schema == 1) {
        for (int i = 7; i < 14; ++i) {
            auto* param = state.getParameter(ids[i]);
            juce::ValueTree child("PARAM"); child.setProperty("id", ids[i], nullptr);
            child.setProperty("value", param->convertFrom0to1(param->getDefaultValue()), nullptr);
            tree.appendChild(child, nullptr);
        }
        tree.removeProperty("samplePath", nullptr); tree.removeProperty("sampleRelative", nullptr);
    }
    auto get = [&](const char* id) { return float(tree.getChildWithProperty("id", id).getProperty("value")); };
    if (get("sampleStart") >= get("sampleEnd") || get("keyLow") > get("keyHigh")) return false;
    if (tree.getProperty("samplePath").toString().length() > 4096 || tree.getProperty("sampleRelative").toString().length() > 4096) return false;
    tree.setProperty("schema", 2, nullptr);
    return true;
}
}
juce::AudioProcessorValueTreeState::ParameterLayout WeeProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    auto add = [&](const char* id, const char* name, float lo, float hi, float def, float skew) {
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name,
            juce::NormalisableRange<float>(lo, hi, 0.0f, skew), def));
    };
    add("gain", "Output", 0, 1, 0.22f, 1);
    add("attack", "Attack", 0.001f, 3, 0.01f, 0.3f);
    add("decay", "Decay", 0.001f, 3, 0.18f, 0.3f);
    add("sustain", "Sustain", 0, 1, 0.7f, 1);
    add("release", "Release", 0.001f, 5, 0.35f, 0.3f);
    add("cutoff", "Tone (Hz)", 20, 20000, 8000, 0.25f);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"waveform", 1}, "Oscillator", juce::StringArray{"Sine", "Saw", "Pulse"}, 0));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"source", 1}, "Sound source", juce::StringArray{"Oscillator", "Sample"}, 0));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"root", 1}, "Sample root (MIDI)", 0, 127, 60));
    add("sampleStart", "Sample start", 0, 1, 0, 1);
    add("sampleEnd", "Sample end", 0, 1, 1, 1);
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"loop", 1}, "Sample loop", false));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"keyLow", 1}, "Sample lowest MIDI key", 0, 127, 0));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"keyHigh", 1}, "Sample highest MIDI key", 0, 127, 127));
    return p;
}
WeeProcessor::WeeProcessor() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    state(*this, nullptr, "WEE", layout()) {
    keyboard.addListener(this);
    for (size_t i = 0; i < values.size(); ++i) values[i] = state.getRawParameterValue(ids[i]);
}
bool WeeProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainInputChannelSet().isDisabled() && (b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || b.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}
void WeeProcessor::prepareToPlay(double sr, int) { rate = sr; engine.prepare(sr); sampler.prepare(sr); }
void WeeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const auto start = juce::Time::getHighResolutionTicks();
    buffer.clear();
    if (loader.exchange.adopt()) sampler.setSample(loader.exchange.current());
    const int source = int(values[7]->load());
    if (source != previousSource) { engine.allNotesOff(true); sampler.allNotesOff(true); previousSource = source; }
    if (panicRequested.exchange(false)) { engine.allNotesOff(true); sampler.allNotesOff(true); }
    const wee::SampleSettings settings {int(values[8]->load()), int(values[12]->load()), int(values[13]->load()), values[9]->load(), values[10]->load(), values[11]->load() >= 0.5f};
    auto noteOn = [&](int note, float velocity) { if (source) { if (!loader.loading.load() && loader.exchange.current() && loader.exchange.current()->generation == loader.completed.load()) sampler.noteOn(note, velocity, settings); } else engine.noteOn(note, velocity); };
    auto noteOff = [&](int note) { engine.noteOff(note); sampler.noteOff(note); };
    const int ready = keyQueue.getNumReady();
    int first, n1, second, n2;
    keyQueue.prepareToRead(ready, first, n1, second, n2);
    auto key = [&](int index) { const auto e = keyEvents[size_t(index)]; if (e.velocity > 0) noteOn(e.note, e.velocity); else noteOff(e.note); };
    for (int i = 0; i < n1; ++i) key(first + i);
    for (int i = 0; i < n2; ++i) key(second + i);
    keyQueue.finishedRead(n1 + n2);
    wee::Parameters p {values[0]->load(), values[1]->load(), values[2]->load(), values[3]->load(), values[4]->load(), values[5]->load(), int(values[6]->load())};
    if (auto* host = getPlayHead()) if (auto position = host->getPosition()) if (auto tempo = position->getBpm()) bpm.store(*tempo);
    auto render = [&](int from, int to) {
        if (to > from) {
            auto* left = buffer.getWritePointer(0, from);
            auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, from) : nullptr;
            if (source) sampler.render(left, right, to - from, p); else engine.render(left, right, to - from, p);
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
        if (metadata.numBytes < 3) continue;
        const int d1 = bytes[1] & 127, d2 = bytes[2] & 127;
        if (kind == 0x90 && d2) noteOn(d1, float(d2) / 127.0f);
        else if (kind == 0x80 || kind == 0x90) noteOff(d1);
        else if (kind == 0xe0) { engine.pitchWheel(d1 | (d2 << 7)); sampler.pitchWheel(d1 | (d2 << 7)); }
        else if (kind == 0xb0 && d1 == 64) { engine.sustainPedal(d2 >= 64); sampler.sustainPedal(d2 >= 64); }
        else if (kind == 0xb0 && d1 == 120) { engine.allNotesOff(true); sampler.allNotesOff(true); }
        else if (kind == 0xb0 && d1 == 123) { engine.allNotesOff(); sampler.allNotesOff(); }
    }
    render(cursor, buffer.getNumSamples()); midi.clear();
    peak.store(buffer.getNumSamples() ? buffer.getMagnitude(0, buffer.getNumSamples()) : 0);
    voiceCount.store(source ? sampler.activeVoices() : engine.activeVoices());
    if (buffer.getNumSamples()) cpu.store(float(juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - start) * rate / buffer.getNumSamples() * 100));
}
juce::ValueTree WeeProcessor::patchState() {
    const juce::ScopedLock lock(contentStateMutex);
    auto tree = state.copyState(); tree.setProperty("schema", 2, nullptr); return tree;
}
void WeeProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = patchState().createXml()) copyXmlToBinary(*xml, dest);
}
bool WeeProcessor::applyPatch(juce::ValueTree tree, const juce::File& base) {
    const juce::ScopedLock lock(contentStateMutex);
    if (!migrateAndValidate(tree, state)) return false;
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
    if (!migrateAndValidate(tree, state)) return false;
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
void WeeProcessor::loadSample(const juce::File& file) {
    const juce::ScopedLock lock(contentStateMutex);
    const auto path = file.getFullPathName();
    state.state.setProperty("samplePath", path, nullptr);
    auto* source = state.getParameter("source");
    source->beginChangeGesture(); source->setValueNotifyingHost(1); source->endChangeGesture();
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
