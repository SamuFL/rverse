# RVRSE — Rise & Hit Designer
### Warmup Project Brief · iPlug2 / C++ · VST3 · AU · CLAP

---

## Vision

RVRSE is a free, open-source one-shot sample plugin that automatically generates a reverse-reverb riser from any loaded hit sample, then fires the original hit at the exact moment defined by a user-set beat length. The riser and hit are always perfectly aligned — no manual editing, no extra samples needed.

This is the **warmup project** for the OpenSampler initiative. Its purpose is to validate the iPlug2 toolchain, establish the CI/CD workflow, and produce a genuinely useful, releasable plugin before committing to the larger sampler project.

---

## Core Concept

### The Signal Chain

Everything flows through two distinct processing layers: an **offline pipeline** (triggered when samples load or heavy parameters change) and a **real-time layer** (running every audio block, responsive to MIDI CC).

```
┌─────────────────────────────────────────────────────────────┐
│  OFFLINE PIPELINE  (pre-processed, runs off audio thread)   │
│                                                             │
│  Load Sample → Lush (reverb) → Reverse → Time-Stretch       │
│                                             ↓               │
│                                    final_riser[]            │
└─────────────────────────────────────────────────────────────┘
                                             ↓
┌─────────────────────────────────────────────────────────────┐
│  REAL-TIME LAYER  (per-sample, audio thread)                │
│                                                             │
│  Playback of final_riser[] → Stutter Gate → Fade Envelope   │
│                                             +               │
│  Playback of dry hit[] (fires at beat X)                    │
│                                             ↓               │
│                                       Audio Output          │
└─────────────────────────────────────────────────────────────┘
```

### Why This Split?

| Layer | Parameters | Reason |
|---|---|---|
| Offline | Lush, Riser Length | Reverb + time-stretch require full buffer rebuilds — too expensive for real-time |
| Real-time | Stutter Rate, Stutter Depth, all others | Lightweight gate math — must respond instantly to MIDI CC for expressiveness |

### Timing Model

RVRSE reads the host BPM via iPlug2's `ITimeInfo`. When a MIDI note-on arrives:

1. Riser playback begins immediately from `final_riser[]`
2. The plugin counts forward exactly `(Riser Length × samplesPerBeat)` samples
3. At that exact sample offset, the dry hit fires — perfectly on the beat
4. Both voices release naturally

> **The core idea:** because the riser IS the hit reversed and reverbed, the timbral build-up always matches the impact perfectly. One sample → complete transition. No sample hunting, no mismatched textures.

---

## Parameters

### Riser Section — Offline Parameters
> Changes to these parameters trigger a buffer rebuild off the audio thread. Expect a short recalculation delay (acceptable).

| Parameter | Range | Default | Description |
|---|---|---|---|
| Lush | 0 – 100% | 40% | Reverb wet amount + room size (linked). 0 = dry reversed sample. 100 = fully washed-out reverb tail, reversed. |
| Riser Length | 0.25 – 16 beats | 4 beats | How many beats before the hit the riser begins. Tempo-synced via host BPM. Snaps to musical values (1/4, 1/2, 1, 2, 4, 8, 16). |

### Riser Section — Real-Time Parameters
> These are computed per-sample in the audio thread. Full MIDI CC support. Changing them is instantaneous with no glitches.

| Parameter | Range | Default | MIDI CC (default) | Description |
|---|---|---|---|---|
| Stutter Rate | 1/32 – 1/2 note | OFF | CC 1 (mod wheel) | Rhythmic chop rate applied to the riser during playback. Tempo-synced. OFF = stutter disabled. |
| Stutter Depth | 0 – 100% | 50% | CC 11 (expression) | Wet/dry mix of the stutter gate. 0 = no stutter audible even if Rate is set. |
| Fade In | 0 – 100% | 60% | — | Shape of the riser volume ramp from silence to full over the riser duration. |
| Riser Tune | -24 – +24 st | 0 st | CC 2 | Pitch-shifts the riser independently. Applied as a stretch factor during playback. |

### Hit Section

