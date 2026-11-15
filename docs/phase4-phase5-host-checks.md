# Combined Phase 4 and Phase 5 Cubase checks

The Phase 5 `0.5.0` VST3 was installed in the per-user VST3 folder on 15 November 2026. This checklist records the host behavior still needed before Phase 4 and Phase 5 sign-off. Use a copy of the saved `hyperreal-test` Cubase project so exploratory edits do not alter the original. Test media are in `build/manual-test`.

## 1. Load and recall baseline

- Open the copied project and HYPERREAL. Confirm the custom 16-pad editor appears, its previously loaded sample is READY, and the existing MIDI clip plays it.
- Close and reopen only the editor during playback. Confirm playback continues and the editor returns without losing the pad state.

## 2. Waveform and tuning (Phase 4)

- Load `hyperreal-sustain-4s.wav` to a spare pad. Open WAVEFORM / SAMPLE EDIT. Confirm the source trace, trim bounds, diagonal fade lines, time/frame readouts, zoom and panning are legible.
- Drag start/end and fade tips. Confirm the displayed edit and the sound change immediately, without an Apply action. Move a trim line after setting a fade: the diagonal line should remain anchored to its trim boundary. Reverse playback and check that the playback cursor travels in the new direction while the sample sounds.
- Start a marker drag and press Escape. Confirm the starting edit returns. Try 1x and 8x zoom and pan to both ends. Check that the cursor moves smoothly through playback and the editor remains responsive.
- Change coarse and fine tuning, then use semitone snap. Confirm pitch and duration change together and the on-screen duration updates. Listen at the highest upward setting for aliasing or harsh artifacts and report whether the quality is acceptable for percussion.
- Save, close and reopen the project; confirm the edit, reverse and tuning values and sound are recalled.

## 3. Pad-click MIDI output (Phase 4)

- Add a separate MIDI track. Set its input to HYPERREAL's MIDI output, its output back to HYPERREAL, and record-enable that MIDI track. Record several pad clicks. Confirm recorded notes use the clicked pads' MIDI notes (36–51) and the resulting MIDI part plays HYPERREAL.
- Record a keyboard note into the normal instrument track. Confirm it does not create a duplicate note on the separate HYPERREAL-output MIDI track.
- Cubase records an instrument track's input before the instrument runs, so pad clicks are not expected to write into that same instrument track directly.

## 4. Shaping and modulation (Phase 5)

- On a sustained sample, change amplitude attack, decay, sustain and release. Confirm the graph and numeric values follow the controls and the sound responds. Compare one-shot with gate mode: gated note-off should begin release; one-shot should end at the selected sample boundary.
- Switch the filter Off, Low-pass, High-pass and Band-pass. Sweep cutoff and Q; confirm distinct audible responses and no explosive output. Set positive and negative filter-envelope depth and change its independent ADSR; confirm its graph and sound respond without altering the amplitude ADSR.
- Route the LFO to Pitch, then Filter Cutoff. Try slow and fast rates, each of the four shapes, and depth zero versus a clearly audible depth. Confirm zero depth has no audible modulation. Compare Reset Each Note with Free Running by repeatedly triggering a long sample: reset should start from the same phase, while free running should continue between notes.
- With shaping enabled, verify velocity changes level, mute stops new notes, solo limits playback, choke cuts the previous voice, and repeated triggers respect the voice limit without a loud click.

## 5. Host automation, persistence and sustained use (Phase 5)

- In Cubase, automate representative amplitude attack, filter cutoff/Q, and LFO rate/depth parameters. Use Write/Read or an automation lane, then play the recorded automation. Confirm the custom editor visibly follows the host values and sound changes smoothly. Drag each control continuously for several seconds and check that the displayed value does not jump back or stick at an earlier value.
- Save, close and reopen the project. Confirm samples, Phase 4 edits, envelope/filter/LFO settings and automation all return with the same sound.
- Play repeated MIDI triggers for a longer session and leave the editor open for several minutes. Check for silence after notes end, freezes, audio dropouts, UI lag and unexpected CPU growth. A focused real-time render or offline bounce can supplement this check if Cubase permits it; compare its duration and sound with live playback.

Record each result as pass, fail with steps to reproduce, or deferred. The clean offline Windows installation check from Phase 2 remains separately deferred until a suitable clean environment is available.
