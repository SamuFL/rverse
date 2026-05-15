# RVRSE v1.1.0 — Scope & Roadmap

> **Status:** Locked
> **Last updated:** 2026-05-04
> **Owner:** Samuel Ferraz-Leite (@SamuFL)

---

## 1. Source material

This plan consolidates:

- `RVRSE-Feedback.md` — community feedback (5 bugs, 15 FRs, 8 remarks, 5 questions)
- The published v1.1.0 roadmap blog post (public commitments)
- The archived Beads tracker snapshot for historical context only

Anything not listed here is **out of scope** for v1.1.0 and lives in the v1.2.0 backlog or further out.

---

## 2. Must Ship

| #   | Commitment                                        | Source FRs/Bugs                                |
| --- | ------------------------------------------------- | ---------------------------------------------- |
| C-1 | Properly codesigned macOS installer               | BUG-001, BUG-003                               |
| C-2 | Clear error handling for compressed file formats  | BUG-004                                        |
| C-3 | Drag & drop sample loading                        | FR-010                                         |
| C-4 | Sample trimming (auto-remove dead air front/back) | FR-002                                         |
| C-5 | Crossfade control (reverse → dry transition)      | FR-003                                         |
| C-6 | Play / Export from the UI                         | FR-011, FR-012                                 |
| C-7 | Improved reverb engine                            | (technical debt — **optional, see Section 5**) |

**Linux is explicitly out of scope**. Community PRs welcome via MIT licence.

---

## 3. v1.1.0 scope (locked)

### 3.1 In scope — Tier 1

#### Installer & distribution

- **C-1a:** Apple Developer ID code-signing for macOS `.vst3`, `.component`, `.app`
- **C-1b:** Notarization + stapling
- **C-1c:** Proper `.pkg` installer (replaces zip + xattr instructions)
- **C-1d:** Updated install docs reflecting signed installer (deletes the xattr troubleshooting section)

#### Bug fixes

- **C-2:** Detect compressed/unsupported file formats, show clear error: "RVRSE supports uncompressed WAV and AIFF only" (BUG-004)
- **B-1:** Fix empty resource/examples folder on install (BUG-005)
- **B-2:** Investigate Studio One / Fender Studio Pro detection failure (BUG-002) — needs reproduction first; may resolve as a side-effect of signing
- **B-3:** Documentation pass for stutter CC mappings (QST-001 / FR-005) — quick win, no code change

#### Features

- **C-3:** Drag & drop sample loading (`yyahav` PR `#TBD` — already implemented, in review)
- **C-4:** Sample trimming — remove silence at front and back, manually "cut into" the sample
- **C-5:** Crossfade control between reversed tail and dry hit
- **C-6a:** UI play button (preview without MIDI)
- **C-6b:** Render/export to file from the UI (drag-out can come in v1.2)
- **C-7:** Reverb engine replacement — see Section 5 (Risk & Spike)

### 3.2 In scope — Tier 2: Quality and infrastructure

These don't ship as user-facing features but are non-negotiable for a maintainable v1.1.0:

- **Q-3:** Catch2 test framework + tests/ directory
- **Q-4:** Headless integration tests — at least one golden-master smoke test
- **Q-6:** Migrate active project management from Beads to GitHub Issues + Projects + Milestones — completed via issue #26

### 3.3 Out of scope — explicitly deferred to v1.2.0+

| ID     | Item                             | Reason for deferral                                               |
| ------ | -------------------------------- | ----------------------------------------------------------------- |
| FR-001 | Dry sample on separate MIDI note | Single request; nice-to-have; design decision needed              |
| FR-004 | Odd-meter tempo subdivisions     | Single request; small but not committed                           |
| FR-005 | User-assignable MIDI CCs         | Quality-of-life; documentation fix lands first (B-3)              |
| FR-006 | Linux build                      | Explicitly excluded in roadmap post; community PRs only           |
| FR-007 | External/sidechain reverb        | Architectural change; conflicts with C-7 (own reverb improvement) |
| FR-013 | Sample region selection          | Overlaps with C-4 trimming; revisit in v1.2 once trim ships       |
| FR-014 | Fractional LFO rates             | Single request                                                    |
| FR-015 | ARA support                      | Major integration;                                                |

### 3.4 Out of scope — permanently rejected

These are not coming back. Document the rejection so users can be linked to the decision rather than re-litigating it.

| ID     | Item            | Rejection reason                                                                                                                                                                               |
| ------ | --------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| FR-008 | AAX / Pro Tools | Avid SDK + iLok wrapping is too much overhead for a free plugin maintained by one person. Pro Tools users are welcome to suggest the project to a contributor willing to maintain an AAX fork. |
| FR-009 | iPadOS / AUv3   | Touch UI rework + App Store distribution + ongoing certificate maintenance is a separate project, not a port. Not happening.                                                                   |

### 3.5 Out of scope — explicitly punted

| ID                     | Item                                         |
| ---------------------- | -------------------------------------------- |
| Open Sampler work      | Separate products, separate roadmaps         |
| "Logic EQ for Windows" | Idea-phase appendix item, not a roadmap item |

---

## 4. Milestone structure

Three internal phases inside the `v1.1.0` GitHub milestone, tracked via Projects v2 status field. Target: ship v1.1.0 within 2 weeks (by 2026-05-18).

