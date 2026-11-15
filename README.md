# HYPERREAL

HYPERREAL is a 16-pad drum sampler VST3. This branch is developing Phase 5 with per-pad amplitude and filter envelopes and per-voice filters. The instrument also has native sample loading, a bounded voice engine, non-destructive waveform editing, tuning, and a locally bundled React interface. This is a development build; Phase 4 and 5 Cubase checks are scheduled together at the end of Phase 5.

The [product brief](brief.docx) defines functionality, the [design brief](design-brief.txt) defines the retro local interface, and the [development plan](plan.docx) defines phase order. Dated entries live in [changelogs](changelogs).

## Windows build

Requirements: Visual Studio 2022 with Desktop development with C++, Git, CMake 3.22 or later, Node 22, and Microsoft.Web.WebView2 SDK 1.0.3485.44. The development machine uses CMake 3.31.10. The WebView2 loader is linked statically into the plugin. The WebView2 Runtime is also needed to display the editor; a generic parameter editor is used if the runtime is unavailable.

```powershell
Register-PackageSource -ProviderName NuGet -Name nugetRepository -Location 'https://www.nuget.org/api/v2' -Trusted -Force
Install-Package Microsoft.Web.WebView2 -ProviderName NuGet -Scope CurrentUser -RequiredVersion 1.0.3485.44 -Source nugetRepository -Force -Confirm:$false
npm ci --prefix packages/ui
npm run build --prefix packages/ui
cmake --preset windows-x64
cmake --build --preset windows-debug
cmake --build build/windows-x64 --config Debug --target hyperreal_engine_smoke
cmake --build build/windows-x64 --config Debug --target hyperreal_processor_state_smoke
ctest --test-dir build/windows-x64 -C Debug --output-on-failure
```

The compiled VST3 and standalone app appear under `build/windows-x64/HYPERREAL_artefacts/Debug`. The build does not copy the VST3 into the system plugin directory. JUCE is fetched at a pinned commit during initial development configure. The React bundle is embedded in the binaries and uses no server or CDN at runtime.

## Current slice

- Select one of 16 pads and load a local WAV, AIFF, or FLAC file using the native file chooser. Decode happens on a worker thread. Imports are limited to two channels, 60 seconds, 128 MB decoded per pad and 512 MB for the current 16-pad set.
- MIDI notes 36–51 play their matching pads in one-shot or gated mode. Pad clicks audition the same engine and emit MIDI through a VST3 output bus. Up to four voices per pad overlap in the bounded 64-voice pool.
- Host parameters control master gain, pad performance settings, tuning, amplitude ADSR and filter shaping. Trim, fades and reverse edit the playback region without changing source samples.
- DAW project state embeds all 16 decoded samples and their settings without source paths. Supported earlier state versions remain readable.
- The interface is bundled locally and remains separate from audio processing.

See [Phase 5 development](docs/phase5-development.md) for the implemented slice and remaining shaping work, and [Phase 4 development](docs/phase4-development.md) for deferred Cubase checks. The clean disconnected-machine check from [Phase 2](docs/phase2-slice.md) remains open. Kit persistence, multi-output routing and final retro visuals are later phases.
