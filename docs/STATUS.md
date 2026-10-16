# Implementation and verification

## Phase 1

Windows x64 VST3 and standalone builds succeeded in GitHub Actions run 38850425598. The DSP and preset-state CTest suites both passed. Actual GUI operation, audio-device output, FL Studio playback and plugin validation were not performed by that run.

## Phase 2 implementation

- Fixed 32-voice stereo sampler: linear interpolation, MIDI root/key-range mapping, velocity, ADSR, sustain, ±2-semitone bend, oldest-voice stealing and panic.
- Sample start/end, looping of the trimmed region, one-shot boundary fades, tone filtering, source selection and mono output fold-down.
- Dedicated background WAV/AIFF/FLAC decoder with size/duration limits, nonfinite-data sanitization, waveform preview and readable loading/missing/corrupt-file errors.
- Fixed-size pointer exchange: audio swaps prepared assets at block boundaries, kills old sample voices and retires the old buffer; worker performs all asset destruction. Pending replacements are coalesced; no shared_ptr destruction or locking in the audio handoff.
- Native sample file browser, drag/drop audio or presets, onscreen-key audition, visible mapping/trim controls, clear/relocate and functional source switching. No decorative inactive tabs are exposed.
- Schema-2 host state and presets, schema-1 migration, relative/fallback absolute sample paths, manual missing-file recovery and temporary-file saves.
- Two original CC0 synthesized stereo demos with reproducible generation and a provenance manifest, alongside three original synthesis presets.

## Verification

Phase 2 portable sampler tests pass under GCC 14.2 and AddressSanitizer/UndefinedBehaviorSanitizer (leak detection disabled because this environment runs under ptrace). They cover 3 rates × 5 buffers, stereo independence, pitch/root/octave, looping, release/sustain, key ranges, trim, silence on missing asset, voice limits, 20,000 concurrent publications and audio-thread allocation/asset-destruction guards. Phase 1 DSP tests still pass. Content generation and provenance verification pass locally.

The Windows CI run for Phase 2 is the remaining verification gate at the time of writing. It compiles VST3, standalone and tests and adds integration checks for asynchronous decode, sample state recall, schema migration, moved-folder recovery, missing/corrupt files, rapid replacement and mono AIFF/FLAC playback. Consult the Actions result for this commit. FL Studio/other DAWs, human listening, standalone device configuration, ASIO and GUI interaction remain unverified even after CI passes.

## Real-time behavior and limits

No sampler disk I/O, allocation, blocking locks or asset deletion occur in processBlock. Decoder requests/status and preset I/O use non-audio threads. A two-slot atomic pointer exchange bounds audio-side work; a loaded sample is immutable. GUI keys use a bounded SPSC queue; overflow requests panic. Short MIDI messages are read in-place. The JUCE wrapper/APVTS/host APIs still need whole-plugin profiling before a strict hard-real-time guarantee. Tests guard the portable engine and exchange, not every host/wrapper call.

A sample is fully resident in RAM, not disk-streamed; one asset is mapped across one key range per patch. Simultaneous current/pending/retired/decoding buffers can temporarily use several times the per-asset limit. Old voices stop on content change. Playback or an offline bounce should begin only after loading completes; MIDI notes during loading are discarded. Files are not embedded in host projects; collect them with your project. Controls for root, key range, trim and loop apply at note-on. Invalid trim/key ranges are silent. Channels share global bend/sustain; no MPE. Same-pitch note-off releases all matching held voices.

Loop boundaries have no automatic crossfade; choose matching edges. Asset switching, stealing, envelope edits and block-rate filter edits can click. Large sample pitch shifts can alias; no formant preservation or time stretching. Output soft clipping is not a mastering limiter. No streaming/SIMD/oversampling optimization is claimed. GUI is resizable and uses standard readable JUCE fonts; it has no complete seven-page product navigation yet.

## Next milestones

1. Verify the Phase 2 Windows build/tests, then run the documented FL Studio and pluginval checks with imported stereo and long samples. Test sample relocation, rapid replacement while holding notes, save/reopen, offline bounce, multiple instances and buffers 32–1024.
2. Extend sampling to multiple key/velocity zones, round robin, release samples, crossfade loops and persistent user-library indexing; profile memory/loading before expanding the library.
3. Phase 3: wavetable, multilayer architecture, filter/LFO/modulation matrix, FM and granular foundations with DSP tests.
4. Later: vocal slicing/pitch/formant/time stretching, deterministic generation/export, complete effects/UI, profiling, signed installers and release validation.

Multisample mapping, reverse, time stretching, vocal processing, eight layers, generator/export, modulation matrix, MIDI learn, expression/aftertouch routing, full effects, undo/redo, A/B, favorites/search, installers and the large commercial sample collection remain unfinished.
