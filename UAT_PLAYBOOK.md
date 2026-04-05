# RVRSE — UAT Test Playbook

This document describes the **User Acceptance Tests** for the RVRSE audio plugin.
Run these tests manually after every change to the DSP pipeline or plugin behaviour.

---

## General Test Setup

| # | Host             | Format | Notes                            |
|---|------------------|--------|----------------------------------|
| 1 | **Studio One**   | VST3   | Primary DAW for testing          |
| 2 | **Cubase**       | VST3   | Secondary DAW for cross-compat   |
| 3 | **Standalone**   | APP    | Quick smoke-test, no DAW needed  |

### Prerequisites

- Build the plugin: `cmake --build build --config Debug`
- Plugin auto-deploys to `~/Library/Audio/Plug-Ins/` (macOS)
- Prepare a **reference sample** (short percussive hit, ~100–500 ms, stereo, 44.1 or 48 kHz).
  Store it in a stable location so the path doesn't change between sessions.
- Studio One project: set up an instrument track with RVRSE loaded, reference sample pre-loaded.

### MIDI CC Reference

| CC  | Parameter       | Range          |
|-----|-----------------|----------------|
| CC1 | Stutter Rate    | 0–30 Hz        |
| CC11| Stutter Depth   | 0–100%         |

### Plugin Parameters (DAW Generic Editor)

| # | Parameter       | Range           | Default   | Unit  | Step  |
|---|-----------------|-----------------|-----------|-------|-------|
| 1 | Master Volume   | 0–100           | 100       | %     | 0.01  |
| 2 | Stutter Rate    | 0–30            | 0 (off)   | Hz    | 0.1   |
| 3 | Stutter Depth   | 0–1             | 0.5       |       | 0.01  |
| 4 | Lush            | 0–100           | 40        | %     | 0.1   |
| 5 | Riser Length     | 0.25–16         | 4         | beats | 0.25  |
| 6 | Fade In         | 0–100           | 60        | %     | 0.1   |
| 7 | Riser Volume    | -60 to +6       | 0         | dB    | 0.1   |
| 8 | Hit Volume      | -60 to +6       | 0         | dB    | 0.1   |
| 9 | Debug Stage     | Normal / Reverbed / Reversed / Riser Only | Normal | — | enum |

---

## Test Scenario 1 — Sample Loading & Riser Generation

**Goal:** Verify that RVRSE correctly loads various sample types and generates an
audible, artefact-free reverse-reverb riser for each.

### Steps

1. Open RVRSE in each test host (Studio One VST3, Cubase VST3, Standalone).
2. For each test sample:
   a. Load the sample via the file dialog.
   b. Trigger a MIDI note and listen to the riser → hit playback.
   c. Verify the riser duration matches the expected beat length (default: 4 beats).
   d. Verify no clicks, pops, or silence gaps at the riser → hit boundary.
   e. Verify the hit fires at the correct beat boundary (quantised to the grid).

### Test Samples

| # | Description                  | Expected Behaviour                           |
|---|------------------------------|----------------------------------------------|
| 1 | Short kick drum (< 100 ms)   | Riser builds from near-silence to impact     |
| 2 | Snare with long tail (> 1 s) | Lush reverb tail, smooth ramp                |
| 3 | Tonal hit (piano chord)      | Pitched content preserved, no warble          |
| 4 | Noise burst (white/pink)     | Even riser with no tonal artefacts           |
| 5 | Stereo sample                | Stereo image maintained throughout riser     |
| 6 | Mono sample                  | Plays correctly on both L+R channels         |
| 7 | Very long sample (> 10 s)    | Loads without crash; riser truncated cleanly |
| 8 | 44.1 kHz sample              | Correct playback at project sample rate      |
| 9 | 48 kHz sample                | Correct playback at project sample rate      |
| 10| 96 kHz sample                | Correct playback at project sample rate      |

### Pass Criteria

- [ ] All samples load without crash in all three hosts
- [ ] Riser audio is audible and builds to the hit
- [ ] No clicks or pops at any transition point
- [ ] Stereo samples play in stereo; mono samples play on both channels

---

## Test Scenario 2 — Parameter Tests (All 8 Parameters)

**Goal:** Verify that each DAW-automatable parameter produces the expected audible
effect. Use the reference sample and DAW generic editor for all tests.

