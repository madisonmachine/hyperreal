# Phase 4: waveform editing and tuning

Development started 14 November 2026 on `phase4-waveform-editing-tuning` from the Phase 3 branch. This is a development build, not a Phase 4 sign-off.

## Implemented

- Source-frame trim boundaries, fade-in and fade-out lengths, and reverse flags live beside immutable shared sample data. Editing does not rewrite or duplicate the original audio buffer. Existing voices retain their previous edit snapshot; newly triggered MIDI and audition voices use the new edit.
- Import and state restoration generate four cached min/max waveform resolutions. The editor requests a bounded summary when its waveform panel opens or the sample changes, and retains up to eight summaries while the editor is open. Canvas draws the original trace dimly and a bright preview scaled by the staged trim/fade envelope, with selected region, markers, reverse label and active playback cursor. This is a peak-summary preview, so narrow transients and fade transitions within one summary bucket are approximate. Trim and fade handles occupy separate lanes so zero-length fades can be dragged; reversing moves fade handles to the playback-relative ends. Canvas resolution follows host window resizing. It offers 1x to 8x zoom, panning, frame entry, seconds readout and estimated playback duration after tuning. Edits are drafted until Apply; Revert Draft cancels an unfinished gesture.
- Coarse tune is a host parameter from -24 to +24 semitones; fine tune is a host parameter from -100 to +100 cents. Numeric input validates range and reports rejected values; semitone snap rounds the combined offset. Resampling changes pitch and duration together, and a 10 ms glide smooths changes to an active voice. Voices retire at the selected region boundary within the current audio block.
- Host state `HRP7` stores edit metadata and tune parameters. Older `HRP6` through `HRP2` states migrate with full-region default edits and zero tuning. Copy, paste, swap and clear include or reset the new fields.

## Checked locally

The Release UI and plugin build and both CTest suites pass. Pluginval strictness 5 reports SUCCESS. Engine tests cover forward trim, fades, reverse, invalid boundaries, cached waveform data, octave-up/down duration, 44.1-to-48 kHz conversion and edited state round-trip. Processor tests cover host parameter validation, current state round-trip and older-state migration. The current Phase 4 build has not yet been installed or checked in Cubase.

## Still required for Phase 4 sign-off

- Host checks in Cubase for waveform display, marker dragging, MIDI versus audition consistency, save/reopen and automation of tuning.
- Wider sample-rate, short-region, extreme tuning and zoom checks; listen for aliasing at high transposition and decide whether the interpolation quality is sufficient.
- Check the editor at different sizes and while audio is playing, including playback cursor and UI responsiveness. Pitch analysis and scale quantization are optional and deferred.

The disconnected clean-Windows installation and sustained host-use checks remain deferred from earlier phases, separately from Phase 4 acceptance.
