# Requirement and phase checklist

The product brief is authoritative for instrument behaviour. The design brief adds retro visual direction and a fully local architecture. The plan orders delivery. A checked item requires implementation and evidence, not just a document.

| Requirement | Source | Phase | Status |
| --- | --- | --- | --- |
| Monorepo, reproducible build, VST3 and standalone shell | Product brief, plan | 1 | Local and GitHub Actions builds passed |
| Stable parameter IDs and versioned state contracts | Both briefs, plan | 1 | Initial contracts recorded |
| CI, changelog and documented build | Product brief, plan | 1 | Main and Phase 2 pull request runs passed |
| Local React/WebView bundle and native bridge | Design brief, plan | 2 | Cubase editor opened; LOAD and AUDITION passed |
| One-pad import, MIDI, audition, offline use and recall | Product brief, plan | 2 | Cubase import, audition and source-free project recall passed; offline prerequisite prepared; MIDI lifecycle and clean offline checks pending |
| Sixteen pads, MIDI 36-51, sample operations and voices | Product brief | 3 | Pending |
| One-shot/gated, velocity, polyphony, choke, mute and solo | Product brief | 3 | Pending |
| Non-destructive trim, fades, reverse and tuning | Product brief | 4 | Pending |
| Amplitude/filter envelopes, filters, LFO and automation | Product brief | 5 | Pending |
| Mixer and up to 16 separate stereo pad outputs | Product brief | 6 | Pending |
| Two DAW multi-output and state checks, including Cubase | Product brief, plan | 6, 9 | Pending |
| Factory Kits, My Kits, portable media and DAW recall | Product brief | 7 | Pending |
| Original 1990s hardware styling and performance fallbacks | Design brief | 8 | Pending |
| Host validation, measured CPU/GPU and offline regression | Both briefs | 9 | Pending |
| Offline Windows installer preserving user kits | Both briefs | 10 | Pending |
| macOS VST3/AU after platform validation | Product brief, plan | 10 | Pending |

Initial release exclusions: sequencer, song arrangement, cloud, online accounts, sample layering, large effects rack, and advanced time-preserving pitch shift. Optional pitch detection and compressors must not delay required features.
