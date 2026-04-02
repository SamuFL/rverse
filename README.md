# RVRSE — Rise & Hit Designer

**Free, open-source audio plugin (VST3 / AU / CLAP) built with iPlug2 and C++17.**

RVRSE generates a reverse-reverb riser automatically from any loaded hit sample, then fires the
original hit at a tempo-synced beat boundary. One sample in → complete transition out. No manual
editing, no extra samples needed.

> **Status:** Pre-release (`v0.1.0-dev`). Core DSP pipeline working. Stutter gate, GUI polish,
> and CI still in progress. See [CHANGELOG.md](./CHANGELOG.md) for details.

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
12. [Contributing](#contributing)
13. [License](#license)

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
| `TimeStretch.h` | **Offline** | OLA time-stretcher with Hann windowing |
| `RvrseProcessor.h` | **Offline** | Pipeline orchestrator — chains all offline stages |
| `Stutter.h` | **Real-Time** | Per-sample trapezoidal gate with continuous Hz rate (audio thread only, MIDI CC responsive) |
| `RVRSE.h` | **Both** | Main plugin class — owns all state, bridges offline ↔ real-time |
| `RVRSE.cpp` | **Both** | Constructor (GUI), `LoadSampleFromFile`, `ProcessBlock`, `OnReset` |
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
Time-stretch via OLA                 [stretchBufferStereo — TimeStretch.h]
    Target length = riserLengthBeats × (60 / BPM) × sampleRate
    Hann window, 50% overlap
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
    │     Read riser[mRiserPos], apply velocity + fade
    │     Advance position
    │
    ├── Hit trigger check
    │     If mSamplesFromNoteOn >= mHitOffset → fire hit (mHitPos = 0)
    │
    ├── Hit voice
    │     Read hit[mHitPos], apply velocity + fade
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
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

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

### Current Limitations

- No GUI knobs yet — parameters like Lush, Riser Length, and Stutter are not yet exposed in the UI (use DAW generic editor or MIDI CC).
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
│   ├── SampleData.h          # Sample data struct
│   ├── SampleLoader.h / .cpp # Audio file loading (dr_wav)
│   ├── Reverb.h              # Schroeder/Moorer reverb
│   ├── BufferUtils.h         # Buffer utilities (reverse, resample, fade)
│   ├── TimeStretch.h         # OLA time-stretcher
│   ├── RvrseProcessor.h      # Offline pipeline orchestrator
│   ├── Stutter.h             # Real-time stutter gate (audio thread only)
│   ├── dr_libs_impl.cpp      # dr_wav implementation unit
│   ├── libs/dr_libs/         # Header-only audio codec library
│   └── resources/            # Fonts, images, plugin resources
├── iPlug2/                   # iPlug2 framework (git submodule)
├── build/                    # CMake build output (not committed)
├── CMakeLists.txt            # Top-level CMake configuration
├── RVRSE_BRIEF.md            # Full product specification
├── CHANGELOG.md              # Release notes (Keep a Changelog format)
├── AGENTS.md                 # AI agent instructions and workflow rules
└── LICENSE                   # Project license
```

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
