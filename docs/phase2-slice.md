# Phase 2 native and interface slice

Status: in progress, 14 November 2026.

The first pad plays a single loaded mono or stereo sample from MIDI note 36, using its MIDI sample offset and velocity. AUDITION requests are transferred by an atomic counter and begin at the next audio block. WAV, AIFF and FLAC decode on a worker. Invalid imports preserve the current sample. The prototype has one voice and plays one-shot; overlap, gate mode, polyphony and choke are Phase 3.

The React and TypeScript UI is built with a pinned npm lockfile, compiled to fixed asset names, and embedded as JUCE BinaryData. WebBrowserComponent serves only `/`, `/assets/app.js`, and `/assets/style.css` through a local resource provider. The UI calls four native functions: `getSnapshot`, `chooseSample`, `audition`, and `setMasterGain`. The snapshot and gain command carry `schemaVersion: 1`. Native code validates the gain payload and sends host begin/change/end gestures. The WebView can be closed without stopping audio. The generic editor is a fallback if WebView2 is unavailable.

Host state version 2 embeds decoded float samples alongside the JUCE parameter state, so the source path is not needed for recall. The Phase 1 version 1 parameter-only state is still accepted. Current state writes are synchronous on the host state callback; larger media and concurrent snapshot behaviour need further host testing before release.

The audio callback reads immutable sample data and prepared atomics. It does no file I/O, JavaScript, routine allocation or blocking lock. Old sample buffers are retired by the worker when no callback or voice can reference them.

## Verification completed

- TypeScript check and production Vite build passed.
- Debug and Release VST3 and standalone builds passed.
- pluginval strictness 5 passed both headless and with editor creation tests.
- Debug and Release native smoke tests passed import, sample-offset MIDI, embedded sample restore and invalid-import preservation.
- Debug and Release full processor recall tests passed after deleting the original WAV, including restored master gain and audible output.
- The development machine reports WebView2 Runtime 157.0.4341.46.
- GitHub Actions passed on `main` for the Phase 1 shell and on pull request #1 for this slice, including the Release build, native tests and headless pluginval validation.
- The current Release VST3 replaced the superseded per-user v0.1.0 installation. Cubase Pro 15's `vstscanner.exe` reported HYPERREAL v0.2.0 as an Instrument and exited successfully. The installed binary matches the tested Release build by SHA-256.

## Remaining Phase 2 checks

- Verify installed operation on a clean offline machine and package an offline WebView2 prerequisite for the eventual installer. Microsoft documents the Evergreen Standalone Installer for offline deployment: https://learn.microsoft.com/microsoft-edge/webview2/concepts/distribution
- Open the installed instrument in Cubase Pro 15; inspect the editor, select and audition a sample, then exercise project recall and editor closure. The Windows computer-use helper was unavailable during this pass. Scanner recognition does not establish these behaviours.
