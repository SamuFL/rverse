# AGENTS.md — RVRSE

This file contains mandatory instructions for any AI coding agent working on this repository.
Read it fully before touching any code.

---

## 1. Project Overview

RVRSE is a free, open-source audio plugin (VST3 / AU / CLAP) built with **iPlug2 and C++17**.
It generates a reverse-reverb riser automatically from any loaded hit sample and fires the hit
at a tempo-synced beat boundary. Full spec: [`RVRSE_BRIEF.md`](./RVRSE_BRIEF.md).

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

## 3. Library Documentation — Context7

This project uses the **Context7 MCP server** to provide agents with up-to-date library
documentation. Before implementing any call to an external library or framework, **check
Context7 first** to verify the correct API.

### When to use Context7

- **iPlug2 API calls** — `IPlug`, `IGraphics`, `IControl`, `IParam`, `IMidiQueue`, etc.
- **dr_libs** — `dr_wav`, `dr_flac` header-only audio codecs.
- **C++ standard library** — when unsure about C++17 behaviour or edge cases.
- **CMake** — build system functions, `FetchContent`, target properties.
- **Any third-party dependency** added in the future.

### Rule

> **Do not guess library APIs.** If you are unsure about a function signature, parameter
> order, return type, or behaviour, use Context7 to look it up. Incorrect API usage in
> audio code causes subtle bugs that are painful to diagnose.

### Setup

