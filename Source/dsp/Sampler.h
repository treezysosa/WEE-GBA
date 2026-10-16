#pragma once
#include "Engine.h"
#include <vector>
namespace wee {
// Immutable after publication. Only the loader thread allocates/frees these buffers.
struct SampleData {
    std::vector<float> left, right;
    double rate = 44100;
    uint64_t generation = 0;
    bool valid() const noexcept { return left.size() >= 2 && left.size() == right.size() && rate > 0; }
};
struct SampleSettings {
    int root = 60, low = 0, high = 127;
    float start = 0, end = 1;
    bool loop = false;
    int transpose = 0;
};
class Sampler {
    struct Voice {
        int note = 0, stage = 0;
        bool held = false, sustained = false, loop = false;
        float velocity = 0, envelope = 0, filterL = 0, filterR = 0;
        double position = 0, increment = 1, begin = 0, end = 0;
        uint64_t age = 0;
    };
    std::array<Voice, 32> voices {};
    const SampleData* sample = nullptr;
    double rate = 44100, bend = 1, modulationPitch = 1;
    bool pedal = false;
    uint64_t age = 0;
    float gain = 0;
public:
    void prepare(double sr) noexcept { rate = std::max(8000.0, sr); reset(); }
    void reset() noexcept { voices = {}; bend = 1; modulationPitch = 1; pedal = false; gain = 0; age = 0; }
    void setSample(const SampleData* asset) noexcept { reset(); sample = asset; }
    void pitchWheel(int wheel) noexcept { bend = std::pow(2.0, (wheel - 8192) / 8192.0 * 2.0 / 12.0); }
    void sustainPedal(bool down) noexcept {
        pedal = down;
        if (!down) for (auto& v : voices) if (v.sustained && !v.held) { v.sustained = false; v.stage = 4; }
    }
    void noteOn(int note, float velocity, const SampleSettings& s) noexcept {
        if (velocity <= 0) { noteOff(note); return; }
        if (!sample || !sample->valid() || note < s.low || note > s.high) return;
        const double length = double(sample->left.size());
        const double first = std::floor(std::clamp(double(s.start), 0.0, 1.0) * (length - 1));
        const double last = std::floor(std::clamp(double(s.end), 0.0, 1.0) * length);
        if (last - first < 2) return; // Invalid trim produces silence; never reads outside asset.
        Voice* target = nullptr;
        for (auto& v : voices) if (!v.stage) { target = &v; break; }
        if (!target) target = &*std::min_element(voices.begin(), voices.end(), [](auto& a, auto& b) { return a.age < b.age; });
        *target = {}; target->note = note; target->stage = 1; target->held = true;
        target->velocity = std::clamp(velocity, 0.0f, 1.0f); target->age = ++age;
        target->position = target->begin = first; target->end = last; target->loop = s.loop;
        target->increment = sample->rate / rate * std::pow(2.0, (note + s.transpose - s.root) / 12.0);
    }
    void noteOff(int note) noexcept {
        for (auto& v : voices) if (v.note == note && v.held) { v.held = false; v.sustained = pedal; if (!pedal) v.stage = 4; }
    }
    void allNotesOff(bool immediate = false) noexcept {
        pedal = false;
        for (auto& v : voices) { v.held = v.sustained = false; v.stage = immediate ? 0 : 4; }
    }
    void pitchMultiplier(double multiplier) noexcept { modulationPitch = multiplier; }
    int activeVoices() const noexcept { int n = 0; for (const auto& v : voices) n += v.stage != 0; return n; }
    void render(float* left, float* right, int count, const Parameters& p) noexcept {
        if (!sample || !sample->valid()) return;
        const float a = float(1 / (rate * std::max(0.001f, p.attack)));
        const float d = float((1 - p.sustain) / (rate * std::max(0.001f, p.decay)));
        const float r = float(1 / (rate * std::max(0.001f, p.release)));
        const float fc = 1 - std::exp(float(-6.28318530718 * std::clamp(double(p.cutoff), 20.0, rate * 0.45) / rate));
        const float smooth = 1 - std::exp(float(-1 / (rate * 0.005)));
        for (int i = 0; i < count; ++i) {
            float sumL = 0, sumR = 0;
            for (auto& v : voices) {
                if (!v.stage) continue;
                if (v.position >= v.end) {
                    if (v.loop) v.position = v.begin + std::fmod(v.position - v.begin, v.end - v.begin);
                    else { v.stage = 0; continue; }
                }
                if (v.stage == 1) { v.envelope += a; if (v.envelope >= 1) { v.envelope = 1; v.stage = 2; } }
                else if (v.stage == 2) { v.envelope -= d; if (v.envelope <= p.sustain) { v.envelope = p.sustain; v.stage = 3; } }
                else if (v.stage == 3) v.envelope = p.sustain;
                else { v.envelope -= r; if (v.envelope <= 0) { v.stage = 0; continue; } }
                const auto index = size_t(v.position);
                const float fraction = float(v.position - double(index));
                const auto next = double(index + 1) < v.end ? index + 1 : (v.loop ? size_t(v.begin) : index);
                auto read = [&](const std::vector<float>& data) { return data[index] + fraction * (data[next] - data[index]); };
                if (p.bypassTone) { v.filterL = read(sample->left); v.filterR = read(sample->right); }
                else { v.filterL += fc * (read(sample->left) - v.filterL); v.filterR += fc * (read(sample->right) - v.filterR); }
                // Short boundary fades reduce one-shot trim clicks. Loops use raw boundaries.
                const float edge = v.loop ? 1.0f : float(std::clamp(std::min(v.position - v.begin, v.end - v.position) / std::max(1.0, sample->rate * 0.002), 0.0, 1.0));
                const float amplitude = v.envelope * v.velocity * edge;
                sumL += v.filterL * amplitude; sumR += v.filterR * amplitude;
                v.position += v.increment * bend * modulationPitch;
            }
            gain += smooth * (p.gain - gain);
            if (right) { left[i] += std::tanh(sumL * gain * 0.5f); right[i] += std::tanh(sumR * gain * 0.5f); }
            else left[i] += std::tanh((sumL + sumR) * 0.5f * gain * 0.5f);
        }
    }
};
}
