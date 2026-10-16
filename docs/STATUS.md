# Implementation and verification

## Prior milestones

Phase 1 Windows VST3/standalone and two suites passed in run 38850425598; Phase 2's sampler and three suites passed in run 38852200800; Phase 3's eight-layer hybrid rack and four suites passed in final run 38854504316. FL Studio/audio-device listening was not verified by those builds.

## Phase 4 implementation (retained)

- Background energy/onset detector, 50 ms refractory interval and at most 32 chops. Automatic analysis has manual corrections: add, delete, drag, split, select and numeric marker editing; keyboard navigation/preview.
- Vocal source mode on each sample layer, 16 fixed voices per vocal layer, four overlap-add heads per voice and a bounded waveform-correlation alignment search. Pitch and source-clock duration are independent. Supports MIDI root/chop mapping, velocity, sustain/bend, reverse and tempo-dependent stretching. Existing layers/filter/modulation/mixing still apply.
- Background 1024-point STFT spectral-envelope formant shaping with overlap-add reconstruction and cancellation. Optional fixed-pitch compensation approximately prewarps formants against the global Vocal pitch control. It is not a speech/voice cloning system.
- Host PPQ/transport-aware 16-step chop sequencing with divisions, rests/rearrangement, gate, velocity and length. A free clock is used when no host playhead exists. Internal sequence, MIDI and audition lanes are distinct.
- Dedicated Vocal Lab editor page, prepared waveform/chop preview, missing-file recovery, and worker-rendered whole/chop 24-bit stereo WAV export through temporary files.
- Schema 4: 400 parameters and one validated SLICES document. Schemas 1–3 migrate without enabling vocal mode or sequencing. Manual markers and settings recall in host state and `.wee` files. Source file identity lets vocal-only rebuilds preserve ordinary sample/granular voices.
- Three original synthetic-vocal presets and a CC0 procedural robotic-vowel sample, with provenance/hash verification. No human recordings or large vocal collection are supplied.

## Verification gate

Portable vocal tests cover transient locations/manual bounds, stereo/reverse, independent pitch/duration at multiple rates and buffers, note mapping and voice limits, lane isolation/sustain/release, spectral centroid movement, processing cancellation, tempo ratios, and sample-positioned sequence gates across odd callback sizes. Audio playback/sequence sections have a C++ allocation guard. Legacy synth/sampler/hybrid tests and vocal sanitizers are run before handoff; consult the reported tool/build results for actual passes.

Windows CI must build VST3/standalone and pass all five suites. Integration tests add edited-slice and schema-4 recall, background formant preparation, preview, dry WAV format/duration, free-clock sequencing and editor construction/page switching. Actual native mouse gestures, exported-audio drag/import, human listening, FL Studio/other DAWs, ASIO, pluginval and real-time CPU profiling remain unverified even if CI passes.

## Real-time design and limits

Playback voice/head pools, scratch arrays and Hann tables are fixed/prepared. The audio callback only snapshots atomic controls and swaps immutable assets; parameter listeners only store formant targets atomically. File decoding, onset detection, FFT processing, allocations, exports and retired-buffer destruction run on the worker. The existing bounded exchange and SPSC GUI-note queue remain in use. Standalone/host sequencing schedules events without building/allocating MIDI buffers. Portable guards do not audit every JUCE/host API call.

Vocal pitch/stretch controls run during playback. Formant edits prepare a new shared buffer in the background and stop/retrigger vocal voices on adoption; they are not seamless live formant automation. Compensation applies to the fixed global pitch knob only, not MIDI intervals/transpose/bend/modulation. Correlation-guided OLA can smear transients and produce phase/grain artifacts; spectral-envelope warping is approximate, with gain bounds. A recording's BPM is supplied manually, not detected. Stretch clamps to 0.25–4 including sync. Extremely short/empty trimmed chops may be silent.

The eight-layer rack still shares one external recording. Vocal global pitch/formant/stretch/map/sequence settings are shared; layer envelopes/filter/mix/tuning are independent. Other engines have 32 voices per layer; vocal has 16. LFO/expression routing remains layer/global rather than MPE. Source note range/trim and reverse are captured at note-on. The spectral waveform display shows original audio, not a rendered formant preview.

