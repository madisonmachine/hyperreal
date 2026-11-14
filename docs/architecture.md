# Architecture decisions

## ADR 0001 Local audio and interface boundary

Status: accepted for Phase 1, 13 November 2026.

The product brief defines functionality. Its native-only UI proposal is superseded by the later design brief and development plan: a locally bundled React and TypeScript interface embedded through JUCE WebBrowserComponent. Native C++ remains authoritative for DSP, parameters, MIDI, buses, state, sample and kit I/O. The WebView is a display and interaction layer. The plugin must play when the editor is closed or unavailable.

Phase 1 used JUCE's generic parameter editor solely to verify host loading and stable parameter IDs. The Phase 2 branch embeds a local React WebView slice for pad 1. A generic editor remains available when WebView2 is unavailable. The production bundle is served by a local resource provider.

## ADR 0002 Dependency and build baseline

Status: accepted for Phase 1, 13 November 2026.

Use C++20, CMake 3.22 or later, Visual Studio 2022 x64, and JUCE 9.0.5 pinned to commit `d60b7a78030661574a615b586d9325ba7a7fa7dd`. The JUCE 9.0.5 headers expose `withNativeFunction`, `withResourceProvider`, and WebView2 support. WebView2 runtime packaging and redistribution remain Phase 2 verification tasks. Native audio must remain independent of WebView availability.

Use ordinary CMake targets, one UI workspace when created, and no service or package framework. GitHub is for source and CI only; the installed instrument must operate offline.

## ADR 0003 State authority

Status: provisional, 13 November 2026.

Host parameters use immutable string IDs with explicit version suffixes. The Phase 1 shell publishes `master.gain.v1` and `pad.01.level.v1` through `pad.16.level.v1`. The Phase 2 host state uses a versioned binary envelope containing a JUCE parameter ValueTree and embedded decoded sample data. The full kit and project-state schemas in `schemas` remain design contracts for later phases, not yet implemented runtime serialization. Schema migrations must be added before a breaking format change. A future parameter must receive a new ID; existing IDs must not be reused for changed semantics.

The audio callback may read prepared state but must not do file I/O, allocate routinely, wait for locks, execute JavaScript, or depend on rendering.
