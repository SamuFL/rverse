# Ship universal macOS builds validated under Rosetta

RVRSE macOS builds target both `arm64` and `x86_64` by default, retain macOS 10.15 as the Intel deployment floor (macOS 11 for Apple Silicon), and treat x86_64 unit tests plus VST3/AU validation under Rosetta as the Intel release gate. This spends additional CI time on every macOS build but catches architecture regressions before release; physical Intel hardware testing is desirable but is not required because the project does not have access to such hardware.
