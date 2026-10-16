# Implementation and verification

## Phase 1 implemented in source

- VST3 and standalone JUCE targets, C++20, CMake, PowerShell x64 workflow.
- 32-voice oscillator instrument, velocity, ADSR, polyBLEP saw/pulse, sine, one-pole low-pass, sustain, pitch bend, oldest-voice stealing and panic.
- Seven automatable parameters; schema-1 binary host state and XML patch save/load, temporary-file replacement for patch saves, async file dialogs, safe editor callback lifetime.
- Original resizable handheld UI, working patch load/save and init/panic buttons, oscillator selection, knobs, keyboard, BPM/voice/CPU/peak readouts.
- Three original synthesis presets; empty sample provenance manifest explicitly disclosing no sample content.

## Verification actually performed

GCC 14.2 C++20 Linux build and execution of portable DSP tests: passed. Covers 3 sample rates × 5 buffer sizes × 3 oscillator shapes, finite bounded stereo output, idle silence, sustained notes, release completion, 32-voice cap/stealing, panic and A4/pitch-bend frequency. AddressSanitizer/UndefinedBehaviorSanitizer also passed with leak detection disabled (LeakSanitizer cannot run under this environment’s ptrace supervision). No physical audio listening or audio-device playback occurred.

JUCE compilation and serialization tests remain unrun: CMake/JUCE/Linux audio/GUI development dependencies were unavailable and the configured proxy connection failed. Windows/MSVC, FL Studio, other DAWs, standalone device setup, ASIO, GUI accessibility/scaling and plugin validation are unverified. Do not interpret the portable test result as plugin certification.

## Real-time design and known limits

DSP voice storage is fixed, with no DSP-thread disk access or dynamic voice allocation. GUI keys use a bounded SPSC queue; overflow requests panic. UI polls atomic telemetry. Short host MIDI messages are read in place to avoid allocating for SysEx. The processor clears consumed MIDI and supports mono/stereo output. Host parameter replacement follows JUCE APVTS behavior; host tempo/parameter APIs and the JUCE wrapper still need a profiler/allocation audit before a hard real-time guarantee. GUI keyboard producers must remain on the message thread. This is not MPE; pitch bend and sustain are global across channels. Same-pitch note-off releases all matching held voices.

Filter cutoff and ADSR automation are block-rate and not smoothed; abrupt edits and voice stealing can click. Output tanh bounds peaks but is not a mastering limiter; nonlinear harmonics can alias. No oversampling or SIMD optimization has been profiled. The UI uses standard JUCE typography, not a bundled pixel font, and no animated waveform or D-pad is exposed. Preset file errors are reported; process crashes during file replacement are not yet fault-injection tested. Version 1 is the only supported schema; future migrations are not implemented.

## Next practical milestones

1. Obtain JUCE 8.0.6 and CMake, run the complete Debug/Release builds and state tests on Windows. Fix any integration errors; run pluginval and the FL Studio checklist in README before distributing binaries.
2. Phase 2: introduce immutable prepared stereo sample assets, a background loader and bounded handoff with deferred reclamation; then key/velocity zones, start/end/loops, multisample presets and missing-file recovery. Add legal synthetic demo audio or licensed recordings with full provenance. Verify repeated loads under sustained MIDI and offline bounce without callback allocations.
3. Phase 3: extend layer/voice architecture, wavetable, modulation and filters; add FM/granular only with working tests and exposed controls.
4. Phases 4–7: vocal DSP, deterministic generator/export, effects, full library/navigation, profiling, installers and release validation.

All sampling, vocals, time stretching, formant processing, granular/FM/additive engines, eight-layer patches, generation, MIDI/audio export, modulation matrix, MIDI learn, aftertouch/expression routing, effects rack, undo/redo, A/B, favorites, library indexing, sample drag/drop, installers and commercial sample collection remain unfinished. No controls claim to implement them.