Context7 is configured in `.vscode/mcp.json` (git-ignored — contains API key).
See [context7.com](https://context7.com) to obtain an API key.

---

## 4. Architecture Rules — Non-Negotiable

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

## 5. Code Standards

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

## 6. Documentation Rules

Keeping documentation current is **not optional**. It is part of completing any task.

### What must stay up to date

| File | Update when... |
|---|---|
| `README.md` | Any user-facing behaviour changes, new build steps, new dependencies |
| `BRIEF.md` | Architecture decisions change or new constraints are discovered |
| `CHANGELOG.md` | Any commit that changes user-facing behaviour (add an entry under `[Unreleased]`) |
| `UAT_PLAYBOOK.md` | Any parameter added/changed/removed, any new test scenario needed |
| `Constants.h` comments | Any constant value or MIDI CC mapping changes |
| Inline `///` doc comments | Any public function signature changes |

### PR Documentation Checklist

Before opening or updating a pull request, **review every file in the table above** and
verify it reflects the changes in the PR. This is a blocking requirement — a PR with
stale documentation is not ready for review.

Specifically:
1. **Before opening a PR:** Re-read `README.md`, `CHANGELOG.md`, and `UAT_PLAYBOOK.md`.
   Confirm all new/changed behaviour is documented and all parameter tables are current.
2. **While working on a PR:** After each commit that changes user-facing behaviour,
   update `CHANGELOG.md` in the same commit (not as an afterthought).
3. **Before requesting review:** Do a final pass over all documentation files.
   Check that parameter names, ranges, defaults, and descriptions match the code.

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

## 7. Build & Test

```bash
# Configure (first time) — use SYMLINK on macOS with Make/Ninja generators
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DIPLUG_DEPLOY_METHOD=SYMLINK

# Build
cmake --build build --config Debug

# The plugin outputs appear in:
# build/out/RVRSE.vst3/
# build/out/RVRSE.component/   (macOS only)
# build/out/RVRSE.clap/
# build/out/RVRSE.app/

# Run unit tests
cmake --build build --target rvrse_tests && ctest --test-dir build
```

Manually validate in a DAW after any change to the DSP pipeline. Recommended DAWs:
**Cubase**, **Studio One**, **Logic** (macOS only).

---

## 7b. Version Management

The **single source of truth** for the plugin version is `RVRSE/config.h`:

```c
#define PLUG_VERSION_HEX 0x00000100
#define PLUG_VERSION_STR "0.1.0"
```

A sync script propagates this version to all satellite files (plists, installer, CMakeLists).
CI enforces consistency — PRs with version drift will fail the `version-check` job.

### Bumping the version

```bash
# 1. Edit config.h — update both PLUG_VERSION_STR and PLUG_VERSION_HEX
# 2. Run the sync script
python3 scripts/sync-version.py

# 3. Verify (optional — CI also runs this)
python3 scripts/sync-version.py --check

# 4. Commit all changed files together
```

### What the script updates

| File(s) | Fields |
|---|---|
| `RVRSE/resources/*.plist` (11 files) | `CFBundleShortVersionString`, `CFBundleVersion` |
| `RVRSE/installer/RVRSE.iss` | `AppVersion`, `VersionInfoVersion` |
| `RVRSE/CMakeLists.txt` | `project(RVRSE VERSION ...)` |

> **Never manually edit version strings in satellite files.** Always change `config.h`
> and run the sync script. The CI `version-check` job will block any PR where files
> are out of sync.

---

## 8. Git-Flow Branching Strategy

This project uses a standard **git-flow** branching model. Follow it without exception.

| Branch | Purpose | Merges into |
|---|---|---|
| `main` | Production-ready releases only. Tagged with version numbers. | — |
| `develop` | Integration branch. All feature work merges here first. | `main` (via release branch) |
| `feature/<name>` | One branch per task/feature. Short-lived. | `develop` |
| `release/<version>` | Release candidate. Only bug fixes, no new features. | `main` + `develop` |
| `hotfix/<name>` | Emergency fixes against `main`. | `main` + `develop` |

### Workflow Rules

1. **Never commit directly to `main` or `develop`.** Always use a feature branch.
2. **Feature branches** are created from `develop` and merged back via PR or fast-forward:
   ```bash
   git checkout develop
   git pull
   git checkout -b feature/<beads-id>-short-description
   # ... do work, commit with beads ID in message ...
   git checkout develop
   git merge feature/<beads-id>-short-description
   git branch -d feature/<beads-id>-short-description
   git push
   ```
3. **Release branches** are created from `develop` when all planned features are merged:
   ```bash
   git checkout develop
   git checkout -b release/0.1.0
   # ... UAT, bug fixes only ...
   git checkout main
   git merge release/0.1.0
   git tag -a v0.1.0 -m "Release v0.1.0"
   git checkout develop
   git merge release/0.1.0
   git branch -d release/0.1.0
   git push --all && git push --tags
   ```
4. **Name feature branches** using the Beads ID: `feature/rverse-uj4-cpp-environment`.

---

## 9. Git Hooks

This project uses **versioned git hooks** in the `hooks/` directory, activated via
`git config core.hooksPath hooks`. They are enforced automatically — no manual setup needed
after cloning (the config is in `.git/config`).

### Setup (once per clone)

```bash
git config core.hooksPath hooks
```

### Active Hooks

| Hook | Behaviour | Bypass |
|---|---|---|
| **commit-msg** | Requires a Beads issue ID (`bd-xxxx` or `rverse-xxxx`) in every commit message. Exempts merge commits, beads sync, reverts, and version tags. | `--no-verify` |
| **pre-commit** | Blocks commits containing merge conflict markers, files >5 MB, or possible secrets/API keys. Warns (non-blocking) on `std::cout`/`printf` debug statements in C++ files. | `--no-verify` |
| **pre-push** | **Blocks** direct pushes to `main` (must use release/ or hotfix/ branches). **Warns** on direct pushes to `develop` (prefer feature branches). Runs `bd doctor` as a non-blocking health check. | `--no-verify` |

### Adding New Hooks

1. Create the hook script in `hooks/` (must be executable: `chmod +x hooks/<name>`)
2. Follow the naming convention from `githooks(5)`: `pre-commit`, `commit-msg`, `pre-push`, etc.
3. Document the hook in this table
4. Commit the hook — it's versioned and shared with all contributors

> **`--no-verify` is for emergencies only.** If you find yourself bypassing hooks regularly,
> fix the hook or fix your workflow — don't normalise skipping checks.

---

## 10. Work Plan

All tasks are tracked in Beads. Run `bd ready 2>&1 | cat` to see what's unblocked.
Run `bd list 2>&1 | cat` to see all tasks with dependency info.

The build sequence follows five phases, each building on the last:

| Phase | Goal | Key Principle |
|---|---|---|
| **0 — Setup** | C++ environment, iPlug2OOS scaffold, CI | Get an empty plugin building |
| **1 — Playable MVP** | Load sample, MIDI trigger, hear sound, initial README | Shortest path to audio output |
| **2 — Riser Pipeline** | Reverb → reverse → stretch → riser+hit playback | The core feature |
| **3 — Real-Time FX** | Stutter gate, MIDI CC, pitch shift | Expressiveness layer |
| **4 — GUI** | Full dark-theme layout, knobs, waveform view | Polish the interface |
| **5 — Release** | DAW validation (Cubase, Studio One, Logic), v0.1.0 tag | Ship it |

> **Note:** Tasks are fully defined in Beads with descriptions, priorities, and dependency
> chains. Do not duplicate the task list here — Beads is the single source of truth.

---

## 11. What "Done" Means for Any Task

A task is **done** when all of the following are true:

1. The feature works correctly and has been manually tested in a DAW
2. Relevant documentation has been updated (`README`, `CHANGELOG`, inline docs)
3. The code compiles cleanly on both Windows and macOS (no warnings treated as errors)
4. The Beads issue is closed with a meaningful reason
5. Changes are committed with the issue ID in the commit message
6. `git push` has succeeded

---

*This file is part of the RVRSE project. Keep it accurate as the project evolves.*

## Landing the Plane (Session Completion)

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File issues for remaining work** - Create issues for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Close finished work, update in-progress items
4. **PUSH TO REMOTE** - This is MANDATORY:
   ```bash
   git pull --rebase
   bd sync
   git push
   git status  # MUST show "up to date with origin"
   ```
5. **Clean up** - Clear stashes, prune remote branches
6. **Verify** - All changes committed AND pushed
7. **Hand off** - Provide context for next session

**CRITICAL RULES:**
- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- If push fails, resolve and retry until it succeeds
