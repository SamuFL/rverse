#pragma once

/// @file SampleLoader.h
/// @brief Stateless functions for loading audio files into SampleData.
///        OFFLINE ONLY — never call from the audio thread.
///
/// Supports WAV and AIFF via dr_wav (header-only from dr_libs).
/// All functions allocate memory and are therefore forbidden on the audio thread.

#include "SampleData.h"
#include <string>

namespace rvrse {

/// Result of a sample loading operation.
struct SampleLoadResult
{
  bool success = false;
  std::string errorMessage;
  SampleData data;
};

/// Load an audio file from disk into a SampleData struct.
/// Supports WAV (.wav) and AIFF (.aif, .aiff) formats.
///
/// @param filePath  Absolute path to the audio file.
/// @return SampleLoadResult with success/failure and loaded data.
///
/// @note This function allocates memory. OFFLINE ONLY.
/// @note If the source is mono, both L and R channels will contain identical data.
/// @note If the source has more than 2 channels, only the first 2 are used.
SampleLoadResult LoadSample(const std::string& filePath);

/// Extract just the filename from a full path.
/// @param filePath  Full path string (e.g., "/Users/foo/kick.wav")
/// @return Just the filename portion (e.g., "kick.wav")
std::string ExtractFileName(const std::string& filePath);

/// Check if a file extension is a supported audio format.
/// @param filePath  Path or filename to check
/// @return true if the extension is .wav, .aif, or .aiff (case-insensitive)
bool IsSupportedAudioFile(const std::string& filePath);

} // namespace rvrse
