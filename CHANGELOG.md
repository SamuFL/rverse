# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Full IGraphics GUI with dark theme (dark green/blue palette, gold/blue accents) — responsive layout with corner resizer (rverse-ebv)
- Dual waveform display: riser (gold) + hit (blue) with shared amplitude normalization and animated playhead (rverse-ebv)
- Hit preview waveform in hit panel showing original loaded sample (rverse-ebv)
- Riser panel: real-time section (Stutter Rate, Stutter Depth, Riser Volume — gold knobs) + offline section (Lush, Riser Length, Fade In — steel knobs, Stretch Quality toggle) (rverse-ebv)
- Hit panel: volume knob (blue accent), hit waveform preview, logo, donate/support button (rverse-ebv)

### Changed
- Replaced all AcmeInc placeholder branding with SamuFL identity across plists, installer files, and config (rverse-g67)
- Updated user-facing URL to https://samufl.com and contact email to info@samufl.com (rverse-g67)
- Updated config.h and related metadata toward the 0.1.0 release version (rverse-g67)
- Debug Stage parameter hidden/disabled in release builds via `#ifndef NDEBUG` while its parameter slot remains reserved for index stability — intermediate pipeline buffers no longer allocated in release (rverse-0v6.1)
- Header zone: plugin title, Load Sample button with native file dialog, sample info display, BPM display (rverse-ebv)
- Footer zone: master volume slider with label/value, MIDI activity indicator (blue dot), version string (rverse-ebv)
- Resize support: corner resizer with smart layout scaling — waveform absorbs squeeze, panels maintain minimum heights (rverse-ebv)
- Stretch Quality parameter (High / Low) — defaults to High for best audio quality; Low mode (~2× faster) available for real-time tweaking or resource-limited systems (rverse-g4j)
- Riser Length is now a discrete parameter with 7 musical values (1/4, 1/2, 1, 2, 4, 8, 16 beats) instead of continuous 0.25–16 range — snaps to exact beat divisions (rverse-ebv)
- Replaced OLA time-stretcher with signalsmith-stretch (MIT, spectral, polyphonic-aware) for significantly better audio quality at all stretch ratios — transient smearing at riser end eliminated (rverse-g4j)
- Stretch-only rebuilds (riser length, BPM changes) run synchronously during offline/bounce rendering so automation is respected immediately; async during real-time playback (rverse-g4j)

### Fixed
- Plugin GUI crash (SIGABRT in NanoVG font rendering) caused by missing font resource in deployed VST3/AU bundles — switched macOS non-Xcode deployment from COPY to SYMLINK to avoid iPlug2 resource bundling race condition (pre-existing bug)

