# Contributing to RVRSE

Thank you for your interest in contributing to RVRSE! This project is open
source under the [MIT License](LICENSE).

## Getting Started

1. **Fork** the repository and clone your fork.
2. Set up the build environment — see [README.md](README.md) for prerequisites.
3. Find or open a GitHub issue for the work, then create a **feature branch** from `develop`
   using its issue number:
   ```bash
   git checkout develop && git pull
   git checkout -b feature/123-your-feature-name
   ```

## Development Workflow

This project follows **git-flow**:

| Branch | Purpose |
|---|---|
| `main` | Tagged releases only |
| `develop` | Integration branch — all feature work merges here |
| `feature/<issue-number>-<name>` | One branch per GitHub issue |
| `release/<version>` | Release candidates (bug fixes only) |
| `hotfix/<name>` | Emergency fixes against `main` |

**Never commit directly to `main` or `develop`.**

## Architecture Rules

RVRSE has two strictly separated layers. Please read `AGENTS.md` §4 before
writing any DSP code.

- **Offline layer** (`RvrseProcessor`) — background thread, does all heavy
  processing (reverb, reverse, stretch).
- **Real-time layer** (`RvrseVoice`) — audio thread, must be **lock-free and
  allocation-free**.

## Code Standards

- C++17, no newer features unless iPlug2 requires them.
- No allocations, exceptions, or blocking calls on the audio thread.
- No raw owning pointers — use `std::unique_ptr` / `std::vector` / `std::array`.
- Const-correctness everywhere.
- No magic numbers — constants live in `Constants.h`.

## Building & Testing

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
cmake --build build --target rvrse_tests && ctest --test-dir build
```

## Submitting Changes

1. Ensure your code **builds without warnings** on both macOS and Windows.
2. Ensure all **tests pass**.
3. Update `CHANGELOG.md` under `[Unreleased]` if your change affects
   user-facing behaviour.
4. Include the issue number in each commit subject, for example
   `Improve sample loading (#123)`.
5. Open a pull request targeting `develop` and link the issue with
   `Closes #123` when the PR completes it.
6. Describe the behavior change, implementation, and validation performed.

## Reporting Issues

Please use the repository's [GitHub issue templates](https://github.com/SamuFL/rverse/issues/new/choose)
and include:
- Steps to reproduce
- Expected vs actual behaviour
- DAW, OS, and plugin format (VST3 / AU / CLAP)

## Code of Conduct

Please read and follow our [Code of Conduct](CODE_OF_CONDUCT.md).
