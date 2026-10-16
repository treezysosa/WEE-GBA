# WEE — Phase 5 melody and loop generator (0.5.0)

An original C++20/JUCE Windows x64 VST3 and standalone instrument with a handheld-console interface. Eight layers combine virtual analog, sample playback, wavetable, two-operator FM and granular foundations. No proprietary instrument code, branded console assets, scraped recordings or voice impersonations are included.

**Verification:** Phase 5 Windows x64 VST3/standalone and all six test suites passed in run 38859529958. Consult the Phase 5 PR checks for the current revision. Local portable suites and generator address/undefined-behavior sanitizers pass. Actual FL Studio playback, human listening, native GUI interaction and pluginval remain unverified.

## Build on Windows

Install Visual Studio 2022 with **Desktop development with C++**, MSVC v143 and Windows 10/11 SDK. Install CMake 3.22+, Git and Python 3 on PATH. The build targets x64 and pins JUCE 8.0.6. JUCE can be downloaded by CMake or supplied as a local checkout for offline builds.

```powershell
./scripts/build-windows.ps1 -Configuration Debug
./scripts/build-windows.ps1 -Configuration Release
# Optional install; use an elevated PowerShell if permissions require it:
./scripts/build-windows.ps1 -Configuration Release -Install
# Local JUCE checkout:
./scripts/build-windows.ps1 -JucePath C:/Dependencies/JUCE
```

Equivalent manual commands:

```powershell
python scripts/generate-demo.py
python scripts/verify-content.py
cmake -S . -B build/windows -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build/windows --config Release --parallel 2
ctest --test-dir build/windows -C Release --output-on-failure
```

Outputs: `build/windows/WEE_artefacts/Release/VST3/WEE.vst3` (a directory bundle) and `build/windows/WEE_artefacts/Release/Standalone/WEE.exe`. Copy the **whole** VST3 directory into `C:/Program Files/Common Files/VST3/`. Close DAWs before updating. Installation is optional; normal builds do not write to system directories.

The standalone's JUCE audio/MIDI settings dialog selects devices and buffers. Optional ASIO requires a separately obtained SDK and CMake `-DWEE_ENABLE_ASIO=ON -DWEE_ASIO_SDK_PATH=<sdk-root>`. Review its license before distribution; ASIO has not been tested here. Default Windows audio does not need that SDK. JUCE licensing is described in LICENSE.md.

## Play and edit layers

Enable layers with switches **1–8** above the display. Only layer 1 is enabled in an initialized patch. The LAYER menu and side **< LAYER / LAYER >** buttons choose which layer to edit; selection does not mute other layers. All enabled layers receive MIDI, limited by their own LOW/HIGH key range. Every layer has its own 32-voice pool (up to 256 rack voices), ADSR, level, stereo balance and transpose. **A / INIT** clears the shared sample and resets the whole patch; **B / PANIC** stops all voices. The onscreen keyboard plays notes; controls are keyboard focusable. Scroll the central panel to reach additional controls.

- **SYNTH:** choose Sound source = Synth and select Virtual analog, Wavetable, FM or Granular. VA has sine/saw/pulse. Wavetable morphs across four original spectral frames (sine, triangle, saw and pulse), with pitch-appropriate harmonic levels and an optional second detuned oscillator. FM has carrier/modulator ratio and modulation index. The filter menu selects legacy low-pass or resonant low/high/band-pass. Engine-specific controls are disabled when inapplicable.
- **SAMPLE / GRANULAR:** import WAV/AIFF/FLAC or drop a file onto the plugin. Loading enables the selected layer and selects sample playback, except when that layer is already in granular mode. All sample/granular layers share this single recording; each has separate root, trim and key mapping. Wait for the loading message to finish before playing or bouncing. Start/end are normalized 0–1; root uses MIDI numbers (60 by default). Sample LOOP repeats the trimmed region; granular mode instead creates Hann-windowed grains with position, size, density and spread. Root/trim/transpose are captured at note-on; grain controls affect newly spawned grains.
- **MODULATION:** one free-running LFO per layer, with sine/triangle/square, Hz or tempo-linked beat divisions. Four routes connect LFO, last accepted velocity, mod wheel (CC1) or pressure to pitch, cutoff, WT position, amplitude, pan, FM index or grain position. Set source, destination and a signed depth; Off is neutral. Engine-specific destinations act only on their respective engines. Pressure is a global expression signal, not per-voice MPE.
- **LAYER MIXER:** all eight levels and balances plus a smoothed global master level. Disabled layers can be edited before enabling them. Lower layer levels if stacked notes reach the output clamp.

