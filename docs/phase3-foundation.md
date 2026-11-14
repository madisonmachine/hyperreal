# Phase 3 sixteen-pad foundation

Status: implementation complete, with one-shot end-fade host listening and deferred stress checks open, 14 November 2026. This branch is based on the Phase 2 draft pull request.

## Implemented in this slice

- Sixteen independent sample slots map to MIDI 36-51. The UI shows all pad load states. A single left-click auditions a loaded pad; right-click opens pad-specific load and edit actions. CANCEL LOAD appears while the selected pad is loading.
- A worker decodes one bounded request per pad, using round-robin service across pending pads. A newer request for the same pad invalidates the older result; cancellation invalidates queued or in-flight results. Failed loads preserve the currently playable sample.
- A fixed pool of 64 voices supports at most four overlapping one-shot voices per pad. The oldest voice in that pad is stolen at its limit; when the global pool is full, the oldest global voice is stolen. MIDI events retain sample offsets and velocity gain. UI audition requests coalesce per pad when several arrive in one audio block.
- Sample replacement leaves existing voices on their immutable old buffer. The worker retires buffers only after the callback and every voice release them.
- Host state embeds all 16 pad samples, bounded to 128 MB decoded per sample and 512 MB for the current set. The reader accepts Phase 3, Phase 2 one-pad and Phase 1 parameter-only state. It validates all 16 samples before publishing the replacement set.

## Note behavior and host controls, 14 November 2026

- Pad level now controls rendered audio. Each pad has stable host parameters for one-shot/gate mode and choke group 0 to 4, appended after the original parameter IDs. The editor shows mode and choke controls for the selected pad.
- Gate note-off releases the oldest held voice on that pad over 5 ms; one-shot notes ignore note-off. Zero-velocity note-on is treated as note-off. Choke triggers and MIDI all-notes-off/all-sound-off fade voices over 5 ms. Audition remains one-shot.
- This slice introduced HRP4 state and kept HRP3 and HRP2 readable. The engine and processor smoke tests covered gate release, one-shot note-off, choke fade, all-notes-off, command validation, current-state recall and older-state migration.

## Checks completed

Mute and multi-solo were added after the note controls. Each pad has Boolean host parameters for mute and solo, with mute taking precedence. Muted or excluded pads reject new triggers; active voices fade over 5 ms. The grid shows MUTED or SOLO status. HRP5 persists these settings and reads HRP4, HRP3 and HRP2 states. Engine tests cover mute, two soloed pads and transition fades; processor tests cover command validation, state recall and HRP4 migration.

Pad copy, paste, swap and clear now move the sample and all current per-pad settings. Copy captures a separate, temporary buffer; paste checks the 512 MB active kit limit. Swap exchanges content while MIDI notes remain attached to grid slots. Paste, swap and clear cancel pending imports for affected pads, and voices already playing retain their original sample. The editor requires a second confirmation step for paste, swap and clear. The engine and processor tests cover sample and setting transfer, fixed MIDI mapping, clearing and active-voice retention. The clipboard is editor-session data and is not saved in host state.

The engine now uses a bounded 64-slot fade-tail pool when stealing a voice. Gated note-off matches MIDI channel and releases the oldest held note for that pad/channel. Per-pad voice limit is 1 to 4; one voice provides mono retrigger. Velocity curves are soft (square root), linear and hard (square). HRP6 saves the new controls and reads HRP5. Same-offset events follow MIDI buffer order, so the later note in a choke group wins. A playing-to-stopped host playhead transition requests a 5 ms release of active voices; the engine path is covered by a native test. Cubase showed that native window file drops did not reach JUCE through WebView2. The editor now reads a dropped File in bounded 64 KB chunks inside WebView2, transfers the bytes through validated native commands, and decodes them on the worker. The pad that receives the DOM drop is the target; drops outside the grid target the selected pad. Cubase confirmed that drag and drop now loads samples successfully.