The loader caches original audio (up to 128 MiB) and can hold original plus processed buffers in current/pending/retired/working assets; worst-case memory can exceed 1 GiB per instance. There is no streaming. Formant work, repeated preparation and exports can take time for long files. Latest requests cancel stale work; use short recordings while evaluating. File export streams output blocks, caps peaks and does not render the entire layered patch, effects, or sequence arrangement.

The host tail remains 6.25 seconds. Topology changes, steals and abrupt parameter edits can click; no oversampling/full anti-alias guarantee or hard real-time CPU guarantee is claimed. Wavetables remain built-in and FM remains two-operator. The UI retains an original handheld style with readable standard fonts.

## Phase 5 implementation and validation gate

Implemented deterministic rule-based melody/chord/arp/chop generation across 14 style profiles, seven scales, key/register, reproducible seeds, four locks, complexity/feel/density/variation, and 1/2/4/8/16-bar lengths. A functional GENERATE page offers a piano roll, progression degrees, playback controls and MIDI/WAV export. Two original pattern presets demonstrate 8 and 16 bars. No external music/AI content was used.

A bounded immutable pattern exchange feeds a sample-positioned loop scheduler and a separate layered rack. Keyboard playing and the earlier vocal sequencer retain their rack; stopping a generated pattern clears only its own voices. Pattern settings and exact notes persist in schema 5; older schemas migrate to an empty stopped generator. Generation, tree serialization, sorted-event preparation and export are outside processBlock. Portable scheduler sections include an allocation guard. A second rack raises instance memory and can increase CPU significantly during generated/live overlap; it has not been profiled in a DAW.

Portable generator tests exercise 1960 style/mode/length/scale combinations, reproducibility, locks, note bounds/key membership, sample-exact timing at three rates/four buffer sizes, looping/transport, no playback allocations and independently parsed MIDI. Initial Phase 5 Windows CI run 38859529958 built VST3/standalone and passed six suites; the current revision must retain that gate. New JUCE integration tests cover schema-5 recall/rejection, factory patterns, audible isolated playback, host play/stop, readable eight-bar MIDI and audible sixteen-bar WAV with exact duration, plus failure on missing samples. Follow-up integration checks cover locked and randomized sound palettes and audible generated vocal-chop playback/export after a base-key change. Consult the Phase 5 PR/checks for actual build outcomes.

Limits: styles are compact heuristics rather than genre-complete composition systems. Fixed 4/4 only, one pattern mode at a time, at most 1024 notes, one shared external recording. Locks preserve degrees or timing with documented mode/length exceptions. Mid-loop seeks do not rebuild already-held notes. Generator controls are metadata, not automation parameters; changing them while playing can restart the pattern. Export includes the current rack, but excludes keyboard/controller performance and separate vocal sequencing. Exact-length WAV cuts late release tails and has 5 ms edge fades; no seamless-tail claim. No native drag-out, MIDI-through output, note editor, odd-meter arranger or AI provider is implemented. Future AI generation can produce the same bounded Pattern document off-thread without changing playback/export.

## Next steps

1. Run FL Studio/pluginval and actual editing/listening tests at buffers 32–1024: transport seeks/loop boundaries, 8/16-bar sync, locks/seeds, saved projects, MIDI and audio import, automation, relocation and multiple instances. These remain unverified in the build environment.
2. Profile both racks, vocal stretching and worst-case RAM; improve memory sharing/streaming, transient quality and seamless per-voice formants.
3. Phase 6: full effects rack, indexed/favorited preset browsing, complete handheld navigation, UI polish and shortcuts.
4. Phase 7: profiling, crash/glitch validation, DAW matrix, signed installer and release documentation.

The massive licensed vocal library, seamless live/polyphonic formants, separate samples per layer/multisample zones, complete effects, undo/A-B/MIDI learn, installers and release validation remain unfinished.
