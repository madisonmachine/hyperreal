# Phase 3 sixteen-pad foundation

Status: in progress, 14 November 2026. This branch is based on the Phase 2 draft pull request.

## Implemented in this slice

- Sixteen independent sample slots map to MIDI 36–51. The UI selects a pad, shows all pad load states, and routes LOAD, CANCEL LOAD and AUDITION to the selected pad. A double-click auditions a loaded pad.
- A worker decodes one bounded request per pad, using round-robin service across pending pads. A newer request for the same pad invalidates the older result; cancellation invalidates queued or in-flight results. Failed loads preserve the currently playable sample.
- A fixed pool of 64 voices supports at most four overlapping one-shot voices per pad. The oldest voice in that pad is stolen at its limit; when the global pool is full, the oldest global voice is stolen. MIDI events retain sample offsets and velocity gain. UI audition requests coalesce per pad when several arrive in one audio block.
- Sample replacement leaves existing voices on their immutable old buffer. The worker retires buffers only after the callback and every voice release them.
- Host state embeds all 16 pad samples, bounded to 128 MB decoded per sample and 512 MB for the current set. The reader accepts Phase 3, Phase 2 one-pad and Phase 1 parameter-only state. It validates all 16 samples before publishing the replacement set.

## Note behavior and host controls, 14 November 2026

- Pad level now controls rendered audio. Each pad has stable host parameters for one-shot/gate mode and choke group 0 to 4, appended after the original parameter IDs. The editor shows mode and choke controls for the selected pad.
- Gate note-off releases the oldest held voice on that pad over 5 ms; one-shot notes ignore note-off. Zero-velocity note-on is treated as note-off. Choke triggers and MIDI all-notes-off/all-sound-off fade voices over 5 ms. Audition remains one-shot.
- Host state is now HRP4. HRP3 and HRP2 remain readable. The engine and processor smoke tests cover gate release, one-shot note-off, choke fade, all-notes-off, command validation, current-state recall and older-state migration.

## Checks completed

- The React/TypeScript UI production build passed.
- The Release VST3 and standalone build passed.
- Release pluginval strictness 5 passed, including editor creation and automation checks.
- Engine tests passed queued imports, independent MIDI 36/37 timing, overlapping voices, replacement during playback, bounded per-pad voices, multi-pad embedded state, and preservation after invalid import.
- Full processor state tests passed source-free recall, Phase 2 `HRP2` migration and rejection of a truncated state without replacing the active sample or gain.

## Checks and features not yet completed

- Manual Cubase Pro 15 validation of the 0.3.0 editor, loading multiple pads, MIDI 36 to 51, mode and choke controls, cancellation, automation and saved-project recall. The installed 0.2.0 build remains the Cubase-verified version; 0.3.0 has not yet been installed or tested in Cubase.
- Drag-and-drop import, pad copy/paste/swap/clear, mute, multi-solo, adjustable velocity response and configurable polyphony.
- Anti-click fades when stealing voices at the four-voice pad limit or 64-voice global limit; MIDI-channel-aware repeated-note ownership and transport reset behavior.
- Dense-trigger performance measurement and long-session memory stress.
- The Phase 2 clean disconnected-Windows installation test remains open. The signed offline WebView2 installer and readiness check are prepared, but this PC has no clean Windows VM. This check remains a release gate.

The callback performs no file I/O, JavaScript, routine allocation or blocking lock. Host state writing remains synchronous and can be large; snapshot cost under dense playback and large media requires later hardening.
