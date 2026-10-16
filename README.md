# WEE — Phase 2 sampler (0.2.0)

A real C++20/JUCE synthesizer source project targeting Windows x64 VST3 and standalone. Original handheld-console styling; no Nintendo assets, proprietary instrument code, or sampled recordings.

**Status:** Phase 1 VST3/standalone compiled on Windows and passed DSP/state tests. Phase 2 adds sampling; the Windows x64 VST3/standalone build and all three DSP, sampler and integration test suites passed in GitHub Actions run 38851631923. Consult the latest PR checks for subsequent revisions. FL Studio playback remains unverified. This is an incremental instrument, not the complete flagship specification.

## Build on Windows

Install Visual Studio 2022 with **Desktop development with C++**, MSVC v143, and Windows 10/11 SDK. Install CMake 3.22+, Git, and Python 3 and place them on PATH. Use Windows x64. JUCE 8.0.6 is fetched by CMake; for offline builds supply a local checkout with `-JucePath`.

From PowerShell:

```powershell
./scripts/build-windows.ps1 -Configuration Debug
./scripts/build-windows.ps1 -Configuration Release
# Optional install, from an elevated PowerShell if required:
./scripts/build-windows.ps1 -Configuration Release -Install
# Offline dependency source:
./scripts/build-windows.ps1 -JucePath C:/Dependencies/JUCE
```

Equivalent commands:

```powershell
python scripts/generate-demo.py
python scripts/verify-content.py
cmake -S . -B build/windows -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build/windows --config Release --parallel 2
ctest --test-dir build/windows -C Release --output-on-failure
```

Outputs are `build/windows/WEE_artefacts/Release/VST3/WEE.vst3` (a directory bundle) and `build/windows/WEE_artefacts/Release/Standalone/WEE.exe`. Install the **whole** VST3 bundle into `C:/Program Files/Common Files/VST3/`. Close hosts before overwriting. Installation is optional; the build does not alter system folders.

Standalone uses JUCE's audio/MIDI settings dialog to select devices and buffer sizes. Default Windows audio is available without an ASIO SDK. Optional ASIO requires a separately obtained SDK and CMake `-DWEE_ENABLE_ASIO=ON -DWEE_ASIO_SDK_PATH=<sdk-root>`; review that SDK's license before distribution. This path is untested.

## Play

Load WEE as an instrument, route MIDI to it, or click the onscreen keyboard. Sine, bandlimited saw, and bandlimited pulse are available. Six automatable controls cover output, ADSR, and tone cutoff. Pitch bend is ±2 semitones; CC64 sustains notes. MIDI channels share one instrument/voice pool. A / INIT resets parameters; B / PANIC stops all voices. Controls are keyboard focusable; JUCE's onscreen keyboard supports computer-keyboard playing when focused. Load/save uses `.wee` XML patches; three original synthesis patches are in `Content/Presets`. Host state recalls all seven parameters; external file references are not required in Phase 1.

The display reports last-block peak, active voices, host BPM (120 fallback), and estimated callback load. CPU measurement is indicative, not a complete host performance meter. Resizing is supported within 1000×720 to 1600×1152.

## FL Studio verification procedure — not yet performed

1. Build Release x64 and copy the VST3 bundle to the standard folder.
2. Open **Options → Manage plugins**, enable verification, and **Find installed plugins**.
3. Add WEE to the Channel Rack, play MIDI, and test velocities, sustain and pitch bend.
4. Automate all parameters; save/reopen a project and compare settings and audio.
5. Test 44.1/48/96 kHz and buffers 32–1024, multiple instances, resize, offline WAV bounce, and rapid preset switching.
6. Run a VST3 validator/pluginval before calling a release validated.

If absent from scanning, confirm x64 Release and the complete `.vst3` directory, rescan failed plugins, and inspect the build/scan error. If silent, check MIDI routing, audio device, output gain and sustain settings. Crackles require profiling, not merely increasing the buffer. If JUCE download fails, supply `WEE_JUCE_PATH`; do not disable TLS verification. No Linux development packages or JUCE are bundled.

