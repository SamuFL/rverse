#pragma once

/// @file TrimUtils.h
/// @brief Shared helpers for resolving manual sample trim ranges.

#include <algorithm>
#include <cmath>

namespace rvrse {

struct TrimRangeFrames
{
  int mStartFrame = 0;
  int mEndFrameExclusive = 0;

  int NumFrames() const { return std::max(0, mEndFrameExclusive - mStartFrame); }
  bool IsValid() const { return mEndFrameExclusive > mStartFrame; }
};

inline int TrimMsToFrames(double trimMs, double sampleRate)
{
  if (trimMs <= 0.0 || sampleRate <= 0.0)
    return 0;

  return static_cast<int>(std::lround((trimMs * sampleRate) / 1000.0));
}

inline int MinTrimRegionFrames(double sampleRate, double minRegionMs)
{
  if (sampleRate <= 0.0)
    return 1;

  return std::max(1, static_cast<int>(std::ceil((minRegionMs * sampleRate) / 1000.0)));
}

inline double FramesToTrimMs(int frames, double sampleRate)
{
  if (frames <= 0 || sampleRate <= 0.0)
    return 0.0;

  return (static_cast<double>(frames) * 1000.0) / sampleRate;
}

inline bool CanEditTrim(int totalFrames, double sampleRate, double minRegionMs)
{
  if (totalFrames <= 0 || sampleRate <= 0.0)
    return false;

  return totalFrames > MinTrimRegionFrames(sampleRate, minRegionMs);
}

inline TrimRangeFrames ResolveTrimRangeFrames(int totalFrames,
                                              double sampleRate,
                                              double trimStartMs,
                                              double trimEndMs,
                                              double minRegionMs)
{
  if (totalFrames <= 0 || sampleRate <= 0.0)
    return {};

  const int minRegionFrames = MinTrimRegionFrames(sampleRate, minRegionMs);
  if (totalFrames <= minRegionFrames)
    return {0, totalFrames};

  int trimStartFrames = std::clamp(TrimMsToFrames(trimStartMs, sampleRate), 0, totalFrames - minRegionFrames);
  int trimEndFrames = std::clamp(TrimMsToFrames(trimEndMs, sampleRate), 0, totalFrames - minRegionFrames);

  trimStartFrames = std::min(trimStartFrames, totalFrames - trimEndFrames - minRegionFrames);
  trimEndFrames = std::min(trimEndFrames, totalFrames - trimStartFrames - minRegionFrames);

  TrimRangeFrames range;
  range.mStartFrame = trimStartFrames;
  range.mEndFrameExclusive = totalFrames - trimEndFrames;

  if (range.NumFrames() < minRegionFrames)
  {
    range.mStartFrame = std::max(0, range.mEndFrameExclusive - minRegionFrames);
    range.mEndFrameExclusive = std::min(totalFrames, range.mStartFrame + minRegionFrames);
  }

  return range;
}

inline double TrimRangeToStartMs(const TrimRangeFrames& range, double sampleRate)
{
  return FramesToTrimMs(range.mStartFrame, sampleRate);
}

inline double TrimRangeToEndMs(const TrimRangeFrames& range,
                               int totalFrames,
                               double sampleRate)
{
  return FramesToTrimMs(std::max(0, totalFrames - range.mEndFrameExclusive), sampleRate);
}

} // namespace rvrse
