#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace wee {
struct Parameters {
    float gain = 0.22f, attack = 0.01f, decay = 0.18f, sustain = 0.7f, release = 0.35f;
    float cutoff = 8000.0f;
    int waveform = 0;
    bool bypassTone = false;
};
// Fixed voice pool; all storage and preparation are outside the callback.
class Engine {
    struct Voice {
        int note = -1; float velocity = 0, envelope = 0; double phase = 0, increment = 0;
        float filter = 0; int stage = 0; bool held = false, sustained = false;
        uint64_t age = 0;
    };
    std::array<Voice, 32> voices {};
    double sampleRate = 44100, bend = 1, modulationPitch = 1;
    uint64_t clock = 0;
    bool pedal = false;
    float currentGain = 0;
    static float blep(double t, double dt) noexcept {
        if (t < dt) { t /= dt; return float(t + t - t*t - 1); }
        if (t > 1 - dt) { t = (t - 1) / dt; return float(t*t + t + t + 1); }
        return 0;
    }
public:
    void prepare(double rate) noexcept { sampleRate = std::max(8000.0, rate); reset(); }
    void reset() noexcept { voices = {}; bend = 1; modulationPitch = 1; pedal = false; clock = 0; currentGain = 0; }
    void pitchWheel(int value) noexcept { bend = std::pow(2.0, ((value - 8192) / 8192.0) * 2.0 / 12.0); }
    void sustainPedal(bool down) noexcept {
        pedal = down;
        if (!down) for (auto& v : voices) if (v.sustained && !v.held) { v.stage = 4; v.sustained = false; }
    }
    void noteOn(int note, float velocity, int transpose = 0) noexcept {
        if (velocity <= 0) { noteOff(note); return; }
        Voice* target = nullptr;
        for (auto& v : voices) if (v.stage == 0) { target = &v; break; }
        if (!target) target = &*std::min_element(voices.begin(), voices.end(), [](const auto& a, const auto& b) { return a.age < b.age; });
        *target = {}; target->note = note; target->velocity = velocity;
        target->increment = 440.0 * std::pow(2.0, (note + transpose - 69) / 12.0) / sampleRate;
        target->stage = 1; target->held = true; target->age = ++clock;
    }
    void noteOff(int note) noexcept {
        for (auto& v : voices) if (v.note == note && v.held) {
            v.held = false; v.sustained = pedal; if (!pedal) v.stage = 4;
        }
    }
    void allNotesOff(bool immediate = false) noexcept {
        pedal = false;
        for (auto& v : voices) { v.held = v.sustained = false; v.stage = immediate ? 0 : 4; }
    }
    void pitchMultiplier(double multiplier) noexcept { modulationPitch = multiplier; }
    int activeVoices() const noexcept { int n = 0; for (const auto& v : voices) n += v.stage != 0; return n; }
    void render(float* left, float* right, int count, const Parameters& p) noexcept {
        const auto a = float(1 / (sampleRate * std::max(0.001f, p.attack)));
        const auto d = float((1 - p.sustain) / (sampleRate * std::max(0.001f, p.decay)));
        const auto r = float(1 / (sampleRate * std::max(0.001f, p.release)));
        const auto fc = 1.0f - std::exp(float(-6.28318530718 * std::clamp(double(p.cutoff), 20.0, sampleRate * 0.45) / sampleRate));
        const float smooth = 1 - std::exp(float(-1 / (sampleRate * 0.005)));
        for (int i = 0; i < count; ++i) {
            float sum = 0;
            for (auto& v : voices) {
                if (!v.stage) continue;
                if (v.stage == 1) { v.envelope += a; if (v.envelope >= 1) { v.envelope = 1; v.stage = 2; } }
                else if (v.stage == 2) { v.envelope -= d; if (v.envelope <= p.sustain) { v.envelope = p.sustain; v.stage = 3; } }
                else if (v.stage == 3) v.envelope = p.sustain;
                else { v.envelope -= r; if (v.envelope <= 0) { v.envelope = 0; v.stage = 0; continue; } }
                const double dt = std::min(0.45, v.increment * bend * modulationPitch);
                float s = 0;
                if (p.waveform == 0) s = float(std::sin(6.28318530718 * v.phase));
                else if (p.waveform == 1) s = float(2 * v.phase - 1) - blep(v.phase, dt);
                else s = (v.phase < 0.5 ? 1.0f : -1.0f) + blep(v.phase, dt) - blep(std::fmod(v.phase + 0.5, 1.0), dt);
                v.phase += dt; if (v.phase >= 1) v.phase -= 1;
                if (p.bypassTone) v.filter = s; else v.filter += fc * (s - v.filter);
                sum += v.filter * v.envelope * v.velocity;
            }
            currentGain += smooth * (p.gain - currentGain);
            const float out = std::tanh(sum * currentGain * 0.25f);
            left[i] += out; if (right) right[i] += out;
        }
    }
};
}
