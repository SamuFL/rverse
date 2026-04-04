# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- GitHub Actions CI: macOS (Apple Silicon, Ninja) + Windows (VS2022) — builds all plugin formats and runs 42 unit tests on every push/PR (rverse-lxg)
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