| Parameter | Range | Default | Description |
|---|---|---|---|
| Hit Volume | -inf – +6 dB | 0 dB | Level of the dry original sample when it fires. |
| Hit Tune | -24 – +24 st | 0 st | Pitch-shifts the hit. Simple linear resampling for MVP. |

### Global

| Parameter | Range | Default | Description |
|---|---|---|---|
| Master Volume | -inf – +6 dB | 0 dB | Overall output level. |
| Dry/Wet | 0 – 100% | 100% | Blends the RVRSE output with the dry input (useful in FX chain mode). |

---

## DSP Architecture

### Offline Pipeline (off audio thread)

These operations run in a background thread whenever the sample loads, or Lush / Riser Length changes. They must never block the audio thread.

```
source_buffer[]
      ↓
  [ Reverb ]  ←  Lush knob
      ↓
lushed_buffer[]
      ↓
  [ Reverse ]  (in-place)
      ↓
reversed_buffer[]
      ↓
  [ Spectral Time-Stretch ]  ←  Riser Length + host BPM
      ↓
final_riser[]  →  ready for real-time playback
```

On any offline parameter change: rebuild from the appropriate stage onwards (e.g. changing Lush re-runs everything; changing only Riser Length skips the reverb stage and stretches the existing `reversed_buffer[]`).

### Real-Time Layer (audio thread, per-sample)

```cpp
// Pseudocode — per sample in processBlock()

float riser_sample = final_riser[playhead++];

// Stutter gate (real-time, MIDI CC responsive)
float gate = stutter.process(riser_sample, stutterRate, stutterDepth);

// Fade envelope
float faded = gate * fadeEnvelope.next();

// Hit fires at the calculated offset
float hit_sample = (playhead >= hitOffset) ? hit_buffer[hitPlayhead++] * hitVolume : 0.f;

output = (faded + hit_sample) * masterVolume;
```

### Reverb (MVP)

Use a **Schroeder/Moorer-style algorithmic reverb** — a network of parallel comb filters feeding into series allpass filters. No convolution, no external libraries. The `Lush` knob maps linearly to both room size (comb filter delay times) and wet gain.

Implement as a stateless function:
```cpp
void applyReverb(const float* in, float* out, size_t numSamples, float lushAmount);
```

### Time-Stretching

Uses **signalsmith-stretch** (MIT, spectral, polyphonic-aware) for high-quality stretching.
Originally OLA for MVP; upgraded to spectral for production quality. Stretch factor:

```
stretchFactor = (riserLengthBeats × samplesPerBeat) / reversed_buffer.size()
```

Recalculate `final_riser[]` whenever Riser Length changes or the host BPM changes.

### Stutter (Real-Time)

Stutter is a **rhythmic gate** computed entirely in the audio thread — never baked into the pre-processed buffer. This is what makes it MIDI CC-controllable with zero latency.

```
stutterPeriod  = samplesPerBeat / rateSubdivision   (e.g. 1/16 note)
stutterPhase   += 1 per sample
gate_signal     = (stutterPhase % stutterPeriod) < (stutterPeriod * 0.5) ? 1.0 : 0.0
output          = lerp(input, input * gate_signal, stutterDepth)
```

MIDI CC changes to Rate and Depth take effect on the very next sample — no buffer invalidation, no rebuilds.

### MIDI CC Mapping

MIDI CC should be user-assignable in a future version. For MVP, use sensible hardcoded defaults with clear constants in the source:

```cpp
constexpr int CC_STUTTER_RATE  = 1;   // Mod wheel
constexpr int CC_STUTTER_DEPTH = 11;  // Expression pedal
constexpr int CC_RISER_TUNE    = 2;   // Breath controller
```

---

## Tech Stack

| Concern | Choice | Notes |
|---|---|---|
| Language | C++17 | Standard for audio plugins |
| Framework | iPlug2 (latest, via iPlug2OOS) | Fully open source, no licensing restrictions |
| Plugin formats | VST3, AU, CLAP | VST3 SDK is now MIT licensed — no conflicts |
| Build system | CMake via iPlug2OOS template | Recommended starting point for 2025/26 |
| GUI | IGraphics (iPlug2 native) | Vector graphics, no extra dependencies |
| Audio file I/O | dr_libs (header-only) | WAV + AIFF, single header, no build complexity |
| Reverb DSP | Custom Schroeder (self-written) | No external libs needed |
| Time-stretch | signalsmith-stretch (MIT, header-only) | Spectral, polyphonic-aware, transient-preserving |
| CI/CD | GitHub Actions | Auto-build on Windows + macOS |
| License | MIT | Permissive; warmup project should be maximally forkable |

