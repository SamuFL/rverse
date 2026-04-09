# Contributing to RVRSE

Thank you for your interest in contributing to RVRSE! This project is open
source under the [MIT License](LICENSE).

## Getting Started

1. **Fork** the repository and clone your fork.
2. Set up the build environment — see [README.md](README.md) for prerequisites.
3. Create a **feature branch** from `develop`:
   ```bash
   git checkout develop && git pull
   git checkout -b feature/your-feature-name
   ```

## Development Workflow

This project follows **git-flow**:

| Branch | Purpose |
|---|---|
| `main` | Tagged releases only |
| `develop` | Integration branch — all feature work merges here |
| `feature/<name>` | One branch per task or feature |
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
4. Open a pull request targeting `develop`.
5. Describe what you changed and why.

## Reporting Issues

Please open a GitHub issue with:
- Steps to reproduce
- Expected vs actual behaviour
- DAW, OS, and plugin format (VST3 / AU / CLAP)

## Code of Conduct

Please read and follow our [Code of Conduct](CODE_OF_CONDUCT.md).