A headless Release benchmark of the revised engine loaded 16 pads and processed 100,000 blocks of 128 frames at 44.1 kHz with 16 triggers per block: 1.6 million triggers in about 6.70 seconds, 0.067 ms average and 0.384 ms worst observed block. Private bytes stayed at 2,887,680. A further 5,000 copy/paste/swap/clear and snapshot cycles and 1,000 worker sample replacements while processing also left private bytes level after warmup. This is a synthetic closed-editor measurement on this PC, not a DAW timing guarantee or a long-session editor test.

- The React/TypeScript UI production build passed.
- The Release VST3 and standalone build passed.
- Release pluginval strictness 5 passed, including editor creation and automation checks.
- Engine tests passed queued imports, independent MIDI 36/37 timing, overlapping voices, replacement during playback, bounded per-pad voices, multi-pad embedded state, and preservation after invalid import.
- Full processor state tests passed source-free recall, Phase 2 `HRP2` migration and rejection of a truncated state without replacing the active sample or gain.

Cubase Pro 15 confirmed the 0.3.0 custom 16-pad editor with VOICES and VELOCITY controls. In the earlier build, LOAD TO PAD 01, its AUDITION button and copy/paste buttons worked, while direct file drops failed and pad audition required a delayed double-click. The revised editor uses single-click audition and right-click menus for LOAD SAMPLE, COPY, PASTE, SWAP and CLEAR. In Cubase, the owner confirmed that drag and drop, copy/paste, Swap Pads and master gain work; loaded pads are READY and audible, respond to a MIDI keyboard, and survive saving, closing and reopening the project in the same state. One left-click auditions immediately without lag, and a drop onto an unselected pad loads that specific pad. Native tests cover memory-backed WAV decode and MIDI playback, and the Release build/pluginval editor checks pass.

The owner then completed the remaining interactive Cubase procedure. Right-click LOAD SAMPLE and CLEAR PAD worked, including confirmation and cancellation of the clear prompt. Copy/paste transferred a sample plus mode, choke group, voice limit and velocity curve settings; the destination kept its MIDI slot. The 16-note MIDI 36-51 fixture played correctly. One-shot/gate release, mute and multi-solo including mute precedence, and shared/off choke groups behaved as expected. Voice limit 1 versus 4, the three velocity curves, and Cubase write/read automation behaved as expected. Stopping Cubase also stopped a playing sample, but the audible result cannot distinguish HYPERREAL's playhead transition from a host MIDI all-notes-off event. The short sample loads did not expose a usable CANCEL LOAD interval. Sustained editor-open use was deferred.

The remaining Phase 3 implementation gaps were closed after that Cubase session. The engine now publishes a per-pad MIDI note-on counter; the editor flashes MIDI activity separately from click audition and persistent selection/load state. The owner confirmed both distinct flashes in Cubase Pro 15. One-shot playback fades through the last 5 ms of the source sample to zero at its final frame. At 64 occupied fade tails, a new note is dropped while the selected old voice releases in place, avoiding an abrupt tail replacement with bounded memory. A test-only worker gate deterministically pauses a valid decode, cancels it and verifies the prior playable pad remains READY. Native tests also check the MIDI counters, one-shot end fade and tail-pool overflow. The revised one-shot ending has not yet had a Cubase listening check. The latest draft PR Windows CI passed the build and test checks.

## Checks and features not yet completed

- Cubase listening check of the revised one-shot end fade. The activity flashes were confirmed in Cubase; the anti-click overflow and cancellation paths have deterministic native tests. Direct Cubase CANCEL LOAD observation remains impractical with the small fixtures.
- Cubase stop silenced an active sample, but it remains unclear whether this used the playhead transition or Cubase's MIDI all-notes-off. The engine transport-stop request is covered by a native test.
- Long-session memory stress with the editor open, plus sustained host performance measurement.
- The Phase 2 clean disconnected-Windows installation test is deferred at the owner's request until a suitable VM or second machine is available later in development. The signed offline WebView2 installer and readiness check are prepared; this remains a release gate.

The callback performs no file I/O, JavaScript, routine allocation or blocking lock. Host state writing remains synchronous and can be large; snapshot cost under dense playback and large media requires later hardening.