The plugin reports a 6.25-second release/filter tail to the host. The display reports host BPM (120 fallback), total/selected-layer voices, approximate callback CPU load and last-block peak. LFO tempo sync sets cycle speed; it does not align phase to host song position. Pitch bend is ±2 semitones; CC64 sustains notes. A source/engine change or layer disable stops that layer's current voices. Changes to transpose still release correctly using the original MIDI key.

## Presets and samples

LOAD/SAVE PATCH uses `.wee` XML files; host project state stores parameters and the shared sample path. New schema 5 preserves 400 parameters, slice metadata and a versioned generator document. Phase 1/2 presets migrate into layer 1; Phase 3 retains all eight layers, with vocal mode initially off. Sample presets store a relative path for moved content folders plus an absolute fallback. Missing/corrupt samples are silent and show an error; LOAD SAMPLE / RELOCATE repairs the reference. Save the patch again afterward. Audio is external, not embedded in projects, so archive your recordings with your project.

Factory patches include **Prism Pocket** (wavetable motion), **Dual Orbit** (three-layer WT/FM/VA), **Eightfold Nebula** (eight-layer pad) and **Grain Lantern** (granular + WT), plus the five earlier synth/sample patches. Three original CC0 synthetic stereo recordings are generated by `scripts/generate-demo.py` and verified against a provenance manifest. The build script/CI generate them automatically; no vocal collection is bundled. Keep `Content/Presets` and `Content/Samples` together.

## GitHub Actions download

The Windows workflow runs on pushes to main, pull requests and manual dispatch. It builds Release, runs five CTest suites and verifies demo content. Successful runs upload **WEE-Windows-x64** for 14 days. Download that artifact, extract it, then extract the enclosed `WEE-Windows-x64.zip`. It contains `WEE.vst3`, `WEE.exe`, Content, documentation and licenses. A successful CI build is not FL Studio certification or a signed installer.

## FL Studio checklist — requires local testing

1. Install the complete x64 VST3 bundle in the standard directory.
2. Open **Options → Manage plugins**, enable verification and **Find installed plugins**.
3. Add WEE to the Channel Rack. Test all engines, eight-layer patches, velocity, sustain, bend, CC1 and pressure where the host supports it.
4. Automate layer parameters, change editor pages/layers, save/reopen a project, and verify settings and sample paths.
5. Test sample replacement while playing, missing-file relocation and an offline bounce after loading finishes.
6. Try multiple instances, 44.1/48/96 kHz and buffers 32–1024. Run pluginval/VST3 validation before a release.

If not found, check x64 Release and the entire `.vst3` folder, then rescan failed plugins. If silent, check enabled layers, key range, source/sample availability, output level, trim and modulation routes (an amplitude route can intentionally silence a layer). If a sample is missing, relocate it. Crackles require profiling; reduce layers/voices while diagnosing. If JUCE download fails, provide a local checkout; do not disable TLS verification.

## Portable tests and limitations

CMake core-only tests do not require JUCE:

```sh
cmake -S . -B build/core -DWEE_CORE_ONLY=ON
cmake --build build/core
ctest --test-dir build/core --output-on-failure
```

Without CMake, compile the three standalone tests with C++20 and `-I Source`; add `-pthread` for SamplerTests.cpp. Example: `g++ -std=c++20 -O2 -I Source tests/HybridTests.cpp -o /tmp/wee_hybrid_tests`. Full JUCE builds also run host/preset and decoder integration tests.

This is an incremental instrument. Wavetables are built-in, FM is two-operator, granular uses four fixed heads per voice, and eight layers share one RAM sample asset. Multisample libraries and streaming remain unfinished. The Phase 4 algorithms and their limits are described below. Complex patches have not been profiled for real-time DAW CPU limits. See docs/STATUS.md and docs/CONTENT.md for exact boundaries and next steps.

## Vocal Lab — Phase 4

