# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- `TimeStretch.h` — OLA time-stretcher with Hann windowing + `calcStretchFactor()` helper (rverse-07s)
- `BufferUtils.h` — in-place stereo buffer reversal for the offline pipeline (rverse-cmf)
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
