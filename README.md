# RVRSE — Rise & Hit Designer

**Free, open-source audio plugin (VST3 / AU / CLAP) built with iPlug2 and C++17.**

RVRSE generates a reverse-reverb riser automatically from any loaded hit sample, then fires the
original hit at a tempo-synced beat boundary. One sample in → complete transition out. No manual
editing, no extra samples needed.

> **Status:** Pre-release (`v0.1.0-dev`). Core DSP pipeline, stutter gate, IGraphics GUI, and all
> DAW parameters working. CI running on macOS + Windows. Dark-themed native GUI with waveform
> display, dual control panels, and hit preview. Release packaging still in progress.
> See [CHANGELOG.md](./CHANGELOG.md) and the Release Preparation Epic (`rverse-0v6`) for details.

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [How It Works](#how-it-works)
3. [Architecture Overview](#architecture-overview)
4. [File Map](#file-map)
5. [Sample Loading Flow](#sample-loading-flow)
6. [Offline Pipeline](#offline-pipeline)
7. [Real-Time Playback](#real-time-playback)
8. [Build Instructions](#build-instructions)
9. [Plugin Formats](#plugin-formats)
10. [Usage in a DAW](#usage-in-a-daw)
11. [Project Structure](#project-structure)
12. [Roadmap — v1.0 Release](#roadmap--v10-release)
13. [Contributing](#contributing)
14. [License](#license)

---

## Quick Start

```bash
git clone --recursive https://github.com/SamuFL/rverse.git
cd rverse
git config core.hooksPath hooks   # Enable project git hooks
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The built plugins appear in `build/RVRSE/` — see [Plugin Formats](#plugin-formats) for paths.

---

## How It Works

1. **Load** a one-shot hit sample (WAV or AIFF) via the LOAD SAMPLE button.
2. **Play** a MIDI note — the riser starts immediately and the hit fires at the beat boundary.
3. The riser is your hit sample processed through reverb → reversed → time-stretched to match
   the configured riser length (default: 4 beats at host BPM).

```
MIDI Note-On                                         Hit fires here
     │                                                     │
     ▼─── riser plays (reverse-reverb, building up) ──────▼─── dry hit ───▶
     |◄────────── Riser Length (e.g. 4 beats) ────────────►|
```

Because the riser IS the hit reversed and reverbed, the timbral build-up always matches the
impact perfectly.

---

## Architecture Overview

The codebase is split into two strictly separated layers. This separation is **non-negotiable** —
it prevents audio glitches and real-time safety violations.

### Offline Layer (background thread)

Runs heavy DSP that would be too expensive for real-time. Produces a pre-computed riser buffer.

- Sample loading and decoding (`SampleLoader`)
- Reverb application (`Reverb.h`)
- Buffer reversal (`BufferUtils.h`)
- Time-stretching (`TimeStretch.h`)
- Pipeline orchestration (`RvrseProcessor.h`)

### Real-Time Layer (audio thread)

Runs in `ProcessBlock()` every audio callback. Must be **lock-free** and **allocation-free**.

- Reads from pre-computed riser buffer (read-only during playback)
- Hit playback at calculated beat offset
- Velocity scaling, fade-out envelopes
- MIDI message processing (sample-accurate via `IMidiQueue`)
- Stutter gate (continuous Hz rate, MIDI CC responsive)
- *(Future: pitch shift — computed per-sample)*

### Bridge Between Layers

The two layers communicate via:
- **`std::atomic<bool>` flags** — signal when new data is ready
- **`std::shared_ptr` swaps** — lock-free pointer exchange for buffers
- **One brief mutex** — only held during the one-time sample pointer swap (not per-sample)

```
┌─────────────────────────────────────────────────────────────┐
│  OFFLINE PIPELINE  (background thread)                      │
│                                                             │
│  Load Sample → Reverb → Reverse → Time-Stretch → Fade-Out  │
│                                          ↓                  │
│                              shared_ptr<RiserData>          │
│                              + atomic<bool> ready flag      │
└──────────────────────────────────┬──────────────────────────┘
                                   │ lock-free handoff
┌──────────────────────────────────▼──────────────────────────┐
│  REAL-TIME LAYER  (audio thread, ProcessBlock)              │
│                                                             │
│  Poll ready flag → swap buffer pointer                      │
│  Per-sample: riser voice + hit voice → mix → output         │
└─────────────────────────────────────────────────────────────┘
```

---

## File Map

Every source file belongs to exactly one layer:

| File | Layer | Purpose |
|---|---|---|
| `config.h` | Framework | iPlug2 plugin identity (name, type, I/O, MIDI config) |
| `Constants.h` | Shared | All numeric constants — no magic numbers anywhere |
| `SampleData.h` | Shared | `SampleData` struct: deinterleaved stereo float32 + metadata |
| `SampleLoader.h/.cpp` | **Offline** | Stateless `LoadSample()` — reads WAV/AIFF from disk via dr_wav |
| `Reverb.h` | **Offline** | Schroeder/Moorer reverb (8 comb filters + 4 allpass filters) |
| `BufferUtils.h` | **Offline** | `reverseBuffer`, `resampleLinear`, `applyTailFadeOut` |
| `TimeStretch.h` | **Offline** | Spectral time-stretcher (signalsmith-stretch, MIT) |
| `RvrseProcessor.h` | **Offline** | Pipeline orchestrator — chains all offline stages |
| `Stutter.h` | **Real-Time** | Per-sample trapezoidal gate with continuous Hz rate (audio thread only, MIDI CC responsive) |
| `RVRSE.h` | **Both** | Main plugin class — owns all state, bridges offline ↔ real-time |
| `RVRSE.cpp` | **Both** | Constructor (GUI), `LoadSampleFromFile`, `ProcessBlock`, `OnReset` |
| `WaveformControl.h` | **GUI** | Dual waveform display (riser + hit) and hit preview control |
| `GUIColors.h` | **GUI** | Brand palette, layout constants, text styles |
| `dr_libs_impl.cpp` | Build | Single translation unit for `DR_WAV_IMPLEMENTATION` |

---

## Sample Loading Flow

When the user clicks LOAD SAMPLE:

```
User clicks button
    │
    ▼
PromptForFile() → native OS file dialog (filtered to wav/aif/aiff)
    │
    ▼
LoadSampleFromFile(path)                     [RVRSE.cpp]
    │
    ├── Sets mLoadState = Loading
    ├── Updates UI label → "Loading..."
    │
    └── Spawns background std::thread:
            │
            ├── rvrse::LoadSample(path)      [SampleLoader.cpp]
            │     drwav_init_file() → drwav_read_pcm_frames_f32()
            │     Deinterleaves to SampleData { mLeft[], mRight[] }
            │     Mono files → duplicated to both channels
            │
            ├── Resamples hit to DAW sample rate if needed
            │     (resampleLinearStereo from BufferUtils.h)
            │
            ├── Stores in mHitSample (shared_ptr, under mutex)
            ├── Sets mNewSampleReady = true (atomic flag)
            │
            ├── Feeds ORIGINAL sample to mProcessor.setSample()
            │     (pipeline resamples internally)
            │
            └── Updates UI with: "filename.wav (44100 Hz, stereo, 0.3s)"
```

### Where the sample lives (three distinct copies)

| Pointer | Owner | Written by | Read by | Purpose |
|---|---|---|---|---|
| `mHitSample` | `RVRSE` | Loader thread (under mutex) | ProcessBlock (one-time swap) | Staging area for handoff |
| `mPlaySample` | `RVRSE` | ProcessBlock (audio thread) | ProcessBlock per-sample | **The hit** — audio thread's local copy |
| `mRiserBuffer` | `RVRSE` | ProcessBlock (from processor) | ProcessBlock per-sample | **The riser** — offline pipeline output |

---

## Offline Pipeline

`RvrseProcessor` rebuilds the riser whenever the sample, BPM, Lush, or riser length changes:

```
Source sample (at native sample rate, e.g. 96 kHz)
    │
    ▼
Resample to DAW output rate          [resampleLinearStereo — BufferUtils.h]
    │
    ▼
Apply reverb                         [applyReverbStereo — Reverb.h]
    8 parallel comb filters            Schroeder/Moorer algorithm
    → 4 series allpass filters         "Lush" knob controls feedback + room + damping
    │
    ▼
Cache reversed buffers ★             [Optimisation: skip reverb on BPM-only changes]
    │
    ▼
Reverse the buffer                   [reverseBufferStereo — BufferUtils.h]
    │
    ▼
Time-stretch (spectral)               [stretchBufferStereo — TimeStretch.h]
    Target length = riserLengthBeats × (60 / BPM) × sampleRate
    signalsmith-stretch: polyphonic, transient-aware
    │
    ▼
Tail fade-out                        [applyTailFadeOutStereo — BufferUtils.h]
    1/16 beat — smooths riser→hit boundary
    │
    ▼
Store as shared_ptr<RiserData>
Set mNewRiserReady = true (atomic)
```

**Generation-based abort:** Each rebuild increments a generation counter. If parameters change
mid-rebuild, the running pipeline checks the counter and bails early — no wasted work.

---

## Real-Time Playback

`ProcessBlock()` runs on the audio thread every callback (typically 64–2048 samples):

```
Start of block:
    ├── Read host BPM → update processor if changed
    ├── Check mNewSampleReady flag → swap mPlaySample
    └── Check mNewRiserReady flag → swap mRiserBuffer

Per-sample loop (s = 0 to nFrames):
    │
    ├── Process MIDI at this sample offset
    │     Note-on:  Start riser (mRiserPos = 0)
    │               Arm hit trigger (mSamplesFromNoteOn = 0)
    │               Calculate mHitOffset = riser buffer length
    │     Note-off: Begin 5ms fade-out, kill hit trigger
    │
    ├── Riser voice
    │     Read riser[mRiserPos], apply fade-in envelope
    │     Apply velocity × riser volume gain
    │     Apply stutter gate (per-sample, MIDI CC responsive)
    │     Advance position
    │
    ├── Hit trigger check
    │     If mSamplesFromNoteOn >= mHitOffset → fire hit (mHitPos = 0)
    │
    ├── Hit voice
    │     Read hit[mHitPos], apply velocity × hit volume gain
    │     Apply note-off fade if active
    │     Advance position
    │
    └── Output: (riserL + hitL) × masterVol
                (riserR + hitR) × masterVol
```

---

## Build Instructions

### Prerequisites

| Tool | Minimum Version | Notes |
|---|---|---|
| **CMake** | 3.16+ | Build system generator |
| **C++17 compiler** | Clang 10+ / GCC 9+ / MSVC 2019+ | |
| **Ninja** (recommended) | 1.10+ | Faster than Make; optional |
| **Xcode** (macOS) | 13+ | Required for AU format and `ibtool` |
| **Git** | 2.0+ | Submodules must be initialised |

### Clone (first time)

```bash
git clone --recursive https://github.com/SamuFL/rverse.git
cd rverse
```

If you already cloned without `--recursive`:
```bash
git submodule update --init --recursive
```

### Build (macOS)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DIPLUG_DEPLOY_METHOD=SYMLINK
cmake --build build
```

> **Note:** Use `-DIPLUG_DEPLOY_METHOD=SYMLINK` on macOS with non-Xcode generators
> (Make, Ninja) to avoid a resource deployment race condition. Symlink mode lets
> the DAW read directly from the build output where resources are bundled correctly.

### Build (Windows)

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

### Build (Release)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Running Tests

RVRSE includes a Catch2 unit test suite covering all DSP modules (42 tests).
Tests compile standalone — no iPlug2 or DAW required.

```bash
# Single-config generators (Ninja, Make)
cmake --build build --target rvrse_tests
ctest --test-dir build

# Multi-config generators (Visual Studio, Xcode)
cmake --build build --config Debug --target rvrse_tests
ctest --test-dir build -C Debug

# Or run the test binary directly (verbose output)
./build/tests/rvrse_tests             # Ninja/Make
./build/tests/Debug/rvrse_tests       # Visual Studio

# Run a specific test category
./build/tests/rvrse_tests "[reverb]"
./build/tests/rvrse_tests "[stutter]"
./build/tests/rvrse_tests "[sampleloader]"
```

To disable tests entirely (e.g. CI release builds):

```bash
cmake -B build -DBUILD_TESTING=OFF
```

### Version Management

The canonical version lives in `RVRSE/config.h` (`PLUG_VERSION_STR` and `PLUG_VERSION_HEX`).
After changing it, run the sync script to propagate to plists, installer, and `RVRSE/CMakeLists.txt`:

```bash
python3 scripts/sync-version.py          # apply updates
python3 scripts/sync-version.py --check  # verify (also runs in CI)
```

---

## Plugin Formats

The build produces four plugin formats:

| Format | macOS Install Path | Windows Install Path |
|---|---|---|
| **VST3** | `~/Library/Audio/Plug-Ins/VST3/` | `C:\Program Files\Common Files\VST3\` |
| **AU** | `~/Library/Audio/Plug-Ins/Components/` | *(macOS only)* |
| **CLAP** | `~/Library/Audio/Plug-Ins/CLAP/` | `C:\Program Files\Common Files\CLAP\` |
| **Standalone** | `~/Applications/` | Build output directory |

Build artefacts are in `build/RVRSE/`.

---

## Usage in a DAW

1. **Insert RVRSE** as a virtual instrument on an instrument/MIDI track.
2. Click **LOAD SAMPLE** and select a WAV or AIFF hit sample (up to 30 seconds, mono or stereo).
3. **Play a MIDI note** — the reverse-reverb riser plays immediately, and the dry hit fires
   at the beat boundary (default: 4 beats at host BPM).
4. **Release the note** to fade out both voices (5ms anti-click envelope).

### DAW Parameters

All parameters are exposed in the DAW's generic editor and can be automated:

| Parameter | Range | Default | Notes |
|---|---|---|---|
| Master Volume | 0–100% | 100% | Overall output level |
| Lush | 0–100% | 40% | Reverb amount — triggers offline rebuild |
| Riser Length | 1/4, 1/2, 1, 2, 4, 8, 16 beats (discrete) | 4 | Time-stretch target — triggers offline rebuild |
| Fade In | 0–100% | 60% | Linear ramp over portion of riser length |
| Riser Volume | -60 to +6 dB | 0 dB | Independent riser voice gain |
| Hit Volume | -60 to +6 dB | 0 dB | Independent hit voice gain |
| Stutter Rate | 0–30 Hz | 0 (off) | Per-sample gate rate (also via MIDI CC1) |
| Stutter Depth | 0–1 | 0.5 | Gate depth (also via MIDI CC11) |
| Debug Stage | Normal / Reverbed / Reversed / Riser Only | Normal | Diagnostic: audition intermediate pipeline buffers |
| Stretch Quality | High / Low | High | High = best quality (larger FFT), Low = faster (~2×) for real-time tweaking |

### Current Limitations

- No preset system.
- No pitch shift (planned).
- Single-voice only — overlapping notes cut the previous voice.

---

## Project Structure

```
rverse/
├── RVRSE/                    # Plugin source code
│   ├── RVRSE.h / .cpp        # Main plugin class (GUI + ProcessBlock)
│   ├── config.h              # iPlug2 plugin configuration
│   ├── Constants.h           # All numeric constants
│   ├── GUIColors.h           # Brand palette and layout constants
│   ├── WaveformControl.h     # Waveform display controls (riser + hit + preview)
│   ├── SampleData.h          # Sample data struct
│   ├── SampleLoader.h / .cpp # Audio file loading (dr_wav)
│   ├── Reverb.h              # Schroeder/Moorer reverb
│   ├── BufferUtils.h         # Buffer utilities (reverse, resample, fade)
│   ├── TimeStretch.h         # Spectral time-stretcher (signalsmith-stretch)
│   ├── RvrseProcessor.h      # Offline pipeline orchestrator
│   ├── Stutter.h             # Real-time stutter gate (audio thread only)
│   ├── dr_libs_impl.cpp      # dr_wav implementation unit
│   ├── libs/dr_libs/         # Header-only audio codec library
│   └── resources/            # Fonts, images, plugin resources
├── iPlug2/                   # iPlug2 framework (git submodule)
├── build/                    # CMake build output (not committed)
├── CMakeLists.txt            # Top-level CMake configuration
├── tests/                    # Catch2 unit tests (standalone, no iPlug2)
│   ├── CMakeLists.txt        # Test target configuration
│   ├── test_smoke.cpp        # Smoke tests (framework + basic DSP)
│   ├── test_constants.cpp    # Constants.h relational invariants
│   ├── test_buffer_utils.cpp # Reverse, resample, fade, trim
│   ├── test_reverb.cpp       # Schroeder reverb properties
│   ├── test_time_stretch.cpp # Spectral stretcher factors + edge cases
│   ├── test_stutter.cpp      # Gate symmetry, convergence, phase
│   └── test_sample_loader.cpp# WAV loading, deinterleave, error handling
├── RVRSE_BRIEF.md            # Full product specification
├── GUI_DESIGN_BRIEF.md       # GUI design specification
├── STITCH-DESIGN.md          # Visual design system
├── UAT_PLAYBOOK.md           # Manual test scenarios and pass criteria
├── CHANGELOG.md              # Release notes (Keep a Changelog format)
├── AGENTS.md                 # AI agent instructions and workflow rules
├── docs/prototypes/          # Archived HTML/PNG design prototypes
└── LICENSE                   # MIT license
```

---

## Roadmap — v1.0 Release

Tracked as the **Release Preparation Epic** (`rverse-0v6`). Run `bd children rverse-0v6` for
the full dependency tree.

### Phase 1 — Critical Fixes

| Issue | Priority | Description | Status |
|-------|----------|-------------|--------|
| `rverse-g67` | P2 | Replace all AcmeInc placeholder branding with SamuFL identity | Open |
| `rverse-jwf` | P1 | Replace installer license.rtf placeholder with actual MIT license | ✅ Done (PR #7) |
| `rverse-lxg` | P0 | Set up GitHub Actions CI (Windows + macOS) | ✅ Done |

### Phase 2 — Feature Completion

| Issue | Priority | Description | Status |
|-------|----------|-------------|--------|
| `rverse-nqg` | P1 | Expose DSP params (Lush, Riser Length, Fade In, Riser Volume, Hit Volume) | ✅ Done (PR #5) |
| `rverse-l9x` | P1 | Debug playback mode — expose intermediate pipeline buffers | ✅ Done (in PR #5) |
| `rverse-g4j` | P1 | Upgrade time-stretcher to signalsmith-stretch (spectral) | ✅ Done (PR #8) |
| `rverse-ebv` | P1 | Build full IGraphics GUI (dark theme) | ✅ Done |
| `rverse-bzs` | P2 | Implement Riser Tune + Hit Tune (pitch shift) | Open |
| `rverse-7dr` | P1 | Persist loaded sample path across sessions (state save/restore) | ✅ Done (PR #6) |

### Phase 3 — Documentation & Polish

| Issue | Priority | Description | Status |
|-------|----------|-------------|--------|
| `rverse-6fl` | P0 | Add Catch2 test framework and tests/ directory | ✅ Done |
| `rverse-2uq` | P0 | Unit tests for existing DSP modules | ✅ Done (42 tests) |
| `rverse-zvc` | P2 | Write user manual (LaTeX → PDF) | Blocked by `rverse-ebv` |
| `rverse-jaj` | P2 | Automate version number sync (plists, installer, config.h) | Done |
| `rverse-nwe` | P2 | Add Pluginval to CI pipeline | Open |
| `rverse-k4o` | P3 | Add CONTRIBUTING.md and CODE_OF_CONDUCT.md | Open |

### Phase 4 — QA & Release

| Issue | Priority | Description | Blocked by |
|-------|----------|-------------|------------|
| `rverse-2rc` | P2 | Validate on macOS (Cubase, Studio One, Logic) | — |
| `rverse-46w` | P2 | Validate on Windows (Cubase, Studio One) | — |
| `rverse-t28` | P2 | Tag v1.0.0 and publish GitHub release | `rverse-jaj` |
| `rverse-0iz` | P3 | Set up platform installers (InnoSetup + macOS pkg) | `rverse-g67`, `rverse-jwf` |

### Nice-to-Have (post-v1.0 or if time permits)

| Issue | Priority | Description |
|-------|----------|-------------|
| `rverse-0gq` | P3 | Add tempo-synced stutter rate option |
| `rverse-ttr` | P3 | Clean up Windows packaging scripts |
| `rverse-rar` | P3 | Fix placeholder content in installer RTFs |

---

## Contributing

This is a warmup project for the [OpenSampler](https://github.com/SamuFL) initiative. Development
is tracked via [Beads](https://github.com/steveyegge/beads) issue tracker — see `AGENTS.md` for
the full workflow.

### Key Rules

- **Two-layer architecture** must be respected (see [Architecture Overview](#architecture-overview)).
- **No allocations on the audio thread.** Ever.
- **No magic numbers.** All constants live in `Constants.h`.
- **Update CHANGELOG.md** for any user-facing change.
- **Git-flow branching:** `main` → `develop` → `feature/<beads-id>-description`.

---

## License

See [LICENSE](./LICENSE) for details.
