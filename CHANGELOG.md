# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
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
