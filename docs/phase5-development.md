# Phase 5 shaping filters and modulation

Development began 15 November 2026 on `phase5-shaping-filters-modulation`, based on the Phase 4 branch. This is the first implementation slice, not Phase 5 sign-off. Phase 4 Cubase checks remain deferred to the combined host pass at the end of Phase 5.

## Implemented

- Added a per-voice, per-pad linear amplitude ADSR. Attack and decay ramp to sustain. A gate note-off releases from the current level. A one-shot with positive release begins its release near the edited sample end; a release longer than the remaining sample is clamped to that boundary. Zero-valued defaults preserve the earlier sound and 5 ms gated safety fade. Choke, mute, steal and transport-stop fades remain separate.
- Exposed 64 stable per-pad host parameters for amplitude attack, decay, sustain and release, with validation and host change gestures. The embedded editor has keyboard-operable range controls, numerical values and a schematic envelope graph. Host snapshots supply its values.
- Added the amplitude parameters to pad copy, paste, swap and clear. `HRP8` saves them, while `HRP7` and earlier states migrate to defaults.

## Checked locally

The React/TypeScript production build, Release VST3 build, both native CTest suites and pluginval strictness 5 with editor checks pass. Engine regressions cover ADSR stages, gated release and a one-shot release longer than the remaining sample. Processor tests cover invalid commands, saved-state recall, pad operations and migration from `HRP7`. A synthetic run with a non-default ADSR on every pad processed 1.6 million triggers in 100,000 blocks of 128 frames at 44.1 kHz: 0.140 ms average and 1.134 ms worst observed block, with private bytes steady at 4,198,400 during that dense-trigger section. This is a local closed-editor benchmark, not a host guarantee.

## Remaining Phase 5 work

- Independent filter-envelope ADSR, low-pass/high-pass/band-pass filters, resonance and bipolar envelope depth, with per-voice filter state and stable smoothing.
- LFO pitch/cutoff destinations, rate, shape, depth and documented phase behavior.
- Complete control presentation and host automation behavior, including display text, dense-update coalescing and visible host changes. Assess ADSR control range ergonomics and the schematic graph in the host editor.
- Rendered audio comparisons, stability tests at extreme modulation and resonance, sample-rate changes and long silence, listening checks, and a new benchmark with the full shaping chain.
- Combined Phase 4/5 Cubase checks at the end of Phase 5. The current installed per-user VST3 remains the validated Phase 4 build; the Phase 5 development build has not been installed for host testing.
