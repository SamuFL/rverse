#pragma once

/// @file SampleData.h
/// @brief POD struct representing a loaded audio sample (stereo, float32).
///        Used by both the offline pipeline (writes) and real-time layer (reads).

#include <vector>
#include <string>
#include <atomic>

namespace rvrse {

/// Holds a loaded audio sample in deinterleaved stereo float32 format.
/// Channel layout: mLeft[i], mRight[i] for each sample frame i.
/// If the source file is mono, both channels contain identical data.
struct SampleData
{
  std::vector<float> mLeft;          ///< Left channel samples (float32)
  std::vector<float> mRight;         ///< Right channel samples (float32)
  double mSampleRate = 0.0;          ///< Original sample rate of the file
  int mNumChannels = 0;              ///< Number of channels in the source file (1 or 2)
  std::string mFilePath;             ///< Absolute path to the loaded file
  std::string mFileName;             ///< Just the filename (for display)

  /// @return Number of sample frames (per channel)
  int NumFrames() const { return static_cast<int>(mLeft.size()); }

  /// @return true if sample data has been loaded
  bool IsLoaded() const { return !mLeft.empty() && mSampleRate > 0.0; }

  /// Clear all data and reset to empty state
  void Clear()
  {
    mLeft.clear();
    mRight.clear();
    mSampleRate = 0.0;
    mNumChannels = 0;
    mFilePath.clear();
    mFileName.clear();
  }
};

/// Atomic flag for lock-free signalling between offline loader and real-time layer.
/// The offline thread sets this to true when a new sample is ready.
/// The real-time thread reads it and swaps to the new buffer.
enum class ESampleLoadState
{
  Empty = 0,    ///< No sample loaded
  Loading,      ///< Background thread is loading
  Ready,        ///< Sample loaded and ready for use
  Error         ///< Load failed
};

} // namespace rvrse
