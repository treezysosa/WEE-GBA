# Content and presets

Phase 1 `.wee` patches are JUCE ValueTree XML: root `WEE` with `schema="1"`, and `PARAM` children containing `id` and actual-unit `value` attributes. IDs: gain, attack, decay, sustain, release, cutoff, waveform (0 sine, 1 saw, 2 pulse). Host project state serializes this tree using JUCE XML binary encoding. Unknown future schema versions are rejected; migrations will be added when a version 2 exists. The three factory files are synthesis settings, not vocal recordings.

The manifest is a scaffold, not a completed library manager. Future audio assets stay outside the plugin binary and must record creator, source, license and usage rights, file hash, verified language/region and tags. License evidence and consent records should accompany commissioned recordings. Distinct regional and language categories need knowledgeable review. No vocal audio, celebrity imitation, scraped media, or third-party sample content is included.
