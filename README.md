# HYPERREAL

HYPERREAL is a 16-pad drum sampler VST3. The Phase 3 foundation gives each pad an independent sample slot for MIDI notes 36–51, a bounded overlapping voice pool, native sample loading and audition, and a local React interface. This branch is a prototype; pad operations, adjustable velocity and polyphony, and the final visual design remain in development.

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
- MIDI notes 36–51 play their matching pads as one-shots from scheduled sample offsets. The UI AUDITION control uses the same engine. Up to four voices per pad overlap in the current 64-voice pool.
- Master gain is a host parameter with a versioned bridge command and automation gesture. Other pad parameters are reserved but do not affect audio yet.
- DAW project state embeds all 16 decoded samples and restores them without source paths. Phase 1 parameter-only and Phase 2 one-sample states remain readable.
- The interface is bundled locally and remains separate from audio processing.

See [Phase 3 foundation](docs/phase3-foundation.md) for test evidence and unfinished Phase 3 checks. The clean disconnected-machine check from [Phase 2](docs/phase2-slice.md) remains open. Kit persistence, routing, waveform editing and final retro visuals are later phases.
