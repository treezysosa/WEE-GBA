#include "PluginProcessor.h"
#include "ui/PluginEditor.h"
#include <charconv>
namespace {
bool validPatch(const juce::ValueTree& tree, juce::AudioProcessorValueTreeState& state) {
    if (!tree.hasType("WEE") || int(tree.getProperty("schema", 0)) != 1 || tree.getNumChildren() != 7) return false;
    const char* ids[] = {"gain", "attack", "decay", "sustain", "release", "cutoff", "waveform"};
    for (const auto* id : ids) {
        int found = 0;
        for (const auto& child : tree) if (child.hasType("PARAM") && child.getProperty("id").toString() == id) {
            const auto value = child.getProperty("value");
            if (!(value.isDouble() || value.isInt() || value.isInt64() || value.isString())) return false;
            // XML attributes arrive as strings. Reject nonnumeric strings explicitly.
            const auto text = value.toString();
            const auto utf8 = text.toStdString();
            float number = 0;
            const auto parsed = std::from_chars(utf8.data(), utf8.data() + utf8.size(), number);
            if (parsed.ec != std::errc{} || parsed.ptr != utf8.data() + utf8.size()) return false;
            const auto* parameter = state.getParameter(id);
            if (!std::isfinite(number) || !parameter) return false;
            const auto range = parameter->getNormalisableRange();
            if (number < range.start || number > range.end) return false;
            ++found;
        }
        if (found != 1) return false;
    }
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
    return p;
}
WeeProcessor::WeeProcessor() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    state(*this, nullptr, "WEE", layout()) {
    keyboard.addListener(this);
    const char* ids[] = {"gain", "attack", "decay", "sustain", "release", "cutoff", "waveform"};
    for (size_t i = 0; i < values.size(); ++i) values[i] = state.getRawParameterValue(ids[i]);
}
bool WeeProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainInputChannelSet().isDisabled() && (b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || b.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}
void WeeProcessor::prepareToPlay(double sr, int) { rate = sr; engine.prepare(sr); }
void WeeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const auto start = juce::Time::getHighResolutionTicks();
    buffer.clear();
    if (panicRequested.exchange(false)) engine.allNotesOff(true);
    const int ready = keyQueue.getNumReady();
    int first, n1, second, n2;
    keyQueue.prepareToRead(ready, first, n1, second, n2);
    auto key = [&](int index) { const auto e = keyEvents[size_t(index)]; if (e.velocity > 0) engine.noteOn(e.note, e.velocity); else engine.noteOff(e.note); };
    for (int i = 0; i < n1; ++i) key(first + i);
    for (int i = 0; i < n2; ++i) key(second + i);
    keyQueue.finishedRead(n1 + n2);
    wee::Parameters p {values[0]->load(), values[1]->load(), values[2]->load(), values[3]->load(), values[4]->load(), values[5]->load(), int(values[6]->load())};
    if (auto* host = getPlayHead()) if (auto position = host->getPosition()) if (auto tempo = position->getBpm()) bpm.store(*tempo);
    auto render = [&](int from, int to) {
        if (to > from) engine.render(buffer.getWritePointer(0, from), buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, from) : nullptr, to - from, p);
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
        if (kind == 0x90 && d2) engine.noteOn(d1, float(d2) / 127.0f);
        else if (kind == 0x80 || kind == 0x90) engine.noteOff(d1);
        else if (kind == 0xe0) engine.pitchWheel(d1 | (d2 << 7));
        else if (kind == 0xb0 && d1 == 64) engine.sustainPedal(d2 >= 64);
        else if (kind == 0xb0 && d1 == 120) engine.allNotesOff(true);
        else if (kind == 0xb0 && d1 == 123) engine.allNotesOff();
    }
    render(cursor, buffer.getNumSamples()); midi.clear();
    peak.store(buffer.getNumSamples() ? buffer.getMagnitude(0, buffer.getNumSamples()) : 0);
    voiceCount.store(engine.activeVoices());
    if (buffer.getNumSamples()) cpu.store(float(juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - start) * rate / buffer.getNumSamples() * 100));
}
void WeeProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto tree = state.copyState(); tree.setProperty("schema", 1, nullptr);
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml, dest);
}
void WeeProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (validPatch(tree, state)) state.replaceState(tree);
    }
}
bool WeeProcessor::savePreset(const juce::File& file) {
    auto tree = state.copyState(); tree.setProperty("schema", 1, nullptr);
    auto xml = tree.createXml();
    juce::TemporaryFile temp(file);
    return xml && temp.getFile().replaceWithText(xml->toString()) && temp.overwriteTargetFileWithTemporary();
}
bool WeeProcessor::loadPreset(const juce::File& file) {
    if (file.getSize() > 1024 * 1024) return false;
    auto xml = juce::XmlDocument::parse(file);
    if (!xml) return false;
    auto tree = juce::ValueTree::fromXml(*xml);
    if (!validPatch(tree, state)) return false;
    state.replaceState(tree); return true;
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