Select **VOCAL LAB** and load a recording (or load **Neon Chop Grid**). Importing from this page enables the selected layer’s sample/vocal mode. **USE VOCAL LAYER** enables it for an already loaded file. Vocal mode uses 16 voices per layer; the earlier engines keep their 32-voice pools. One recording and the vocal processing settings are shared across vocal layers, while each layer retains its envelope/filter/level/trim/transpose.

Wait for preparation to finish. **DETECT** runs adaptive onset analysis using the displayed sensitivity. Up to 32 slices map consecutively from the First chop MIDI key (60 default); **Map slices to MIDI keys** off instead plays the entire layer trim, pitched from its root. The detector is heuristic: use the manual controls to correct missed or extra boundaries.

Click the waveform to select a chop. Double-click to add a boundary, drag an internal amber line to move it, or right-click/Delete to remove one. SPLIT divides the selected chop; REMOVE merges at its left boundary (or first interior boundary). Arrow keys change selection; Space or PREVIEW auditions it. The two numeric fields below the waveform edit normalized start/end positions to six decimal places; outer endpoints remain 0 and 1. Manual edits persist in `.wee` files and host state. Reloading the same source or relocating a missing file preserves manual boundaries. STOP ends audition and sequencing; B / PANIC also stops the sequencer.

**Pitch** and **duration multiplier** are independent during playback. The engine uses four Hann-windowed overlap-add read heads per voice with a bounded waveform-correlation search; 1.5 means approximately 50% longer without lowering the carrier pitch. **Stretch to host BPM** multiplies duration by recording BPM / host BPM; the final duration multiplier clamps to 0.25–4. Reverse is captured at note-on. This is a sample-based algorithm, not live microphone processing; extreme shifts, transients and polyphonic recordings can sound grainy/phasey. No zero-artifact guarantee is claimed.

**Formant shift** runs a background 1024-point spectral-envelope warp. **Compensate fixed-pitch formants** prewarps by formant shift minus the fixed Vocal pitch knob, approximately compensating the envelope movement from that pitch change. It does not compensate additional keyboard intervals, layer transpose, bend or pitch modulation. Wait for the loading message and retrigger after formant edits: the new vocal buffer replaces old vocal voices. This is not seamless real-time formant automation. Ordinary sample/granular voices retain their original audio on vocal-only rebuilds.

Scroll down for the **16-step chop sequencer**. Set Run chop sequencer, step count, division, gate and velocity. Each step is 0 for rest or 1–32 for a slice; change those numbers to rearrange chops. Missing slice numbers are silent. In a DAW, it follows transport and PPQ when available, at sample-positioned boundaries; it stops on transport stop and clears its own voice lane on a seek. Without a host playhead it uses a free clock. MIDI playing, sequencing and preview use separate vocal lanes. Enable a vocal layer first. **Neon Chop Sequence** intentionally enables this pattern; other initialized patches do not autoplay.

**EXPORT CLIP / EXPORT CHOP** render a 24-bit stereo WAV on the worker thread using current pitch/stretch/BPM/reverse/formant settings and the selected slice if requested. The export is dry processing with short fades, excluding layer transpose, ADSR/filter/mixer and sequence arrangement; peaks above full scale clamp. Source audio is not embedded in presets. Use recordings you are authorized to process. MIDI/arrangement export and native DAW drag-out remain later work; exported WAVs can be imported normally into FL Studio.

**Neon Syllables.wav** is an original procedural robotic-vowel texture, not a human voice, celebrity imitation, language performance or commercial vocal library. It and the two earlier synthetic recordings are CC0 with hashes/provenance in Content/manifest.json. Three new presets demonstrate mapped chops, a tempo sequence and a stretched texture.


## Generate — Phase 5

Open **GENERATE**, choose a style, output mode, key/scale and length, then press **GENERATE**. The result is an original deterministic rule-based pattern. Styles cover Pop, Dark pop, Indie pop, Soul, R&B, Trap, Lo-fi, City pop, Ambient, Horror, Retro video game, Cinematic, Rock and Dream pop. They bias rhythm spacing, gates and scale-degree progressions; they do not emulate artists or use recorded songs. Melody, Chords, Arpeggio and Vocal chops are separate output modes. Chords stack three or four scale tones every other degree, once per bar. A progression-degree list and piano roll show the saved result.

