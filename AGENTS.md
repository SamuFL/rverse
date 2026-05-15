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

## 2. Task Tracking — GitHub

This project uses **GitHub Issues, Milestones, and Projects** for active task tracking.
Historical Beads data is archived under `.archive/beads-historical/`, but GitHub is the only
source of truth for current work.

### Mandatory GitHub Workflow

Follow this pattern for every working session, without exception:

```
START OF SESSION
  gh issue view <number>                  # Read the issue you are addressing
  gh issue list --milestone "v1.1.0"      # Check nearby milestone context when relevant

DURING WORK
  Create follow-up GitHub issues as needed for newly discovered work
  Keep the working branch and PR aligned with the active GitHub issue

END OF SESSION  ("land the plane")
  git pull --rebase
  git push
  git status                          # Must read "up to date with origin/<branch>"
```

> **CRITICAL:** A session is NOT complete until `git push` succeeds and
> `git status` confirms you are up to date. Never say "ready to push" and stop.
> Push. Every. Time.

### Key Commands Reference

| Command | Purpose |
|---|---|
| `gh issue view <number>` | Read the issue you are working on |
| `gh issue list --milestone "v1.1.0"` | See the active milestone queue |
| `gh issue create` | Create follow-up issues discovered during implementation |
| `gh pr create --base develop` | Open a PR for review |
| `gh pr view --comments` | Review PR discussion and feedback |

### Commit Message Convention

Always include a GitHub issue or PR reference at the end of commit messages:

```
git commit -m "Archive legacy tracker docs and hooks (#26)"
git commit -m "Fix sample loading handoff after drag-and-drop merge (#28)"
```

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
- Stutter gate with real-time MIDI CC control (#123)
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
   git checkout -b feature/<issue-number>-short-description
   # ... do work, commit with the GitHub issue number in the message ...
   git checkout develop
   git merge feature/<issue-number>-short-description
   git branch -d feature/<issue-number>-short-description
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
4. **Name feature branches** using the GitHub issue number when applicable: `feature/26-sunset-beads`.

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
| **commit-msg** | Requires a GitHub issue or PR reference (`#<number>`) in every commit message. Exempts merge commits, reverts, and version tags. | `--no-verify` |
| **pre-commit** | Blocks commits containing merge conflict markers, files >5 MB, or possible secrets/API keys. Warns (non-blocking) on `std::cout`/`printf` debug statements in C++ files. | `--no-verify` |
| **pre-push** | **Blocks** direct pushes to `main` (must use release/ or hotfix/ branches). **Warns** on direct pushes to `develop` (prefer feature branches). | `--no-verify` |

### Adding New Hooks

1. Create the hook script in `hooks/` (must be executable: `chmod +x hooks/<name>`)
2. Follow the naming convention from `githooks(5)`: `pre-commit`, `commit-msg`, `pre-push`, etc.
3. Document the hook in this table
4. Commit the hook — it's versioned and shared with all contributors

> **`--no-verify` is for emergencies only.** If you find yourself bypassing hooks regularly,
> fix the hook or fix your workflow — don't normalise skipping checks.

---

## 10. Work Plan

All tasks are tracked in GitHub. Use the issue tracker, the active milestone, and the project
board to understand what is queued, in progress, and complete.

The build sequence follows five phases, each building on the last:

| Phase | Goal | Key Principle |
|---|---|---|
| **0 — Setup** | C++ environment, iPlug2OOS scaffold, CI | Get an empty plugin building |
| **1 — Playable MVP** | Load sample, MIDI trigger, hear sound, initial README | Shortest path to audio output |
| **2 — Riser Pipeline** | Reverb → reverse → stretch → riser+hit playback | The core feature |
| **3 — Real-Time FX** | Stutter gate, MIDI CC, pitch shift | Expressiveness layer |
| **4 — GUI** | Full dark-theme layout, knobs, waveform view | Polish the interface |
| **5 — Release** | DAW validation (Cubase, Studio One, Logic), v0.1.0 tag | Ship it |

> **Note:** Tasks are fully defined in GitHub Issues and organized via Milestones and the project
> board. Do not duplicate the task list here — GitHub is the single source of truth.

---

## 11. What "Done" Means for Any Task

A task is **done** when all of the following are true:

1. The feature works correctly and has been manually tested in a DAW
2. Relevant documentation has been updated (`README`, `CHANGELOG`, inline docs)
3. The code compiles cleanly on both Windows and macOS (no warnings treated as errors)
4. The GitHub issue or PR is updated appropriately
5. Changes are committed with the relevant `#<number>` reference in the commit message
6. `git push` has succeeded

---

*This file is part of the RVRSE project. Keep it accurate as the project evolves.*

## Landing the Plane (Session Completion)

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File issues for remaining work** - Create issues for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Update the GitHub issue/PR with the outcome and any follow-up work
4. **PUSH TO REMOTE** - This is MANDATORY:
   ```bash
   git pull --rebase
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