**Setup:** Load the reference sample, set all parameters to defaults, trigger a
sustained MIDI note for each sub-test. Change **one parameter at a time** unless
otherwise stated.

### 2.1 Master Volume

| Test | Value | Expected Outcome |
|------|-------|------------------|
| 2.1a | 100% (default) | Full-volume playback of both riser and hit |
| 2.1b | 50% | Both riser and hit are noticeably quieter (−6 dB) |
| 2.1c | 0% | Complete silence — no audio output at all |
| 2.1d | Automate 0→100→0 | Smooth fade in and out, no clicks |

### 2.2 Lush (Reverb Amount)

| Test | Value | Expected Outcome |
|------|-------|------------------|
| 2.2a | 0% | Riser is very thin/dry — mostly the raw reversed sample with minimal reverb wash |
| 2.2b | 40% (default) | Moderate reverb wash, balanced between dry transients and wet tail |
| 2.2c | 100% | Dense, washy reverb — original transients nearly buried in reverb |
| 2.2d | Change 40→80 | Riser **rebuilds** after a short delay (offline processing) — next note-on plays the new riser |

> **Note:** Lush triggers an offline pipeline rebuild. The change is NOT instantaneous
> — the riser plays with the old setting until the rebuild completes. This is expected.

### 2.3 Riser Length

| Test | Value | BPM | Expected Riser Duration |
|------|-------|-----|------------------------|
| 2.3a | 4 beats (default) | 120 | ~2 seconds |
| 2.3b | 1 beat | 120 | ~0.5 seconds |
| 2.3c | 8 beats | 120 | ~4 seconds |
| 2.3d | 16 beats | 120 | ~8 seconds |
| 2.3e | 0.25 beats | 120 | ~0.125 seconds (very short burst before hit) |
| 2.3f | 4 beats | 60 | ~4 seconds (half the BPM = double the duration) |
| 2.3g | 4 beats | 180 | ~1.33 seconds (faster BPM = shorter duration) |

> **Note:** Like Lush, changing Riser Length triggers an offline rebuild. The hit should
> fire at the end of the riser regardless of length — verify the riser→hit timing is correct
> for each setting.

### 2.4 Fade In

| Test | Value | Expected Outcome |
|------|-------|------------------|
| 2.4a | 0% | No fade-in — riser starts at full amplitude immediately |
| 2.4b | 60% (default) | Riser gradually ramps up over the first 60% of its length, then plays at full volume |
| 2.4c | 100% | Riser ramps up over its entire length — barely reaches full volume before the hit |
| 2.4d | Compare 0% vs 100% | At 0%, the riser start is abrupt; at 100%, the start is gentle and progressive |

### 2.5 Riser Volume

| Test | Value | Expected Outcome |
|------|-------|------------------|
| 2.5a | 0 dB (default) | Riser plays at unity gain — balanced with the hit |
| 2.5b | +6 dB | Riser is noticeably louder than the hit |
| 2.5c | -12 dB | Riser is noticeably quieter than the hit |
| 2.5d | -60 dB | Riser is essentially inaudible — only the hit is heard |
| 2.5e | Compare +6 vs -60 | Clear difference: riser dominates at +6, disappears at -60 |

### 2.6 Hit Volume

| Test | Value | Expected Outcome |
|------|-------|------------------|
| 2.6a | 0 dB (default) | Hit plays at unity gain — balanced with the riser |
| 2.6b | +6 dB | Hit is noticeably louder than the riser |
| 2.6c | -12 dB | Hit is noticeably quieter than the riser |
| 2.6d | -60 dB | Hit is essentially inaudible — only the riser is heard |
| 2.6e | Compare +6 vs -60 | Clear difference: hit dominates at +6, disappears at -60 |

### 2.7 Riser Volume + Hit Volume Combined

| Test | Riser Vol | Hit Vol | Expected Outcome |
|------|-----------|---------|------------------|
| 2.7a | 0 dB | 0 dB | Both voices at equal level (default) |
| 2.7b | +6 dB | -60 dB | Only the riser is heard — "riser only" mode |
| 2.7c | -60 dB | +6 dB | Only the hit is heard — "hit only" mode |
| 2.7d | +6 dB | +6 dB | Both voices boosted — louder overall, check for clipping |
| 2.7e | -60 dB | -60 dB | Both inaudible — complete silence (apart from noise floor) |