### Phase 0 — "Foundations" (Days 1–2)

Goal: get the project management surface in order before any feature work starts. Without this, every Copilot CLI session is operating against drifting context.

- **Q-6:** Migrate to GitHub Issues + Projects + Milestones (first thing — blocking)
- Convert this scope document into the actual issues, milestone, project board
- C-1 (Apple Developer Programme signup, code-signing setup, notarization pipeline)
- B-3 (stutter CC documentation pass — quick win, no code)

**Exit criteria:** GitHub project board populated, Beads archived, Apple Developer ID active, signing pipeline working in CI.

### Phase 1 — "User-facing features" (Days 3–9)

Goal: ship the headline features.

- C-3 (drag & drop — merge `yyahav` PR after UAT)
- C-4 (sample trimming)
- C-5 (crossfade)
- C-6a (UI play button)
- C-6b (export/render via system save dialog — confirmed in scope)
- C-2 (compressed file format error handling — small, lands here)
- B-1 (empty resource folder fix)
- B-2 (Studio One investigation — likely resolves with signing)
- Q-3, Q-4 (test framework + at least one integration test) — incremental, not blocking

**Optional spike (Days 3–7 if pursued):** reverb engine evaluation per Section 5. Hard cutoff: if no clear win by end of Day 7, drop C-7 from v1.1.0 entirely.

**Exit criteria:** All headline features merged to `develop`, signed installer producing notarized artefacts.

### Phase 2 — "UAT & release" (Days 10–14)

Goal: validate, polish, ship.

- UAT pass per playbook (Section 4.1)
- Reverb decision: ship if spike succeeded, otherwise defer cleanly
- Documentation review (README, install docs, in-plugin help if any)
- CHANGELOG.md
- Release notes draft
- Tag `v1.1.0`, GitHub release with all artefacts
- Blog post + newsletter announcement

**Exit criteria:** v1.1.0 published, announcement out, the archived Beads tracker clearly marked as historical in README.

### 4.1 UAT playbook (replaces Q-5 cross-DAW validation)

**Mac (primary platform — full coverage):**

- Logic Pro (if available)
- Ableton Live Lite
- Cubase
- FL Studio (trial)
- Studio One Pro 7
- Standalone

**Windows (smoke test only):**

- Cubase
- Studio One Pro 7

**Per-DAW checklist (kept in `docs/uat-checklist.md`):**

1. Plugin scans and loads cleanly
2. Sample loads via file dialog
3. Sample loads via drag & drop (new in v1.1)
4. MIDI note triggers riser + hit
5. Stutter CC1 / CC11 respond per documentation
6. UI play button works (new in v1.1)
7. Export/render produces valid WAV (new in v1.1)
8. Trim controls work (new in v1.1)
9. Crossfade works (new in v1.1)
10. Plugin state saves/loads with the project
11. No crashes on close/reload

---

## 5. Optional: the reverb engine spike

**Status: optional in v1.1.0.** The decision is to keep what already works (Schroeder/Moorer in `Reverb.h`) and treat any improvement as a bonus. The blog post said "improved reverb engine" — that can mean tuned defaults and expanded ranges, not necessarily a new algorithm.

### 5.1 Decision rule

Run the spike only if Days 1–2 finish on schedule. If pursued, hard cutoff is **end of Day 7**. If no candidate clearly beats the current engine by then, drop C-7 from v1.1.0 entirely and ship the existing reverb with retuned defaults.

No "let me give it one more day" — this is the rule that protects the 2-week timeline.

### 5.2 The spike (if pursued)

1. Create `spike/reverb-eval` branch
2. A/B test current `Reverb.h` vs **Airwindows** (MIT) vs **Freeverb3** (LGPL — note licensing implications even though RVRSE is MIT)
3. Three reference samples: drum hit, vocal "ah", synth pad
4. Decision committed to `docs/decisions/0001-reverb-engine.md`

### 5.3 Honest framing for users

If the spike doesn't ship: the v1.1.0 release notes should say "reverb tuning improvements; full engine evaluation continues into v1.2.0.".

---

## 7. Migration status: Beads → GitHub

The migration is complete:

1. GitHub Labels, Milestones, Issue templates, and the Projects v2 board are the active planning surface
2. The Beads database is archived under `.archive/beads-historical/`
3. README and contributor workflow docs point at GitHub as the canonical source of truth
4. New work is tracked only in GitHub; the archived Beads data remains historical reference

---

## 8. What "done" looks like for v1.1.0

A v1.1.0 release is done when:

1. All Tier 1 items shipped (C-7 optional per Section 5)
2. CI green on both platforms with Pluginval passing (already in place)
3. UAT pass complete per the playbook (Section 4.1)
4. Signed `.pkg` installer verified on a clean macOS install
5. README, install docs, and in-plugin help reviewed and updated
6. Windows-as-smoke-tested-only documented in README
7. CHANGELOG.md updated
8. GitHub release created with VST3, AU, CLAP, and `.pkg` artefacts attached
9. Blog post + newsletter announcing the release
10. The archived Beads tracker is clearly documented in README; all v1.1.0 work is tracked in GitHub Issues against the milestone
11. `yyahav` credited in release notes

---