## Tests and architecture

`Source/dsp/Engine.h` is independent of JUCE. Fixed voice storage, polyBLEP oscillators, envelope, one-pole filter, gain smoothing and soft output bounding require no dynamic allocation or locks. `PluginProcessor` maps sample-positioned MIDI, parameters, host tempo and versioned state. `Source/ui` handles presentation and file dialogs. `Source/dsp/Sampler.h` implements stereo resampling; `Source/sampling` contains the background decoder and a bounded pointer handoff with deferred asset destruction. GUI keyboard events use a bounded single-producer/single-consumer queue; disk operations stay on the message thread. The new sampler reads fully decoded RAM buffers; it does not stream from disk.

Portable test without JUCE or CMake:

```sh
g++ -std=c++20 -O2 -I Source tests/DspTests.cpp -o /tmp/wee_dsp_tests
/tmp/wee_dsp_tests
g++ -std=c++20 -O2 -pthread -I Source tests/SamplerTests.cpp -o /tmp/wee_sampler_tests
/tmp/wee_sampler_tests
```

Or configure `cmake -S . -B build/core -DWEE_CORE_ONLY=ON`, build and run CTest. The full build adds state serialization/preset tests. See [status and next steps](docs/STATUS.md), [content format](docs/CONTENT.md), and [licensing](LICENSE.md).

## GitHub Actions builds

The Windows VST3 and Standalone workflow runs on pushes to main, pull requests, or manually from the Actions tab. It builds Release x64, runs DSP, sampler, and preset/state tests, and uploads `WEE-Windows-x64` only after all steps pass. Open a successful run, download that artifact, then extract the inner ZIP. Install the entire `WEE.vst3` folder into the standard VST3 directory; `WEE.exe` is the standalone. The package includes the presets and documentation. Artifacts expire after 14 days. A successful build does not verify FL Studio playback or constitute a signed release.

## Sample cartridge — Phase 2

Choose **LOAD SAMPLE / RELOCATE** to browse WAV, AIFF or FLAC files, or drop a supported audio file on the plugin. Loading switches the sound source to SAMPLE. Wait until the loading message is replaced by the filename before playing or starting an offline bounce. Keyboard notes received while loading are ignored, not queued. Unsupported/missing samples become silent and display an error; use the same button to relocate a missing recording. Choose OSCILLATOR to return to the Phase 1 synth.

The waveform shows the decoded audio peak envelope, with amber start/end markers. Set START and END in normalized 0–1 units, ROOT MIDI to the recording's original pitch (60 by default), and LOW/HIGH KEY to its keyboard range. ROOT, key range, trim and LOOP are captured on each note-on; replay notes after changes. The existing ADSR, velocity, pitch bend, sustain and tone filter work in sample mode. LOOP repeats the trimmed region during the held note and its envelope release. Invalid trim or reversed key ranges sound silent and cannot be loaded as a saved preset. Clear stops/unloads the sample; A / INIT also resets parameters.

The two new factory presets, **Pocket Reed.wee** and **Glass Cartridge.wee**, use original synthetic audio generated by `scripts/generate-demo.py`. The PowerShell script and CI generate and verify these WAVs automatically. Keep `Content/Presets` and `Content/Samples` together. Samples remain external to the plugin; presets save a relative path for folder relocation and an absolute path as fallback. Host projects save the absolute path and parameters, not audio data. Archive your own sample files with your project. The three Phase 1 synthesis presets remain compatible through schema migration.

Limits: one mapped sample per patch, 32 voices, mono/stereo input files, 8–192 kHz, at most 120 seconds and 128 MiB decoded per asset. Interpolation changes duration with pitch and can alias at large pitch shifts. No time stretching, multisample zones, streaming or automatic crossfade loops are claimed. Loop points must be chosen carefully; discontinuous loops and asset switches may click. See docs/STATUS.md for the remaining phases.
