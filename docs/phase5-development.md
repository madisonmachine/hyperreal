# Phase 5 shaping filters and modulation

Development began 15 November 2026 on `phase5-shaping-filters-modulation`, based on the Phase 4 branch. This is the first implementation slice, not Phase 5 sign-off. Phase 4 Cubase checks remain deferred to the combined host pass at the end of Phase 5.

## Implemented

- Added a per-voice, per-pad linear amplitude ADSR. Attack and decay ramp to sustain. A gate note-off releases from the current level. A one-shot with positive release begins its release near the edited sample end; a release longer than the remaining sample is clamped to that boundary. Zero-valued defaults preserve the earlier sound and 5 ms gated safety fade. Choke, mute, steal and transport-stop fades remain separate.
- Exposed 64 stable per-pad host parameters for amplitude attack, decay, sustain and release, with validation and host change gestures. The embedded editor has keyboard-operable range controls, numerical values and a schematic envelope graph. Host snapshots supply its values.
- Added the amplitude parameters to pad copy, paste, swap and clear. `HRP8` saves them, while `HRP7` and earlier states migrate to defaults.
- Added per-voice stereo state-variable filters with Off, low-pass, high-pass and band-pass modes. Cutoff is capped for the host rate, Q is limited to 0.5–10, and bipolar filter-envelope depth spans ±48 semitones. A separate filter ADSR controls that depth. Cutoff, Q and depth update active voices with smoothed coefficients; mode and envelope settings take effect on new voices. Filter state stays with each voice, including overlapping and stolen voices.
- Added host parameters, a filter panel and an independent envelope graph. `HRP9` persists filter settings and reads `HRP8` sessions with filter defaults. Pad copy, paste, swap and clear include the filter settings.
- Added per-voice LFO modulation of pitch or filter cutoff, with sine, triangle, square and saw shapes; 0.05–20 Hz rate; 0–24 semitone depth; and reset or free-running phase. Reset starts at phase zero on each voice; free-running phase continues between notes and resets when audio preparation restarts. Destination, shape and phase mode are captured at note onset. Rate and depth read live host values. The editor shows host-backed controls and a waveform preview. `HR10` state saves LFO settings and reads `HRP9` sessions with defaults. Pad copy, paste, swap and clear include LFO settings.

## Checked locally

The React/TypeScript production build, Release VST3 build, both native CTest suites and pluginval strictness 5 with editor checks pass. Engine regressions cover ADSR stages, gated release and a one-shot release longer than the remaining sample. Processor tests cover invalid commands, saved-state recall, pad operations and migration from `HRP7`. A synthetic run with a non-default ADSR on every pad processed 1.6 million triggers in 100,000 blocks of 128 frames at 44.1 kHz: 0.140 ms average and 1.134 ms worst observed block, with private bytes steady at 4,198,400 during that dense-trigger section. This is a local closed-editor benchmark, not a host guarantee.

The filter slice also passes the React/TypeScript build, Release VST3 build, both CTest suites and pluginval strictness 5. Rendered-audio checks cover filter-mode DC response, positive and negative envelope depth, filter-envelope attack, independent overlapping voices, and finite output at high Q. Processor tests cover filter command ranges, `HRP9` recall, `HRP8` migration and pad operations. A synthetic run with both envelopes and low-pass filtering processed 1.6 million triggers in 100,000 blocks of 128 frames at 44.1 kHz: 0.445 ms average and 2.120 ms worst observed block, with private bytes decreasing during that dense-trigger section. This is a local closed-editor result, not a host guarantee.

The LFO slice passes the React/TypeScript build, Release VST3 build, both CTest suites and pluginval strictness 5 with editor checks. Engine audio regressions cover reset and free-running pitch phase and cutoff modulation. Processor checks cover invalid LFO commands, `HR10` recall, `HRP9` migration and pad operations. The synthetic full shaping chain benchmark processed 1.6 million triggers in 100,000 blocks of 128 frames at 44.1 kHz: 0.597 ms average and 1.589 ms worst observed block, with private bytes falling from 4,186,112 to 4,091,904 during dense triggering. This is a local closed-editor result, not a host guarantee.

## Remaining Phase 5 work

- Complete control presentation and host automation behavior, including display text, dense-update coalescing and visible host changes. Assess the filter and ADSR range ergonomics and schematic graphs in the host editor.
- Rendered audio comparisons, stability tests at extreme modulation and resonance, sample-rate changes and long silence, listening checks, and a new benchmark with the full shaping chain.
- Combined Phase 4/5 Cubase checks at the end of Phase 5. The current installed per-user VST3 remains the validated Phase 4 build; the Phase 5 development build has not been installed for host testing.
