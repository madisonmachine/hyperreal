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
- Cubase initially displayed the generic parameter editor. Static WebView2 loader linkage removed an unbundled `WebView2Loader.dll` dependency, but Cubase still took the generic fallback. A temporary host diagnostic confirmed that `areOptionsSupported` returned false in Cubase. The check now uses the same writable WebView2 data folder as the actual editor. After reinstalling, Cubase displayed the custom 16-pad interface. The rebuilt Release VST3 passed pluginval level 5 editor tests.
- In Cubase Pro 15, LOAD SAMPLE selected a generated local WAV, the editor reported READY, and AUDITION played audibly. A saved Cubase project restored the sample and AUDITION remained audible while the source WAV was moved away from its original path. The generated WAV was then restored.
- The pull request CI run for the editor fix passed the Release build, native tests and pluginval validation.
- Prepared the Microsoft x64 Evergreen Standalone Installer in the ignored build area. Its Authenticode signature is valid, and `packaging/windows/Test-OfflineReadiness.ps1` passed checks of its recorded SHA-256, the Release VST3, the local UI assets and the installed Runtime. This was an artifact readiness check on the development machine, not a clean offline installation.
- In Cubase Pro 15, a saved HYPERREAL project showed READY after reopening. A four-note MIDI 36 clip played four audible tones while the editor was closed, and the custom editor reopened afterward.
- The built UI entry page has no external URLs. Its JavaScript contains only React diagnostic and XML namespace URLs, with no application fetch, XHR or WebSocket calls in the UI source. No Vite or similar development server was running at inspection time. This supports local operation but does not prove a disconnected launch.

## Remaining Phase 2 checks

- Verify installed operation on a clean machine with networking disabled. Include the prepared WebView2 standalone prerequisite in the eventual installer and review current redistribution terms.
- The Windows computer-use helper was unavailable during this pass, so the Cubase interaction checks were performed by the project owner.
