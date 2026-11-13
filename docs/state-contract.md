# State and parameter contract

Version 1 draft, 13 November 2026. The shell implements the host-parameter subset below. The JSON Schemas in `schemas` describe planned kit and project records; they are not yet runtime serializers.

## Host parameter IDs

- `master.gain.v1`: linear gain, normalized 0 to 1, default 1.
- `pad.01.level.v1` through `pad.16.level.v1`: linear level, normalized 0 to 1, default 1.
- Future pad IDs follow `pad.NN.<semantic-name>.vM`, where NN is 01 to 16 and M is a semantic version integer. Grid slot and MIDI note stay separate: slot 01 defaults to note 36 and slot 16 to note 51. A swap exchanges pad content, not slot ID or MIDI note.

The `.v1` suffix is part of the immutable host ID. Do not rename or repurpose an ID after release. Add a new ID and a migration when semantics change. Parameter range, display unit, smoothing, and ownership for each new control must be documented before it is exposed to a host.

## Current shell serialization

`HYPERREAL_STATE` is a JUCE ValueTree with `schema_version = 1` and AudioProcessorValueTreeState parameter values. Unsupported or malformed state is ignored without replacing active state. This is a minimal Phase 1 state path; it contains no samples or kit content.

## Planned portable data

A pad record contains its fixed slot, source sample reference or bundled content identifier, non-destructive edit coordinates, mode, tuning, voice and choke settings, envelope/filter/LFO settings, mute/solo, level/pan, and bus assignment. A kit contains 16 pad records and a versioned manifest. Factory kits are read-only; user kits are stored in a separate writable location. Portable bundles must use relative paths, reject path traversal, and preserve original media. A project state must capture or collect referenced media robustly enough to recall after source files move.

Each schema has an integer `schemaVersion`. Keep readers for supported old versions. Reject unsupported future versions without partially applying data. File decode, archive validation, and serialization run off the audio callback. Prepared changes become active at safe processing boundaries.
