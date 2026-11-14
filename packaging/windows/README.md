# Phase 2 offline deployment preparation

The Windows release policy is to ship the locally embedded UI and the Microsoft Edge WebView2 Evergreen Standalone Installer x64 with the eventual HYPERREAL installer. The installed instrument must never need a development server, CDN, account or network request. JUCE's generic parameter editor remains available if WebView2 cannot start; native audio does not depend on the editor.

Microsoft documents the [Evergreen Standalone Installer for offline clients](https://learn.microsoft.com/microsoft-edge/webview2/concepts/distribution) and the two registry locations for detecting its runtime. The bootstrapper is an online installer and is not suitable as HYPERREAL's offline prerequisite. Review Microsoft's current redistribution terms when packaging a public release.

## Prepared artifact

The x64 standalone installer was acquired from the Microsoft redirect in [webview2-runtime.json](webview2-runtime.json) on 14 November 2026. It lives locally at `build/tools/webview2/MicrosoftEdgeWebView2RuntimeInstallerX64.exe`; generated build content is ignored by Git. Its Microsoft Authenticode signature, size and SHA-256 were verified. The redirect can change to a newer build, so a future acquisition must update the manifest after verifying the new file.

Run the readiness check after building the UI and Release VST3:

```powershell
./packaging/windows/Test-OfflineReadiness.ps1
```

The check requires the recorded offline installer, the Release VST3 binary and the three local UI assets. It rejects a different installer or invalid signature, detects the installed WebView2 Runtime in the machine/user registry, and reports when the runtime still needs installation. It does not download or install anything.

## Clean-machine acceptance run

Status: deferred at the owner's request until a suitable VM or second machine is available later in development. This remains a release gate.

On a clean Windows 10 or 11 x64 test machine, copy the verified installer and VST3 bundle before disconnecting networking. With networking disabled, detect the runtime using the readiness check. If absent, install the copied `MicrosoftEdgeWebView2RuntimeInstallerX64.exe` using Microsoft's documented `/silent /install` option, then rerun the check. Install the VST3 in the per-user or common VST3 folder, open it in Cubase Pro 15, import and audition a local sample, trigger MIDI note 36, and save/reopen the project with the original sample path unavailable. Record the Windows, Cubase and Runtime versions and whether the editor or generic fallback appears. This acceptance run is still pending; the current machine already has WebView2 installed.
