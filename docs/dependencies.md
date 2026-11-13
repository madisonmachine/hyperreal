# Pinned dependencies and obligations

| Component | Version | Use | Source |
| --- | --- | --- | --- |
| JUCE | 9.0.5, commit `d60b7a78030661574a615b586d9325ba7a7fa7dd` | VST3, standalone, native audio and WebView host | https://github.com/juce-framework/JUCE/releases/tag/9.0.5 |
| Microsoft.Web.WebView2 SDK | 1.0.3485.44 | Windows WebView2 headers and static loader for development | https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.3485.44 |
| CMake | 3.22 minimum; 3.31.10 on initial workstation and CI | Configure/build | https://cmake.org/ |
| Visual Studio | 2022 MSVC 19.30 on initial workstation | Windows x64 compiler | Microsoft |

JUCE 9.0.5 is offered under its JUCE commercial terms or AGPLv3. Choose and document the applicable license before distributing the plugin. Do not add unlicensed factory samples, fonts, or images. Review redistribution terms for WebView2 runtime packaging in Phase 2. The Git commit in CMake is the native dependency lock; the UI workspace will add its own package lockfile when created.

Development may download dependencies. No installed runtime may depend on a network connection.
