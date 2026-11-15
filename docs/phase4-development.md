# Phase 4: waveform editing and tuning

Development started 14 November 2026 on `phase4-waveform-editing-tuning` from the Phase 3 branch. This is a development build, not a Phase 4 sign-off.

## Implemented

- Source-frame trim boundaries, fade-in and fade-out lengths, and reverse flags live beside immutable shared sample data. Editing does not rewrite or duplicate the original audio buffer. Existing voices retain their previous edit snapshot; newly triggered MIDI and audition voices use the new edit.
- Import and state restoration generate four cached min/max waveform resolutions. The editor requests a bounded summary when its waveform panel opens or the sample changes, and retains up to eight summaries while the editor is open. Canvas draws the original trace dimly and a bright preview scaled by the staged trim/fade envelope, with selected region, markers, reverse label and active playback cursor. This is a peak-summary preview, so narrow transients and fade transitions within one summary bucket are approximate. Trim and fade handles occupy separate lanes so zero-length fades can be dragged; reversing moves fade handles to the playback-relative ends. Canvas resolution follows host window resizing. It offers 1x to 8x zoom, panning, frame entry, seconds readout and estimated playback duration after tuning. Edits are drafted until Apply; Revert Draft cancels an unfinished gesture.
- Coarse tune is a host parameter from -24 to +24 semitones; fine tune is a host parameter from -100 to +100 cents. Numeric input validates range and reports rejected values; semitone snap rounds the combined offset. Resampling changes pitch and duration together, and a 10 ms glide smooths changes to an active voice. Voices retire at the selected region boundary within the current audio block.
- Host state `HRP7` stores edit metadata and tune parameters. Older `HRP6` through `HRP2` states migrate with full-region default edits and zero tuning. Copy, paste, swap and clear include or reset the new fields.
- The import worker now rejects source rates above 384 kHz, matching host-state restore validation. This prevents out-of-range default edit calculations while preserving any sample already loaded on that pad. Malformed edit state is rejected before waveform summaries are generated.

## Checked locally

The Release UI and plugin build and both CTest suites pass. Pluginval strictness 5 reports SUCCESS. Engine tests cover forward trim, fades, reverse, invalid boundaries, cached waveform data, octave-up/down duration, 44.1-to-48 kHz conversion, edited state round-trip and rejection of a 500 kHz import without replacing the active sample. They now also cover a 96 kHz source at 44.1, 48 and 96 kHz host rates, both tuning extremes with fine-cent offsets, reverse boundaries, one-frame playback and overlong fade clamping. Processor tests cover host parameter validation, current state round-trip and older-state migration. The synthetic dense-trigger benchmark fixture now uses HRP7 edit records and runs again; 100,000 blocks of 128 frames with 16 triggers per block took 10.77 seconds on this PC, with about 0.108 ms average and 0.488 ms worst observed block. This is a closed-editor synthetic result, not a host guarantee. The validated 0.4.0 build is installed in the per-user VST3 folder for Cubase; its binary hash matches the build. The prior installed bundle is backed up in the ignored `build/manual-test` folder.

## Still required for Phase 4 sign-off

- Host checks in Cubase for waveform display, marker dragging, MIDI versus audition consistency, save/reopen and automation of tuning.
- Listen for aliasing at high transposition and decide whether the current interpolation quality is sufficient. Check extreme zoom in the host editor.
- Check the fixed-size Cubase editor while audio is playing, including playback cursor, scrolling and UI responsiveness. Host resizing is not exposed by the current editor and belongs to the later interface phase. Pitch analysis and scale quantization are optional and deferred.

The disconnected clean-Windows installation and sustained host-use checks remain deferred from earlier phases, separately from Phase 4 acceptance.
