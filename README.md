# HYPERREAL

HYPERREAL is a 16-pad drum sampler VST3. Development is currently in Phase 1. The current plugin is a silent shell with 16 pad-level parameters and a temporary generic editor; sample playback and the local React interface are planned for later phases.

The source of truth for scope is [brief.docx](brief.docx). [design-brief.txt](design-brief.txt) supplies the retro-hardware design and offline architecture. [plan.docx](plan.docx) gives the phase order. The user-requested change record lives in [changelogs](changelogs).

## Windows build

Requirements: Visual Studio 2022 with the Desktop development with C++ workload, Git, CMake 3.22 or later, and Microsoft.Web.WebView2 SDK 1.0.3485.44 installed for the current user. CMake fetches JUCE at the pinned commit on first configure, so development setup needs network access. Installed plugin operation will be offline.

```powershell
Register-PackageSource -ProviderName NuGet -Name nugetRepository -Location 'https://www.nuget.org/api/v2' -Trusted -Force
Install-Package Microsoft.Web.WebView2 -ProviderName NuGet -Scope CurrentUser -RequiredVersion 1.0.3485.44 -Source nugetRepository -Force -Confirm:$false
cmake --preset windows-x64
cmake --build --preset windows-debug
```

The VST3 and standalone artefacts appear under `build/windows-x64/HYPERREAL_artefacts/Debug`. The build does not copy the VST3 into the system plugin directory. Run the standalone target for a quick shell smoke check. For Cubase, copy the built VST3 bundle to the common VST3 directory only when ready for host testing, then rescan plugins.

The bundled Visual Studio CMake 3.21 on the initial workstation is too old for JUCE 9.0.5. A user-local CMake 3.31.10 was installed for development.

## Project structure

- `apps/plugin`: JUCE host-facing shell.
- `packages/engine`: native DSP and pad contract.
- `packages/ui`, `packages/visuals`, `packages/bridge`: local interface and bridge.
- `packages/kits`: portable kit model and schema.
- `assets`: locally packaged fonts, art, and factory content.
- `tests`: DSP, state, interface, performance, and host integration checks.
- `packaging`: Windows and macOS distribution work.
- `docs`: contracts, requirements, architecture, and build decisions.
- `changelogs`: timestamped Markdown change records.

No factory sample content or third-party assets have been added.