### Added
- Persist loaded sample path across DAW sessions — save/restore via `SerializeState`/`UnserializeState` with versioned chunk format. Shows "Missing: filename" if file is gone on reload. (rverse-7dr)
- GitHub Actions CI: macOS (Apple Silicon, Ninja) + Windows (VS2022) — builds all plugin formats and runs 42 unit tests on pushes to develop/main and PRs targeting them (rverse-lxg)
- Artifact publishing: rolling `develop-latest` pre-release (Debug zips) on develop push, and proper GitHub Release (Release zips) on `v*` tags — macOS + Windows per-platform downloads (rverse-6m5)
- Expose 5 new DAW-automatable parameters via generic editor: Lush (0–100%), Riser Length (0.25–16 beats), Fade In (0–100%), Riser Volume (-60 to +6 dB), Hit Volume (-60 to +6 dB) (rverse-nqg)
- Debug Stage parameter (Normal / Reverbed / Reversed / Riser Only) — exposes intermediate pipeline buffers for diagnostic playback without hit (rverse-l9x)
- Riser fade-in envelope controlled by Fade In parameter — linear ramp over configurable portion of riser length (rverse-nqg)
- Independent Riser Volume and Hit Volume knobs with dB scaling — both voices fully controllable (rverse-nqg)
- Lush and Riser Length changes propagated to offline pipeline in real-time — riser rebuilds on parameter change (rverse-nqg)
- DSP unit tests: 42 tests across 7 files covering BufferUtils, Reverb, TimeStretch, Stutter, SampleLoader, and Constants (rverse-2uq)
- `trimTrailingSilence` / `trimTrailingSilenceStereo` in BufferUtils.h — removes near-silent tail samples below a configurable threshold (rverse-0d0)
- `kReverbTailSeconds` (5.0s) and `kSilenceThreshold` (-30 dB) constants in Constants.h (rverse-0d0)
- Catch2 v3.7.1 test framework with CTest integration — `cmake --build build --target rvrse_tests && ctest --test-dir build` (rverse-6fl)
- Smoke tests for DSP headers: Constants, BufferUtils, TimeStretch, Stutter (rverse-6fl)
- Real-time stutter gate in `Stutter.h` — per-sample trapezoidal gate with continuous Hz rate (0–30 Hz) and anti-click ramps (rverse-n84)
- Stutter Rate (Hz) and Stutter Depth exposed as DAW-automatable parameters (rverse-n84)
- MIDI CC control: CC1 (mod wheel) → Stutter Rate, CC11 (expression) → Stutter Depth (rverse-6r2)
- Git hooks: commit-msg (beads ID enforcement), pre-commit (conflict markers, secrets), pre-push (git-flow protection) (rverse-e1u)
- Riser + hit dual-voice playback: note-on plays reverse-reverb riser, dry hit fires at beat offset (rverse-djb)
- Offline-to-realtime handoff: `RvrseProcessor` wired into RVRSE class with lock-free riser buffer consumption (rverse-1na)
- `RvrseProcessor.h` — offline pipeline orchestrator: reverb → reverse → stretch with generation-based abort and cached intermediate buffers (rverse-62q)
- `TimeStretch.h` — OLA time-stretcher with Hann windowing + `calcStretchFactor()` helper (rverse-07s)
- `BufferUtils.h` — in-place stereo buffer reversal, linear resampling, tail fade-out utilities (rverse-cmf)

### Fixed
- Installer license.rtf replaced with actual MIT license — was iPlug2 placeholder with no legal basis (rverse-jwf)
- **Critical:** Reverb tail was truncated — output buffer was same size as input, producing no reverb tail. Now pads source with 5 seconds of silence so the Schroeder reverb rings out naturally, then trims trailing silence. This is the core of the reverse-reverb effect. (rverse-0d0)
- Hit sample now resampled to DAW output rate on load — fixes pitched-down playback with 96kHz+ files (rverse-djb)
- Offline pipeline resamples source to output rate before processing — riser length now correct regardless of file sample rate (rverse-djb)
- Riser tail fade-out (1/16 beat) prevents reversed transient from clashing with hit (rverse-djb)
- Hit no longer re-triggers on MIDI note-off — `mSamplesFromNoteOn` counter now reset (rverse-djb)
- Host BPM now read via `GetTempo()` and propagated to offline pipeline — riser re-stretches on tempo change (rverse-djb)
- `kMaxSampleFrames` now supports up to 192kHz files (was limited to 48kHz) (rverse-djb)
- Schroeder/Moorer algorithmic reverb in `Reverb.h` — 8 parallel comb filters feeding 4 series allpasses (rverse-6el)
- `applyReverb()` and `applyReverbStereo()` stateless functions for the offline pipeline
- Reverb constants in `Constants.h` — room size, feedback, damping, allpass gain
- MIDI note-on triggers sample playback with velocity scaling (rverse-p38)
- Sample-accurate MIDI timing via `IMidiQueue`
- 5ms note-off fade-out envelope (anti-click)
- Sample loading via dr_libs (WAV + AIFF support) (rverse-g1x)
- `SampleData.h` — deinterleaved stereo float32 sample buffer struct
- `SampleLoader.h/.cpp` — stateless offline loading functions
- "LOAD SAMPLE" button in GUI with native file dialog
- Sample info display (filename, sample rate, channels, duration)
- dr_libs headers (dr_wav.h, dr_flac.h) in `RVRSE/libs/dr_libs/`
- iPlug2OOS project scaffold with CMake build (rverse-03y)
- Plugin configured as MIDI instrument (VST3 / AU / CLAP)
- Dark-themed placeholder GUI with RVRSE title
- `Constants.h` with all MIDI CC defaults and parameter ranges