### 2.8 Stutter Rate

| Test | Value | Depth | Expected Outcome |
|------|-------|-------|------------------|
| 2.8a | 0 Hz (default) | 0.5 | No stuttering — clean riser playback |
| 2.8b | 4 Hz | 0.5 | Slow tremolo-like pulsing on the riser (4 pulses/sec) |
| 2.8c | 15 Hz | 0.5 | Fast stuttering — "machine gun" effect on the riser |
| 2.8d | 30 Hz | 0.5 | Maximum stutter — very rapid gating, approaching ring-mod territory |
| 2.8e | Sweep 0→30 Hz | 0.5 | Rate increases continuously, no stepping or jumps |

### 2.9 Stutter Depth

| Test | Value | Rate | Expected Outcome |
|------|-------|------|------------------|
| 2.9a | 0 | 15 Hz | No audible stutter — depth of 0 means dry signal passes through |
| 2.9b | 0.5 (default) | 15 Hz | Moderate stuttering — signal dips but doesn't fully cut |
| 2.9c | 1.0 | 15 Hz | Full stutter — signal cuts completely during gate-off portions |
| 2.9d | Sweep 0→1 | 15 Hz | Stutter gradually goes from subtle to full cut |

### Pass Criteria (All Parameter Tests)

- [ ] Each parameter audibly changes the output as described
- [ ] Lush and Riser Length trigger pipeline rebuilds (visible as short delay before new riser)
- [ ] Fade In, Riser Volume, Hit Volume, Stutter Rate/Depth take effect instantly (next sample)
- [ ] No clicks, pops, or glitches when changing any parameter
- [ ] No CPU spikes or audio dropouts during parameter changes

---

## Test Scenario 3 — Continuous Stutter Modulation (MIDI CC)

**Goal:** Verify that the stutter gate responds smoothly to continuous MIDI CC changes
without audible clicks or artefacts.

### Steps

1. Load a test sample and trigger a sustained MIDI note (hold it throughout).
2. Using a MIDI controller or DAW automation lane:
   a. **Slowly sweep CC1** (Stutter Rate) from 0 → 127 while CC11 is at max (100%).
      - Listen for smooth transition from no-stutter into stuttering.
      - Listen for rate increasing continuously (no stepping or jumps).
   b. **Slowly sweep CC1** from 127 → 0.
      - Listen for smooth deceleration back to no-stutter.
   c. **Slowly sweep CC11** (Stutter Depth) from 0 → 127 while CC1 is at a moderate rate (~50%).
      - Listen for smooth wet/dry crossfade.
   d. **Slowly sweep CC11** from 127 → 0.
      - Listen for smooth return to dry signal.
   e. **Simultaneously modulate CC1 and CC11** with an LFO or complex automation curve.
      - Verify no clicks, glitches, or CPU spikes.

### Pass Criteria

- [ ] Stutter rate sweeps smoothly from off to maximum and back
- [ ] Stutter depth fades smoothly between dry and full stutter
- [ ] No audible clicks at any point during continuous modulation
- [ ] No CPU spikes or audio dropouts during rapid modulation

---

## Test Scenario 4 — Discontinuous Stutter Automation (Click-Free)

**Goal:** Verify that sudden, discrete jumps in stutter parameters do **not** produce
audible clicks, pops, or glitches.

### Steps

1. Load a test sample and trigger a sustained MIDI note.
2. Create **step automation** (discontinuous jumps) in the DAW:
   a. **CC1 jump: 0 → 64** (off → mid-rate). Listen for click at transition.
   b. **CC1 jump: 64 → 0** (mid-rate → off). Listen for click at transition.
   c. **CC1 jump: 0 → 127** (off → max-rate). Listen for click at transition.
   d. **CC11 jump: 0 → 127** (dry → full depth). Listen for click.
   e. **CC11 jump: 127 → 0** (full depth → dry). Listen for click.
   f. **Rapid alternation:** CC1 toggling 0 ↔ 127 every beat (stress test).
3. Repeat all tests using **DAW parameter automation** (generic editor knobs)
   instead of MIDI CC, to verify both paths.

### Pass Criteria

