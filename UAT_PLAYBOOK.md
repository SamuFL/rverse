# RVRSE — UAT Test Playbook

This document describes the **User Acceptance Tests** for the RVRSE audio plugin.
Run these tests manually after every change to the DSP pipeline or plugin behaviour.

---

## General Test Setup

| # | Host             | Format | Notes                            |
|---|------------------|--------|----------------------------------|
| 1 | **Cubase**       | VST3   | Primary DAW for testing          |
| 2 | **Studio One**   | VST3   | Secondary DAW for cross-compat   |
| 3 | **Standalone**   | APP    | Quick smoke-test, no DAW needed  |

### Prerequisites

- Build the plugin: `cmake --build build --config Debug`
- Plugin auto-deploys to `~/Library/Audio/Plug-Ins/` (macOS)
- Prepare a set of **test samples** (short percussive hits, long tails, tonal, noise).
  Store them in a stable location so paths don't change between sessions.
- Cubase project: *(TODO — create a dedicated Cubase test project)*
- Studio One project: *(TODO — create a dedicated Studio One test project)*

### MIDI CC Reference

| CC  | Parameter       | Range          |
|-----|-----------------|----------------|
| CC1 | Stutter Rate    | 0–30 Hz        |
| CC11| Stutter Depth   | 0–100%         |

### Plugin Parameters (DAW Generic Editor)

| Parameter       | Range   | Default | Unit |
|-----------------|---------|---------|------|
| Master Volume   | 0–100   | 100     | %    |
| Stutter Rate    | 0–30    | 0 (off) | Hz   |
| Stutter Depth   | 0–100   | 50      | %    |

> **Note:** Riser length is currently hardcoded at 4 beats. Tempo-sync options
> are not yet exposed in the UI — they will be tested once the GUI is available.

---

## Test Scenario 1 — Sample Loading & Riser Generation

**Goal:** Verify that RVRSE correctly loads various sample types and generates an
audible, artefact-free reverse-reverb riser for each.

### Steps

1. Open RVRSE in each test host (Cubase VST3, Studio One VST3, Standalone).
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

## Test Scenario 2 — Continuous Stutter Modulation

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

## Test Scenario 3 — Discontinuous Stutter Automation (Click-Free)

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

## Test Scenario 4 — *(Reserved: Sample Persistence)*

> **Not yet implemented.** See beads issue for sample persistence feature.
> Once implemented, test that:
> - Closing and reopening a project restores the previously loaded sample.
> - If the sample file was moved/deleted, a user-visible warning is displayed.
> - The plugin degrades gracefully (no crash) when the file is missing.

---

## Test Scenario 5 — *(Reserved: GUI Interaction)*

> **Not yet implemented.** Will be added when the IGraphics GUI is built.

---

## Test Scenario 6 — *(Reserved: Tempo-Synced Stutter)*

> **Not yet implemented.** See beads issue rverse-0gq.

---

## Revision History

| Date       | Change                                             |
|------------|----------------------------------------------------|
| 2026-04-02 | Initial playbook: scenarios 1–3 (load, stutter, clicks) |
