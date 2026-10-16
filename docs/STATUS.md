# Implementation and verification

## Prior verified milestones

Phase 1 Windows x64 VST3/standalone and two tests passed in Actions run 38850425598. Phase 2's stereo sampler, loader and sample-state integration passed all three tests in final run 38852200800. Neither run verified FL Studio/audio-device playback or human UI operation.

## Phase 3 implementation

- Eight independently enabled layers, each with 32 voices, ADSR, level/balance, transpose, key range and its own source/engine. Total cap: 256 layer voices. Engine changes/disable stop only that layer; note-off retains original MIDI identity after transpose changes.
- Original four-frame wavetable bank (sine/triangle/saw/pulse), 1024 samples per frame, ten Fourier harmonic-limit levels. Tables/Hann windows are prepared before callbacks, with harmonic levels selected per voice/control chunk. Two detuned WT oscillators per voice with an adjustable blend.
- Two-operator sine phase-modulation FM foundation with ratio and modulation index. This is not a full multi-operator FM workstation.
- Four-head granular sample engine per voice: interpolated stereo reads, precomputed Hann windows, adjustable position/size/density/spread, deterministic per-voice PRNG, keyboard pitch/root and ADSR. Grains wrap within the trimmed span; four-head saturation drops extra spawn requests.
- Per-layer legacy one-pole or resonant TPT state-variable low/high/band-pass filters. One sine/triangle/square LFO per layer, Hz or beat-cycle tempo sync.
- Four modulation routes per layer. Sources: LFO, last note velocity, CC1, global pressure. Destinations: pitch, cutoff, WT position, amplitude, balance, FM index, grain position. MIDI channel/poly pressure are treated as one expression signal; this is not MPE/per-note expression.
- Working Synth, Sample/Granular, Modulation and Layer Mixer pages, scrollable controls, eight enable switches, layer selector and previous/next buttons. Inapplicable engine controls disable. Master level smooths; summed output clamps at ±1.
- Schema 3 stores 361 controls, shared sample references, and migrates schemas 1/2 to layer 1 with other layers disabled. Four original factory patches demonstrate WT motion, mixed engines, all eight layers and granular texture.

## Validation and current gate

Local portable tests cover VA/sample regressions plus 75 engine/rate/buffer combinations for the rack (5 sources × 3 sample rates × 5 block sizes). Additional checks cover WT tuning/harmonic limits, FM spectral changes, granular silence/stereo/determinism, 256-voice stealing, independent layer disable, pan, expression routing, original-key release after transpose edits, LFO speed/phase and filter DC response/stability. The test audio sections contain an allocation guard. The sampler exchange still passes the 20,000-publication lifetime stress test. Hybrid AddressSanitizer/UndefinedBehaviorSanitizer tests also passed, with leak detection disabled because this environment runs under ptrace. Windows integration is the remaining build gate.

The Windows workflow must compile VST3/standalone and pass four CTest suites. Extended state tests cover schema 1/2/3 migrations, layer-8 recall, new factory patches, sample-positioned MIDI and two-byte pressure routing, in addition to Phase 2 decoding/relocation/corrupt-file checks. Consult the latest Phase 3 PR Actions result for actual outcome. FL Studio/other DAWs, physical audio, GUI interaction, pluginval, ASIO and hard real-time performance remain unverified even after CI passes.

## Real-time design and boundaries

Voice/grain pools, tables, modulation arrays and 16-sample scratch buffers are fixed/prepared. processBlock snapshots atomic parameters once per callback; routes/filter coefficients update in chunks of at most 16 samples. Short MIDI is read in place, and GUI notes use a bounded SPSC queue. There is no sample I/O, buffer allocation/destruction or blocking loader lock in the callback. Wavetable preparation and all decoding/reclamation run outside playback. Tests guard portable engines; JUCE wrapper/APVTS/host APIs need complete profiling before a hard-real-time guarantee.

Sample/granular layers share one recording; each has independent root/trim settings captured at note-on. Sample assets are RAM-resident and external to presets. Pending/current/retired/decoding assets can temporarily use several times the 128-MiB per-asset cap. Wait for loading before offline bounce; notes needing an unfinished asset are dropped. File replacement stops old sample/grain voices. Collect recordings with DAW projects.

LFOs run freely and follow BPM for speed, without host-song phase alignment. Last-velocity and pressure routes are layer/global signals, not per-voice modulation. There is no modulation envelope matrix source yet; ADSR controls each voice's amplitude. Single sample interpolation and high-index FM can alias; wavetable harmonic-level transitions, source swaps, steals and abrupt edits can click. There is no oversampling or click-free topology crossfade. The final output clamp protects bounds but is not a mastering limiter. CPU at 256 complex voices is unprofiled; reduce layers/polyphony while evaluating. No GPU/SIMD optimization is claimed.

## Next steps

1. Confirm the Phase 3 Windows build/tests and run FL Studio/pluginval, automation, resize, all-engine listening, save/reopen and offline-bounce checks with multiple instances and buffers 32–1024.
2. Improve modulation smoothing, mip-level transitions and measured CPU hotspots. Extend sample assets to per-layer/multisample zones, crossfade loops and searchable content indexing.
3. Phase 4: transient/slice detection and editing, pitch/formant/time-stretch algorithms, chop sequencing and tempo synchronization, with tests and honest algorithm/latency limits.
4. Later phases: deterministic loop generation/export, full effect rack/library/navigation, profiling and signed installer/release validation.

External WT import, higher-operator FM, full granular time stretching, separate sample files per layer, multisample/velocity zones, reverse/vocal processing, full effects, MIDI learn, undo/redo, A/B, favorites/indexing, generators/export, installers and the large vocal library remain unfinished.
