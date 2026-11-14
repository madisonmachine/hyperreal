# Phase 3 sixteen-pad foundation

Status: in progress, 14 November 2026. This branch is based on the Phase 2 draft pull request.

## Implemented in this slice

- Sixteen independent sample slots map to MIDI 36–51. The UI selects a pad, shows all pad load states, and routes LOAD, CANCEL LOAD and AUDITION to the selected pad. A double-click auditions a loaded pad.
- A worker decodes one bounded request per pad, using round-robin service across pending pads. A newer request for the same pad invalidates the older result; cancellation invalidates queued or in-flight results. Failed loads preserve the currently playable sample.
- A fixed pool of 64 voices supports at most four overlapping one-shot voices per pad. The oldest voice in that pad is stolen at its limit; when the global pool is full, the oldest global voice is stolen. MIDI events retain sample offsets and velocity gain. UI audition requests coalesce per pad when several arrive in one audio block.
- Sample replacement leaves existing voices on their immutable old buffer. The worker retires buffers only after the callback and every voice release them.
- Version 3 host state embeds all 16 pad samples, bounded to 128 MB decoded per sample and 512 MB for the current set. The reader accepts Phase 2 one-pad state and Phase 1 parameter-only state. It validates all 16 samples before publishing the replacement set.

## Checks completed

- The React/TypeScript UI production build passed.
- The Release VST3 and standalone build passed.
- Release pluginval strictness 5 passed, including editor creation and automation checks.
- Engine tests passed queued imports, independent MIDI 36/37 timing, overlapping voices, replacement during playback, bounded per-pad voices, multi-pad embedded state, and preservation after invalid import.
- Full processor state tests passed source-free recall, Phase 2 `HRP2` migration and rejection of a truncated state without replacing the active sample or gain.

## Checks and features not yet completed

- Manual Cubase Pro 15 validation of the 0.3.0 editor, loading multiple pads, MIDI 36–51, cancellation and saved-project recall. The installed 0.2.0 build remains the Cubase-verified version until this branch is ready to install. Editor interaction has not yet been tested in a host.
- Drag-and-drop import, pad copy/paste/swap/clear, mute, multi-solo and per-pad level audio control.
- Gated mode, note-off/repeated-note ownership, adjustable velocity response, configurable polyphony, choke groups, all-notes-off and transport reset behaviour.
- Anti-click fades when stealing/choking voices, dense-trigger performance measurement and long-session memory stress.
- The Phase 2 clean disconnected-Windows installation test remains open. The signed offline WebView2 installer and readiness check are prepared, but this PC has no clean Windows VM. This check must remain a release gate.

The callback performs no file I/O, JavaScript, routine allocation or blocking lock. Host state writing remains synchronous and can be large; snapshot cost under dense playback and large media requires later hardening.