- [ ] No audible clicks on any discrete parameter jump
- [ ] One-pole smoother handles all transitions within ~2 ms
- [ ] Rapid toggling does not cause CPU spikes or audio corruption

---

## Test Scenario 5 — Debug Stage Diagnostic Playback

**Goal:** Verify that the Debug Stage parameter correctly exposes intermediate
pipeline buffers, allowing isolation of each processing stage for diagnostics.

> **Usage hint:** Use Debug Stage to identify which pipeline step introduces
> unwanted artefacts (pitch shift, phasing, etc.). Compare each stage's output
> against the original sample.

### Pre-condition

- Sample loaded and riser generated (Scenario 1 passing)
- All params at defaults except Debug Stage

### Tests

#### 5.1 — Normal mode (default)
- Set **Debug Stage = Normal** (default).
- Trigger note → riser plays, hit fires at beat boundary.
- Identical to behaviour without debug mode.

#### 5.2 — Reverbed
- Set **Debug Stage = Reverbed**.
- Trigger note → plays the **reverbed sample** (forward, not reversed, not stretched).
- **No hit fires** after the buffer ends.
- Duration should be original sample length + reverb tail (up to ~5 s extra).
- No fade-in, no stutter applied — raw reverb output.
- Useful for checking if the **Schroeder reverb introduces pitch colouring**.

#### 5.3 — Reversed
- Set **Debug Stage = Reversed**.
- Trigger note → plays the **reversed reverbed sample** (not stretched).
- **No hit fires**.
- Duration same as Reverbed (pre-stretch length).
- No fade-in, no stutter — raw reversed output.
- Useful for checking that **reversal is clean** (no clicks, no glitches).

#### 5.4 — Riser Only
- Set **Debug Stage = Riser Only**.
- Trigger note → plays the **final stretched riser** with all normal processing
  (fade-in, stutter, riser volume gain).
- **No hit fires** — riser plays in isolation.
- Duration matches Riser Length × tempo (same as Normal mode riser).
- Useful for hearing the **time-stretcher quality** without the hit masking artefacts.

#### 5.5 — A/B comparison workflow
1. Set Debug Stage to **Reverbed** → trigger note → listen for pitch artefacts.
2. Switch to **Reversed** → trigger → compare tonal quality with Reverbed.
3. Switch to **Riser Only** → trigger → listen for stretching artefacts.
4. Switch to **Normal** → trigger → verify full pipeline is unchanged.
5. At each step, the pipeline does NOT rebuild — switching is instant.

#### 5.6 — Lush interaction
- Set **Debug Stage = Reverbed**, Lush = 0% → trigger → should hear nearly dry sample.
- Set Lush = 100% → trigger → should hear heavy reverb (pipeline rebuilds).
- Confirm pitch colouring increases with higher Lush values.

### Pass Criteria

- [ ] Each debug stage plays the correct intermediate buffer
- [ ] No hit fires in Reverbed, Reversed, or Riser Only modes
- [ ] Switching debug stages does not trigger a pipeline rebuild
- [ ] Reverbed and Reversed modes play without fade-in or stutter processing
- [ ] Riser Only mode plays with full processing chain (fade-in, stutter, volume)
- [ ] No clicks or crashes when switching debug stages during playback

---

## Test Scenario 6 — *(Reserved: Sample Persistence)*

> **Not yet implemented.** See beads issue for sample persistence feature.
> Once implemented, test that:
> - Closing and reopening a project restores the previously loaded sample.
> - If the sample file was moved/deleted, a user-visible warning is displayed.
> - The plugin degrades gracefully (no crash) when the file is missing.

---

## Test Scenario 7 — *(Reserved: GUI Interaction)*

> **Not yet implemented.** Will be added when the IGraphics GUI is built.

---

## Test Scenario 8 — *(Reserved: Tempo-Synced Stutter)*

> **Not yet implemented.** See beads issue rverse-0gq.

---

## Revision History

| Date       | Change                                                         |
|------------|----------------------------------------------------------------|
| 2026-04-05 | Added Scenario 5: debug stage diagnostic playback (rverse-l9x) |
| 2026-04-05 | Added Scenario 2: full parameter tests for all 8 params (rverse-nqg) |
| 2026-04-02 | Initial playbook: scenarios 1–3 (load, stutter, clicks)       |
