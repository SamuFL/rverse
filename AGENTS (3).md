# AGENTS.md — RVRSE

This file contains mandatory instructions for any AI coding agent working on this repository.
Read it fully before touching any code.

---

## 1. Project Overview

RVRSE is a free, open-source audio plugin (VST3 / AU / CLAP) built with **iPlug2 and C++17**.
It generates a reverse-reverb riser automatically from any loaded hit sample and fires the hit
at a tempo-synced beat boundary. Full spec: [`BRIEF.md`](./BRIEF.md).

This is a **warmup project** for the larger OpenSampler initiative. Correctness and clean
architecture matter more than speed of delivery.

---

## 2. Task Tracking — Beads (`bd`)

This project uses **[Beads](https://github.com/steveyegge/beads)** (`bd`) for issue tracking.
Beads is a git-backed, agent-optimised issue tracker. All planning lives there, not in markdown
files or freeform TODO comments.

### Setup (once per machine)

```bash
# Install the bd CLI globally
curl -fsSL https://raw.githubusercontent.com/steveyegge/beads/main/scripts/install.sh | bash

# For VS Code + Copilot: install the MCP server
uv tool install beads-mcp
# Then add to .vscode/mcp.json:
# { "servers": { "beads": { "command": "beads-mcp" } } }

# Install git hooks in the repo (auto-syncs issues on commit/pull)
bd hooks install
```

### Mandatory Beads Workflow

Follow this pattern for every working session, without exception:

```
START OF SESSION
  bd prime              # Load context — read this output carefully
  bd ready --json       # Find unblocked tasks to work on

DURING WORK (for each task)
  bd update <id> --claim              # Claim the task before starting
  bd create "Sub-task" --type task    # Create child tasks as needed
  bd dep add <child-id> <parent-id>   # Link dependencies explicitly

END OF SESSION  ("land the plane")
  bd close <id> --reason "..." --json # Close completed tasks
  bd sync                             # Export + commit the issue database
  git pull --rebase
  git push                            # MANDATORY — do not stop before this
  git status                          # Must read "up to date with origin/main"
```

> **CRITICAL:** A session is NOT complete until `git push` succeeds and
> `git status` confirms you are up to date. Never say "ready to push" and stop.
> Push. Every. Time.

### Key Commands Reference

| Command | Purpose |
|---|---|
| `bd prime` | Load full workflow context — run at session start |
| `bd ready` | List tasks with no open blockers — your work queue |
| `bd create "Title" -t task -p 1` | Create a task (priority 0=highest, 4=lowest) |
| `bd update <id> --claim` | Atomically claim a task (sets you as assignee + in_progress) |
| `bd dep add <child> <parent>` | Mark that child is blocked by parent |
| `bd dep tree <id>` | Show full dependency tree for an issue |
| `bd show <id> --json` | View full task details |
| `bd close <id> --reason "Done"` | Mark a task complete |
| `bd sync` | Sync JSONL and commit |
| `bd stats` | Overall project progress |

### Commit Message Convention

Always include the Beads issue ID at the end of commit messages:

```
git commit -m "Add Schroeder reverb implementation (bd-a1b2)"
git commit -m "Wire stutter gate to MIDI CC (bd-c3d4)"
```

This lets `bd doctor` detect orphaned issues (committed but not closed).

---

## 3. Architecture Rules — Non-Negotiable

The codebase is split into two strictly separated layers. Violating this causes audio glitches
and subtle real-time bugs that are painful to debug.

### Offline Layer (`RvrseProcessor`)
Runs on a **background thread**. Never called from the audio thread.

- Sample loading
- Reverb application (`Reverb.h`)
- Buffer reversal
- Time-stretching (`Stretcher.h`)
- Writes the result into `final_riser[]`

### Real-Time Layer (`RvrseVoice`)
Runs in `ProcessBlock()` on the **audio thread**. Must be lock-free and allocation-free.

- Reads from `final_riser[]` (pre-computed, read-only during playback)
- Stutter gate (`Stutter.h`) — computed per-sample
- Fade envelope
- Hit playback at the calculated offset
- Responds to MIDI CC instantly

> **`Stutter.h` is real-time only.** It must never be called from the offline pipeline.
> **`RvrseProcessor.h` is offline only.** It must never be called from the audio thread.
> Use a lock-free flag or `juce::AbstractFifo`-style handoff to transfer the finished
> `final_riser[]` buffer from offline to real-time safely.

---

## 4. Code Standards

- **Language:** C++17. No newer features unless iPlug2 requires them.
- **No allocations on the audio thread.** Allocate buffers in the offline layer only.
- **No exceptions on the audio thread.** Use error codes or flags.
- **No raw owning pointers.** Use `std::unique_ptr` / `std::vector` / `std::array`.
- **Keep DSP functions stateless where possible.** `Reverb`, `Stretcher`, `Stutter` should
  be callable as pure functions with explicit state passed in. This makes them unit-testable.
- **Const-correctness.** Any function that doesn't mutate state must be `const`.
- **No magic numbers.** All MIDI CC defaults, buffer sizes, and timing constants live in
  a single `Constants.h` file.

---

## 5. Documentation Rules

Keeping documentation current is **not optional**. It is part of completing any task.

### What must stay up to date

| File | Update when... |
|---|---|
| `README.md` | Any user-facing behaviour changes, new build steps, new dependencies |
| `BRIEF.md` | Architecture decisions change or new constraints are discovered |
| `CHANGELOG.md` | Any commit that changes user-facing behaviour (add an entry under `[Unreleased]`) |
| `Constants.h` comments | Any constant value or MIDI CC mapping changes |
| Inline `///` doc comments | Any public function signature changes |

### CHANGELOG format

Follow [Keep a Changelog](https://keepachangelog.com/) conventions:

```markdown
## [Unreleased]
### Added
- Stutter gate with real-time MIDI CC control (bd-xxxx)
### Changed
- Lush knob now controls room size and wet gain together
### Fixed
- Riser buffer not regenerating on BPM change
```

> If you change behaviour and don't update `CHANGELOG.md`, the task is not done.

---

## 6. Build & Test

```bash
# Configure (first time)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --config Debug

# The plugin outputs appear in:
# build/RVRSE_artefacts/Debug/VST3/
# build/RVRSE_artefacts/Debug/AU/       (macOS only)
# build/RVRSE_artefacts/Debug/CLAP/
```

There is currently no automated test suite (warmup project scope). Manually validate in a
DAW after any change to the DSP pipeline. Recommended: REAPER (fast plugin reload).

---

## 7. Initial Work Plan

The tasks below represent the full MVP build sequence. They should be created in Beads at the
start of the first session using `bd create`. Dependencies are listed so the agent can wire
them with `bd dep add`.

**Bootstrap first (no dependencies):**

| Priority | Title | Type | Notes |
|---|---|---|---|
| P0 | Set up iPlug2OOS repo and CMake build | task | Use iPlug2OOS template. Verify empty plugin builds on Windows + macOS. |
| P0 | Set up GitHub Actions CI (Windows + macOS) | task | Build artefacts on every push. |

**Core DSP (depends on build working):**

| Priority | Title | Type | Notes |
|---|---|---|---|
| P0 | Implement sample loading with dr_libs | task | WAV + AIFF. Store as float32 stereo buffer. |
| P0 | Implement Schroeder algorithmic reverb | task | Stateless function. Lush param = room size + wet gain. |
| P0 | Implement buffer reversal | task | In-place reverse of the lushed buffer. |
| P0 | Implement OLA time-stretcher | task | Stateless. Stretch factor from Riser Length + host BPM. |
| P1 | Implement offline pipeline orchestrator (RvrseProcessor) | task | Chains: load → reverb → reverse → stretch. Runs off audio thread. Invalidates and rebuilds on param change. |
| P0 | Implement real-time stutter gate (Stutter.h) | task | Per-sample gate. Rate + Depth as live params. MIDI CC responsive. Audio thread only. |
| P0 | Implement RvrseVoice (real-time playback) | task | Reads final_riser[]. Fires hit at calculated offset. Applies stutter + fade envelope. |
| P1 | Wire offline ↔ real-time buffer handoff | task | Lock-free transfer of final_riser[] from RvrseProcessor to RvrseVoice. |

**MIDI + Parameters (depends on voice working):**

| Priority | Title | Type | Notes |
|---|---|---|---|
| P0 | Implement MIDI note-on trigger | task | Starts riser + schedules hit offset from Riser Length + host BPM. |
| P1 | Implement MIDI CC for Stutter Rate + Depth | task | Hardcoded defaults: CC1 = Rate, CC11 = Depth. |
| P2 | Implement Riser Tune + Hit Tune (pitch shift) | task | Linear resampling for MVP. |

**GUI (depends on parameters working):**

| Priority | Title | Type | Notes |
|---|---|---|---|
| P1 | Build basic IGraphics layout (dark theme) | task | Three zones: riser panel, hit panel, bottom bar. |
| P1 | Implement knobs for all parameters | task | Use IPlug2 IKnobControl. Labels. MIDI CC indicator (◉) for stutter knobs. |
| P2 | Implement WaveformView control | task | Shows riser → hit as one continuous waveform. Playhead scrubs during playback. |

**Polish + Release (depends on all above):**

| Priority | Title | Type | Notes |
|---|---|---|---|
| P1 | Write README.md | task | Installation, build instructions, plugin description, screenshots. |
| P2 | Validate on Windows (Reaper) | task | Manual QA: load sample, trigger, tweak CC, verify timing. |
| P2 | Validate on macOS (Reaper + Logic) | task | Same as above plus AU format. |
| P2 | Tag v0.1.0 and publish GitHub release | task | Attach built artefacts from CI. |

### How to create these in Beads

At the start of your first session, run:

```bash
bd prime
# Then create the bootstrap tasks first:
bd create "Set up iPlug2OOS repo and CMake build" -t task -p 0
bd create "Set up GitHub Actions CI" -t task -p 0
# ... continue for all tasks above
# Then wire dependencies:
bd dep add <sample-loading-id> <cmake-id>
# etc.
```

Or ask Copilot in VS Code chat: *"Create all the RVRSE initial tasks in Beads as described
in AGENTS.md, wire their dependencies, and show me the ready queue."*

---

## 8. What "Done" Means for Any Task

A task is **done** when all of the following are true:

1. The feature works correctly and has been manually tested in a DAW
2. Relevant documentation has been updated (`README`, `CHANGELOG`, inline docs)
3. The code compiles cleanly on both Windows and macOS (no warnings treated as errors)
4. The Beads issue is closed with a meaningful reason
5. Changes are committed with the issue ID in the commit message
6. `git push` has succeeded

---

*This file is part of the RVRSE project. Keep it accurate as the project evolves.*
