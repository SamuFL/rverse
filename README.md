# RVRSE — Rise & Hit Designer

**Free, open-source audio plugin (VST3 / AU / CLAP) built with iPlug2 and C++17.**

RVRSE generates a reverse-reverb riser automatically from any loaded hit sample, then fires the
original hit at a tempo-synced beat boundary. One sample in → complete transition out. No external
audio editing or extra samples needed.

> **Current release:** `v1.1.0`. CI builds universal macOS binaries
> (`arm64` + `x86_64`) and Windows x64 binaries. Release-tag macOS builds ship in a signed and
> notarized installer; Windows builds ship as an unsigned ZIP for manual installation.
> The dark-themed native GUI includes drag-and-drop loading, manual trim, preview, and export.
> See [CHANGELOG.md](./CHANGELOG.md) for the full release history.

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
11. [Platform Support](#platform-support)
12. [Project Structure](#project-structure)
13. [Roadmap — v1.0 Release (Historical)](#roadmap--v10-release-historical)
14. [Project Management](#project-management)
15. [Contributing](#contributing)
16. [License](#license)

---

## Quick Start

```bash
git clone --recursive https://github.com/SamuFL/rverse.git
cd rverse
git config core.hooksPath hooks   # Enable project git hooks
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Build outputs appear under `<build-directory>/out/` — see [Plugin Formats](#plugin-formats).

---

## How It Works

1. **Load** a one-shot hit sample (uncompressed WAV or AIFF) via the **LOAD SAMPLE** button,
   or drag the file onto the header, footer, or either waveform.
2. **Trim** the source on the lower hit waveform by dragging the front/back handles. The
   dimmed outer regions are excluded non-destructively. Release a handle to rebuild; double-click
   either handle to reset that edge.
3. **Preview** the sound either by clicking the waveform-panel **Play** button (centered below the waveform) or by playing a MIDI note.
4. **Export** the current normal riser+hit result from the header **Export** button to a 24-bit WAV file in one step.
5. **Shape the transition** with Riser Release. It lets the riser decay beneath the hit after
   the Beat Anchor; it does not fade or crossfade the dry hit.
6. The riser is your hit sample processed through reverb → reversed → time-stretched to match
   the configured riser length (default: 4 beats at host BPM), then released beneath the hit
   for the requested post-beat duration (default: 50 ms).

```
MIDI Note-On                                         Hit fires here
     │                                                     │
     ▼─── riser plays (reverse-reverb, building up) ──────▼─── dry hit ───▶
     |◄────────── Riser Length (e.g. 4 beats) ────────────►|
```

Because the riser IS the hit reversed and reverbed, the timbral build-up always matches the
impact perfectly.

The upper waveform is one shared timeline for the committed playable sequence: the processed
riser and trimmed hit are drawn at their real offsets, with the effective Riser Release shaded
after the Beat Anchor. The lower hit waveform remains the edit surface for the original sample.
The dry hit's fixed 5 ms technical onset ramp is centered on the Beat Anchor; Riser Release
changes only the riser decay and never attenuates the hit.

---

## Architecture Overview

The codebase is split into two strictly separated layers. This separation is **non-negotiable** —
it prevents audio glitches and real-time safety violations.

### Offline Layer (background thread)

Runs heavy DSP that would be too expensive for real-time. Produces a pre-computed riser buffer.

- Sample loading and decoding (`SampleLoader`)
- Reverb application (`IReverbEngine` via `AirwindowsReverbEngine.h`)
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
| `ReverbEngine.h` | **Offline** | Offline reverb seam (`IReverbEngine`, shared dry/wet mapping) |
| `AirwindowsMatrixVerb.h` | **Offline** | Local Airwindows MatrixVerb DSP port |
| `AirwindowsReverbEngine.h` | **Offline** | Chosen offline reverb adapter and tuning bridge |
| `ReverbEngineFactory.h` | **Offline** | Factory that instantiates the retained Airwindows engine |
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

`RvrseProcessor` rebuilds the riser whenever the sample, BPM, Lush, riser length, or Riser
Release changes. Release-only changes reuse the cached reversed buffer and rebuild from the
stretch stage:

```
Source sample (at native sample rate, e.g. 96 kHz)
    │
    ▼
Resample to DAW output rate          [resampleLinearStereo — BufferUtils.h]
    │
    ▼
Apply reverb                         [IReverbEngine via AirwindowsReverbEngine.h]
    Airwindows MatrixVerb              "Lush" controls the Airwindows tuning range
                                       with a 100/0 → 50/100 dry/wet blend
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
| **CMake** | 3.25+ | Build system generator |
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

RVRSE includes a Catch2 unit test suite covering all DSP modules (71 tests).
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
| **VST3** | `/Library/Audio/Plug-Ins/VST3/` | `C:\Program Files\Common Files\VST3\` |
| **AU** | `/Library/Audio/Plug-Ins/Components/` | *(macOS only)* |
| **CLAP** | `/Library/Audio/Plug-Ins/CLAP/` | `C:\Program Files\Common Files\CLAP\` |
| **Standalone** | `/Applications/` | Build output directory |

Build artifacts are written to `<build-directory>/out/`. Preset examples include
`build/macos-ninja/out/` and `build/windows-vs2022/out/`.

### macOS Installation

Release downloads for macOS ship as a signed and notarized `.pkg` installer.
The installer contains universal `arm64` and `x86_64` binaries and supports
macOS 10.15 or later on Intel Macs and macOS 11 or later on Apple Silicon.
Run the installer and keep the default system-wide install locations unless you
have a specific reason to customize them:

- **VST3:** `/Library/Audio/Plug-Ins/VST3/`
- **AU:** `/Library/Audio/Plug-Ins/Components/`
- **CLAP:** `/Library/Audio/Plug-Ins/CLAP/`
- **Standalone:** `/Applications/`
- **Bundled example samples:** `/Library/Application Support/RVRSE/Examples/`

### Windows Installation

Windows releases ship as `RVRSE-<version>-Windows.zip`. Extract it, then copy
the plugin formats you use to the standard system directories:

- **VST3:** `C:\Program Files\Common Files\VST3\RVRSE.vst3\`
- **CLAP:** `C:\Program Files\Common Files\CLAP\RVRSE.clap`
- **Standalone:** Run `RVRSE.exe` from any writable folder.

Administrator access is required when copying plugins into the system
directories. The ZIP also includes `INSTALL.txt` and the PDF user manual.
Windows binaries are unsigned in this release; a signed installer is planned
for v1.2.0. The Windows ZIP does not currently bundle example samples; the two
example WAV files are included only by the macOS installer.

---

## Usage in a DAW

1. **Insert RVRSE** as a virtual instrument on an instrument/MIDI track.
2. Load an uncompressed WAV or AIFF hit sample (up to 30 seconds, mono or stereo):
   - click **LOAD SAMPLE** and use the file picker; or
   - drag the file onto the plugin header, footer, or either waveform.
   Unsupported and compressed files show a clear error instead of entering the render pipeline.
3. On the lower waveform, drag the left or right trim handle to preview a non-destructive source
   trim. Dimmed audio is excluded. Releasing the handle commits the trim and starts one offline
   rebuild; double-clicking a handle resets that edge. The upper waveform keeps showing the last
   playable sequence until the replacement render is ready.
4. **Trigger playback** either from MIDI or from the waveform-panel **Play** button. The
   reverse-reverb riser starts immediately and the dry hit fires at the Beat Anchor (default:
   four beats after the trigger at host BPM).
5. **Stop playback** either by releasing the MIDI note or by clicking the waveform-panel **Stop**
   button. Both paths use the same 5 ms anti-click fade-out.
6. Set **Riser Release** to control how long the riser continues and fades beneath the dry hit
   after the Beat Anchor. It is an additive riser-only release, not a two-sided crossfade.
7. Click the header **Export** button to save the current normal riser+hit sequence as a stereo
   24-bit WAV. Export uses the committed offline render, ignores Master Volume, and remains
   available during playback. It is disabled while a replacement render is pending.
8. Click the circular **SamuFL logo** in the lower-right corner to open
   [samufl.com](https://samufl.com).

### DAW Parameters

Most parameters are exposed in the DAW's generic editor and can be automated. Riser Release is
persisted but intentionally non-automatable; manual trim is edited on the lower waveform and is
also persisted rather than automated.

| Parameter | Range | Default | Notes |
|---|---|---|---|
| Master Volume | 0–100% | 100% | Overall output level |
| Lush | 0–100% | 40% | Reverb amount — linearly blends from 100/0 to 50/100 dry/wet and triggers offline rebuild |
| Riser Length | 1/4, 1/2, 1, 2, 4, 8, 16 beats (discrete) | 4 | Time-stretch target — triggers offline rebuild |
| Fade In | 0–100% | 60% | Linear ramp over portion of riser length |
| Riser Release | 0–500 ms | 50 ms | Non-automatable post-anchor linear riser decay; commits on gesture end and rebuilds from the cached stretch stage |
| Riser Volume | -60 to +6 dB | 0 dB | Independent riser voice gain |
| Hit Volume | -60 to +6 dB | 0 dB | Independent hit voice gain |
| Stutter Rate | 0–30 Hz | 0 (off) | Per-sample gate rate (also via MIDI CC1) |
| Stutter Depth | 0–1 | 0.5 | Gate depth (also via MIDI CC11) |
| Debug Stage | Normal / Reverbed / Reversed / Riser Only | Normal | Diagnostic: audition intermediate pipeline buffers |
| Stretch Quality | High / Low | High | High = best quality (larger FFT), Low = faster (~2×) for real-time tweaking |

### MIDI Control

The v1.1.0 mappings are fixed:

| MIDI CC | Control | Behavior |
|---|---|---|
| **CC1 (Mod Wheel)** | Stutter Rate | Maps 0–127 to 0–30 Hz |
| **CC11 (Expression)** | Stutter Depth | Maps 0–127 to 0–1 |

Both mappings update the DSP and visible knobs in real time. User-assignable MIDI CC mappings
are planned for [v1.2.0](https://github.com/SamuFL/rverse/issues/31).

## Platform Support

- **macOS is the primary supported platform.** v1.1.0 ships universal Intel and Apple Silicon
  binaries in a signed and notarized installer. Intel requires macOS 10.15 or newer; Apple
  Silicon requires macOS 11 or newer.
- **Windows x64 is supported, but smoke-tested only for v1.1.0.** It ships as an unsigned ZIP
  containing VST3, CLAP, standalone, installation instructions, and the PDF manual.
- **Linux is not supported.** Community-maintained support is welcome through pull requests.

### Not Supported

The following are explicitly outside the RVRSE roadmap:

- **AAX / Pro Tools** — Avid SDK and iLok distribution overhead are not sustainable for this
  free, single-maintainer project.
- **iPadOS / AUv3** — a touch UI, App Store distribution, and ongoing mobile maintenance would
  constitute a separate product.
- **External or sidechain reverb input** — RVRSE intentionally derives the riser from the loaded hit.
- **ARA integration** — not aligned with the focused instrument workflow.
- **Convolution reverb or impulse-response loading** — RVRSE uses its built-in algorithmic reverb.

### Current Product Limitations

- No preset system.
- No pitch shift (planned).
- Single-voice only — overlapping notes cut the previous voice.
- Loading an invalid sample can temporarily produce incorrect UI rendering; audio processing
  remains unaffected ([#69](https://github.com/SamuFL/rverse/issues/69)).
- Riser Release does not always clamp to the available source audio when the hit is trimmed
  ([#71](https://github.com/SamuFL/rverse/issues/71)).
- Exported WAV files do not include the real-time stutter effect
  ([#72](https://github.com/SamuFL/rverse/issues/72)).

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
│   ├── ReverbEngine.h        # Offline reverb seam + shared blend helpers
│   ├── AirwindowsMatrixVerb.h# Local Airwindows MatrixVerb DSP port
│   ├── AirwindowsReverbEngine.h # Chosen Airwindows adapter
│   ├── ReverbEngineFactory.h # Reverb seam factory
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
│   ├── test_reverb.cpp       # Reverb seam + Airwindows regressions
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
├── LICENSE                   # Project MIT license
└── THIRD_PARTY_NOTICES.txt   # Third-party license notices
```

---

## Roadmap — v1.0 Release (Historical)

The tables below summarize the historical v1.0 release plan. The `rverse-*` identifiers are
archived tracker IDs retained for reference from the pre-GitHub-planning era.

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

## Project Management

GitHub is the **single source of truth** for active RVRSE planning and execution:

- [Issues](https://github.com/SamuFL/rverse/issues)
- [v1.1.0 milestone](https://github.com/SamuFL/rverse/milestone/1)
- [v1.2.0 milestone](https://github.com/SamuFL/rverse/milestone/2)
- [RVRSE Development project board](https://github.com/users/SamuFL/projects/1)

The previous Beads tracker is preserved for historical reference only at
[`/.archive/beads-historical/`](./.archive/beads-historical/).

## Contributing

This is a warmup project for the [OpenSampler](https://github.com/SamuFL) initiative. Development
is tracked in GitHub — see the project management links above and `AGENTS.md` for the active
workflow.

### Key Rules

- **Two-layer architecture** must be respected (see [Architecture Overview](#architecture-overview)).
- **No allocations on the audio thread.** Ever.
- **No magic numbers.** All constants live in `Constants.h`.
- **Update CHANGELOG.md** for any user-facing change.
- **Git-flow branching:** `main` → `develop` → `feature/<issue-number>-<short-description>` (for example, `feature/26-hit-preview`).
- **Include the related GitHub issue number in every non-exempt commit message** using `#<number>` format (for example, `Add hit preview button (#26)`).

---

## License

See [LICENSE](./LICENSE) for the project license and
[THIRD_PARTY_NOTICES.txt](./THIRD_PARTY_NOTICES.txt) for bundled third-party notices.
