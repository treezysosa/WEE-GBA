# WEE — Phase 3 hybrid instrument (0.3.0)

An original C++20/JUCE Windows x64 VST3 and standalone instrument with a handheld-console interface. Eight layers combine virtual analog, sample playback, wavetable, two-operator FM and granular foundations. No proprietary instrument code, branded console assets, scraped recordings or voice impersonations are included.

**Verification:** Phase 3 Windows x64 VST3/standalone compilation and all four test suites passed in Actions run 38854097991. The subsequent host-tail adjustment has a local regression test; consult this revision’s Actions checks for its Windows result. Actual FL Studio playback, human listening, GUI interaction and pluginval remain unverified.

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

LOAD/SAVE PATCH uses `.wee` XML files; host project state stores parameters and the shared sample path. New schema 3 preserves all 361 parameters. Phase 1 and Phase 2 presets migrate into layer 1 and disable added layers. Sample presets store a relative path for moved content folders plus an absolute fallback. Missing/corrupt samples are silent and show an error; LOAD SAMPLE / RELOCATE repairs the reference. Save the patch again afterward. Audio is external, not embedded in projects, so archive your recordings with your project.

Factory patches include **Prism Pocket** (wavetable motion), **Dual Orbit** (three-layer WT/FM/VA), **Eightfold Nebula** (eight-layer pad) and **Grain Lantern** (granular + WT), plus the five earlier synth/sample patches. Two original CC0 synthetic stereo recordings are generated by `scripts/generate-demo.py` and verified against a provenance manifest. The build script/CI generate them automatically; no vocal collection is bundled. Keep `Content/Presets` and `Content/Samples` together.

## GitHub Actions download

The Windows workflow runs on pushes to main, pull requests and manual dispatch. It builds Release, runs four CTest suites and verifies demo content. Successful runs upload **WEE-Windows-x64** for 14 days. Download that artifact, extract it, then extract the enclosed `WEE-Windows-x64.zip`. It contains `WEE.vst3`, `WEE.exe`, Content, documentation and licenses. A successful CI build is not FL Studio certification or a signed installer.

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

This is an incremental instrument. Wavetables are built-in, FM is two-operator, granular uses four fixed heads per voice, and eight layers share one RAM sample asset. There is no multisample library manager, streaming, formant preservation, time stretching or complete vocal laboratory yet. Complex patches have not been profiled for real-time DAW CPU limits. See docs/STATUS.md and docs/CONTENT.md for exact boundaries and next steps.