---

## GUI Layout

Dark-themed, minimal, performance-focused. Three clear zones:

```
┌──────────────────────────────────────────────────────┐
│  RVRSE          [LOAD SAMPLE]    BPM: 120            │
│  ────────────────────────────────────────────────    │
│  [ waveform: reversed riser ~~~~ | hit transient ]   │
├─────────────────────────┬────────────────────────────┤
│  RISER                  │  HIT                       │
│                         │                            │
│  Lush        [knob]     │  Volume      [knob]        │
│  Length      [knob]     │  Tune        [knob]        │
│  Fade In     [knob]     │                            │
│  Riser Tune  [knob]     │  [ hit waveform preview ]  │
│                         │                            │
│  Stutter Rate [knob] ◉  │                            │
│  Stutter Depth[knob] ◉  │                            │
│  (◉ = MIDI CC active)   │                            │
├─────────────────────────┴────────────────────────────┤
│  Master Vol [knob]   Dry/Wet [knob]   RVRSE v0.1.0   │
└──────────────────────────────────────────────────────┘
```

The **waveform display** shows the complete sequence as one continuous view: the reversed/washed riser on the left transitioning into the sharp transient of the hit on the right. A playhead scrubs through it during playback for live visual feedback.

MIDI CC-controlled knobs should display a small indicator (◉) when actively receiving CC input.

---

## Repository Structure

```
rvrse/
├── CMakeLists.txt
├── README.md
├── BRIEF.md                         ← this document
├── LICENSE  (MIT)
├── CHANGELOG.md
├── .github/
│   └── workflows/
│       ├── build-windows.yml
│       └── build-macos.yml
├── src/
│   ├── RVRSE.h / .cpp               ← iPlug2 plugin class (IPlug + IGraphics)
│   ├── RvrseProcessor.h / .cpp      ← offline pipeline orchestrator
│   ├── RvrseVoice.h / .cpp          ← real-time playback voice (riser + hit)
│   ├── Reverb.h / .cpp              ← Schroeder reverb, stateless
│   ├── Stretcher.h / .cpp           ← spectral time-stretcher (signalsmith-stretch)
│   ├── Stutter.h / .cpp             ← real-time stutter gate (audio thread only)
│   └── WaveformView.h / .cpp        ← IControl subclass for waveform display
└── resources/
    └── (fonts, UI assets)
```

> **Agent note:** `Stutter.h` must never be called from the offline pipeline. It is exclusively a real-time processor. `RvrseProcessor.h` must never be called from the audio thread.

---

## Out of Scope for MVP

- Multiple simultaneous voices / polyphony
- Preset save / load system
- User-assignable MIDI CC (hardcoded defaults for MVP)
- Convolution reverb
- High-quality pitch shifting (linear resampling is fine)
- Linux support
- Standalone app mode
- Sample looping

---

## How RVRSE Feeds Into OpenSampler

Every component built here has a direct equivalent in the main sampler project:

| RVRSE Component | OpenSampler Equivalent |
|---|---|
| `RvrseVoice` | `SamplerVoice` — the per-note playback voice |
| iPlug2 MIDI handling + CC | Same patterns, extended to full note ranges |
| `WaveformView` IControl | Waveform display in the sampler UI |
| GitHub Actions CI | Identical pipeline, different project |
| dr_libs sample loading | Same library, same patterns |
| IParam parameter system | Same approach, more parameters |
| Offline/real-time thread split | Core discipline carried into all future DSP work |

Completing RVRSE means OpenSampler starts with proven, working infrastructure rather than blank files.

---

## Identity

- **Plugin name:** RVRSE
- **Tagline:** Load a hit. Get a riser. Free, forever.
- **License:** MIT
- **Suggested repo:** `github.com/[yourname]/rvrse`

---

*— End of Brief —*