Lengths are 1, 2, 4, 8 or 16 bars in **4/4**, regardless of the host's time signature. Major, natural minor, Dorian, Mixolydian, minor pentatonic, harmonic minor and Phrygian scales are available. Root and octave set the register. Complexity changes subdivision, chord size and pitch choice; Density changes non-downbeat rests; Humanization affects onset and velocity; Swing delays offbeat sixteenths; Variation changes scale-degree movement, chop repetition and longer progressions. Some controls have a smaller effect in chord mode, which always places a chord per bar. Settings changes do not replace notes until generation; changing settings while playing re-arms the current pattern at the host phase (or standalone beginning).

**GENERATE** reuses the current seed; **NEXT SEED** increments it; **RANDOMIZE** increments the seed and chooses new style/complexity/density/variation/feel settings. This is local and offline. Version-1 generator arithmetic uses fixed integer random streams, with no dependency on a cloud service or `std::uniform_distribution`. Identical settings/seed and identical lock context reproduce results. Both the settings and exact saved notes are stored, so future algorithm changes need not alter old patches. **Pocket Signal** contains an 8-bar melody; **Moon Circuit** a 16-bar chord pattern. Loading a preset or host state never auto-arms this generator.

- **Lock melody:** retains prior scale degrees/progression in the same mode; if the new rhythm changes the note count, cycles the old degree sequence. Changing the selected key explicitly revoices those degrees. In Vocal chops, it preserves slice indices, reduced modulo the available slice count.
- **Lock rhythm:** retains note count, onset, duration and velocity in the same mode. Changing bar length rescales those times. Mode changes start a fresh pattern.
- **Lock key:** RANDOMIZE retains root and scale. Manual key changes always remain possible.
- **Lock sound:** RANDOMIZE retains the patch. When unlocked in melodic modes, RANDOMIZE replaces layer settings with an original single-layer VA/wavetable/FM palette. This discards the previous layer mix; save it first. GENERATE/NEXT SEED always preserve sound. Vocal chop mode always preserves the recording/layer configuration.

**PLAY LOOP** arms independent playback using the current layer patch; live keyboard/MIDI notes continue on their own rack. **STOP LOOP** stops the generated rack only. B/PANIC stops both generators, audition and keyboard voices. PLAY LOOP disables the earlier 16-step vocal sequencer to avoid accidental doubling. With **Host BPM / transport** enabled, the pattern follows the host's tempo/play/stop and PPQ grid when provided, wraps at its own length, and clears generated voices on seeks. Without a host it uses a free clock. Disable sync to use Manual BPM and free playback even when a DAW is stopped. A mid-pattern start/seek waits for the next onset rather than reconstructing held notes.

Vocal chop output requires an enabled Vocal Lab layer and a prepared recording. It captures the current available slice indices and MIDI base when generated; new recordings/slice maps require regenerating. Playback uses stored slice indices even if the base MIDI key changes. Missing indices are silent. MIDI export uses the captured note numbers, so use that same base mapping when playing it externally.

**EXPORT MIDI** writes a standard format-0 `.mid` file at 960 PPQ, with note velocities/durations, tempo, 4/4 and an exact end-of-track at the loop length. **EXPORT WAV** snapshots all current layer parameters and renders the saved pattern through a private WEE rack on a worker: 48 kHz, stereo, 24-bit, exact bar length at the captured BPM. It includes the existing layer filters/modulation/mix and vocal processing, but no future effects rack or live keyboard/expression/controller performance. It reloads the referenced sample and fails visibly for a required missing/unsupported source. A 5 ms edge fade limits boundary clicks. Release tails beyond the bar boundary are cut; this is not a seamless-tail guarantee. Export does not include the separate 16-step sequencer. Changes made after queuing an export do not update that job. A new export replaces/cancels an earlier job; failures leave an existing target intact.

Exports use host BPM when sync is enabled (last observed BPM if stopped), otherwise Manual BPM. Host tempo is clamped to 20–400 BPM, manual tempo to 40–240. MIDI/WAV are normal files: import into FL Studio. Native drag-out is still unfinished. Generator controls/actions are saved patch metadata, not host-automatable APVTS parameters. DSP automation remains available on the existing 400 parameters. There is no AI model, note editor, per-part arranger, MIDI-through output, odd meter, streaming or additional sample library in this milestone.
