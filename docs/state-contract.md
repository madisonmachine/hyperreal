# State and parameter contract

Version 5 host-state foundation, 14 November 2026. The JSON Schemas in `schemas` describe planned kit and project records; they are not yet runtime serializers.

## Host parameter IDs

- `master.gain.v1`: linear gain, normalized 0 to 1, default 1.
- `pad.01.level.v1` through `pad.16.level.v1`: linear level, normalized 0 to 1, default 1.
- `pad.01.mode.v1` through `pad.16.mode.v1`: choice 0 one-shot (default), 1 gate. In gate mode, note-off fades the oldest held voice for that pad over 5 ms.
- `pad.01.choke.v1` through `pad.16.choke.v1`: integer group 0 (off, default) through 4. A new note or audition fades existing voices in its nonzero group over 5 ms.
- `pad.01.mute.v1` through `pad.16.mute.v1`: Boolean, default false. Muted pads reject new notes and audition requests. Active voices fade over 5 ms when muted.
- `pad.01.solo.v1` through `pad.16.solo.v1`: Boolean, default false. If any pad is soloed, all soloed and unmuted pads can play; other pads reject new triggers and their active voices fade over 5 ms. Mute takes precedence over solo.
- Future pad IDs follow `pad.NN.<semantic-name>.vM`, where NN is 01 to 16 and M is a semantic version integer. Grid slot and MIDI note stay separate: slot 01 defaults to note 36 and slot 16 to note 51. A swap exchanges pad content, not slot ID or MIDI note.

The `.v1` suffix is part of the immutable host ID. Do not rename or repurpose an ID after release. Add a new ID and a migration when semantics change. Parameter range, display unit, smoothing, and ownership for each new control must be documented before it is exposed to a host.

## Current host serialization

The binary host state starts with `HRP5`, format version 5, and a length-prefixed `HYPERREAL_STATE` JUCE ValueTree with `schema_version = 5`. It then stores 16 fixed-order pad records. Each record has a presence flag and, when loaded, a sample name, source rate, channel count, frame count and decoded float data. No source path is required to restore the current sound. The reader validates all samples and the 512 MB aggregate limit before publishing any pad. Phase 3 `HRP4` and `HRP3` states retain all 16 samples; Phase 2 `HRP2` states are read as pad 1 plus 15 empty pads; Phase 1 parameter-only ValueTrees remain readable. New parameters use their defaults when absent from older states loaded into a fresh instance. Unsupported or malformed state is ignored without replacing active samples.

The UI snapshot uses bridge schema version 2, with selected pad index 0 to 15 and 16 pad status/name records plus the selected pad level, mode, choke, mute and solo controls. Each pad status record also reports mute and solo. Setting mode, choke, mute and solo uses bridge command version 3. Selection is an editor control and is not yet persisted. LOAD and AUDITION target a pad; audition is one-shot regardless of pad mode. The voice pool is fixed at four voices per pad and 64 globally. One-shot note-off is ignored. MIDI all-notes-off and all-sound-off fade active voices over 5 ms. A note-on with zero velocity is treated as note-off. The oldest voice in a pad is replaced immediately when its limit is reached; click-free voice stealing remains open. Note-off ownership tracks pad and trigger order, not MIDI channel.

## Planned portable data

A pad record contains its fixed slot, source sample reference or bundled content identifier, non-destructive edit coordinates, mode, tuning, voice and choke settings, envelope/filter/LFO settings, mute/solo, level/pan, and bus assignment. A kit contains 16 pad records and a versioned manifest. Factory kits are read-only; user kits are stored in a separate writable location. Portable bundles must use relative paths, reject path traversal, and preserve original media. A project state must capture or collect referenced media robustly enough to recall after source files move.

Each schema has an integer `schemaVersion`. Keep readers for supported old versions. Reject unsupported future versions without partially applying data. File decode, archive validation, and serialization run off the audio callback. Prepared changes become active at safe processing boundaries.
