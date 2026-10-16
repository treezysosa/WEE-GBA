# Presets and sample content

## Schema 3

The `.wee` root is `WEE schema="3"`, with 361 `PARAM` children: 45 controls per layer × 8, plus `masterGain`. Each parameter ID must occur exactly once. Layer 1 keeps its original IDs; layers 2–8 use prefixes `l2_` … `l8_`. `Source` retains actual values 0 synth and 1 sample, preserving old choice normalization. `synthMode` selects VA=0, wavetable=1, FM=2 or granular=3 when source=0. Definitions/ranges/defaults are centralized in Source/dsp/LayerConfig.h.

Added layer controls include enable, synth mode, balance, transpose, WT frame/mix/detune, FM ratio/index, grain position/size/density/spread, multimode filter/resonance, LFO controls and four routing triples. Routing sources: Off=0, LFO=1, last velocity=2, CC1=3, global pressure=4. Destinations: pitch=0, cutoff=1, WT position=2, amplitude=3, pan=4, FM index=5, grain position=6. Depth is bipolar -1…1. Full-scale routes represent 12 pitch semitones, four cutoff octaves, one normalized WT/grain position, one amplitude/balance unit or eight FM-index radians. Routes sum and clamp before use. Pitch/cutoff sums cap at ±24 semitones/±6 octaves. LFO divisions are 0.25, 0.5, 1, 2, 4, 8, 16 beats per cycle.

Nonfinite/out-of-range values, fractional discrete controls, invalid trim/key ranges in any layer, unknown/missing/duplicate IDs and unknown schemas are rejected before replacement/save. Schema 1's seven controls or schema 2's fourteen controls populate layer 1; all new controls use defaults, layers 2–8 are disabled, master is 1. No future schema is silently interpreted. Factory files retain older schemas to exercise migration.

## Shared sample paths

One immutable stereo asset is shared by all ordinary sample and granular layers. Root property `samplePath` stores an absolute host/fallback reference. File presets also save `sampleRelative` from the preset directory. An existing relative file takes priority; if no absolute fallback exists, a missing relative path becomes the requested absolute path so the loader can report it. Host state queues the absolute path without resolving filesystem locations on the audio callback. LOAD SAMPLE / RELOCATE manually recovers missing content; save afterward. Audio is not embedded in state.

Each layer has independent root/trim/key range, but there are no separate sample files per layer, velocity-zone maps or round robins yet. Ordinary sample playback obeys loop; granular grains wrap inside the note's trimmed span. New sample requests coalesce in the worker, with asset destruction deferred away from audio. Sample/granular notes received during loading are discarded; synth-only notes can still play.

## Demo provenance

Content/manifest.json records two original CC0 synthetic stereo samples generated reproducibly by scripts/generate-demo.py. CI verifies SHA-256, format and provenance. WAVs are build/package artifacts, not checked-in binaries. They contain no voices or third-party recordings. See Samples/LICENSE.txt. Four original Phase 3 synthesis/hybrid presets reuse these assets only where necessary; they are MIT-licensed parameter settings.

The loader handles WAV/AIFF/FLAC, mono/stereo 8–192 kHz, at most 120 seconds and 128 MiB decoded per asset. Failure publishes empty content and clears the preview. Native file browsing and MIDI audition are available; metadata indexing, library rescan, favorites and a commercial vocal collection remain unfinished.

Future recordings need verified creator/source/license/usage rights/hash and accurate language/region/consent metadata. The synthetic demos use `zxx` (no linguistic content) and no assigned region.
