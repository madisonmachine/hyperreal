# Pinned dependencies and obligations

| Component | Version | Use | Source |
| --- | --- | --- | --- |
| JUCE | 9.0.5, commit `d60b7a78030661574a615b586d9325ba7a7fa7dd` | VST3, standalone, native audio and WebView host | https://github.com/juce-framework/JUCE/releases/tag/9.0.5 |
| Microsoft.Web.WebView2 SDK | 1.0.3485.44 | Windows WebView2 headers and static loader for development | https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.3485.44 |
| WebView2 Evergreen Standalone Installer x64 | Prepared artifact dated 14 November 2026; SHA-256 in `packaging/windows/webview2-runtime.json` | Offline Windows runtime prerequisite | https://learn.microsoft.com/microsoft-edge/webview2/concepts/distribution |
| CMake | 3.22 minimum; 3.31.10 on initial workstation and CI | Configure/build | https://cmake.org/ |
| Visual Studio | 2022 MSVC 19.30 on initial workstation | Windows x64 compiler | Microsoft |

JUCE 9.0.5 is offered under its JUCE commercial terms or AGPLv3. Choose and document the applicable license before distributing the plugin. Do not add unlicensed factory samples, fonts, or images. Review redistribution terms for WebView2 runtime packaging in Phase 2. The Git commit in CMake is the native dependency lock. The UI workspace uses an npm lockfile with pinned React 19.3.1, Vite 8.3.8, TypeScript 7.0.2 and JUCE webview interop 1.0.0.

Development may download dependencies. No installed runtime may depend on a network connection. See `packaging/windows/README.md` for the prepared offline artifact and remaining clean-machine acceptance check.
