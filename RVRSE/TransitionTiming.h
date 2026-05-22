#pragma once

/// @file TransitionTiming.h
/// @brief Shared helpers for riser/hit seam timing.

#include "Constants.h"
#include "TimeStretch.h"
#include "TrimUtils.h"

#include <algorithm>
#include <cmath>

namespace rvrse {

struct TransitionTiming
{
  int mBeatAlignedFrames = 0;   ///< Exact musical beat anchor for the hit midpoint
  int mEffectiveSeamFrames = 0; ///< Full seam-conditioning window (R)
  int mRiserPostBeatFrames = 0; ///< Riser extension past the beat (R/2)
  int mHitPreBeatFrames = 0;    ///< Early hit start before the beat (H/2)
  double mStretchFactor = 1.0;  ///< Target stretch ratio for the final riser render

  int HitStartFrame() const
  {
    return std::max(0, mBeatAlignedFrames - mHitPreBeatFrames);
  }
};

inline int BeatsToFrames(double beats, double bpm, double sampleRate)
{
  if (beats <= 0.0 || bpm <= 0.0 || sampleRate <= 0.0)
    return 0;

  const double samplesPerBeat = (sampleRate * 60.0) / bpm;
  return std::max(0, static_cast<int>(std::lround(beats * samplesPerBeat)));
}

inline int ComputeHitPreBeatFrames(double sampleRate)
{
  return std::max(0, TrimMsToFrames(kTrimEdgeFadeMs, sampleRate) / 2);
}

inline TransitionTiming CalculateTransitionTiming(int reversedFrames,
                                                  double riserLengthBeats,
                                                  double bpm,
                                                  double sampleRate)
{
  TransitionTiming timing;
  timing.mBeatAlignedFrames = BeatsToFrames(riserLengthBeats, bpm, sampleRate);
  timing.mHitPreBeatFrames = ComputeHitPreBeatFrames(sampleRate);

  if (reversedFrames <= 0 || riserLengthBeats <= 0.0 || bpm <= 0.0 || sampleRate <= 0.0)
    return timing;

  const double baseStretchFactor = calcStretchFactor(reversedFrames, riserLengthBeats, bpm, sampleRate);
  const double adaptiveOverlapBeats = std::min(
    kRiserOverlapBeatsBase * std::max(1.0, baseStretchFactor),
    kRiserOverlapBeatsMax
  );
  const double effectiveSeamBeats = std::max(kRiserTailFadeBeats, adaptiveOverlapBeats);

  timing.mEffectiveSeamFrames = BeatsToFrames(effectiveSeamBeats, bpm, sampleRate);
  timing.mRiserPostBeatFrames = std::max(0, static_cast<int>(std::lround(
    static_cast<double>(timing.mEffectiveSeamFrames) * 0.5
  )));

  const int totalTargetFrames = timing.mBeatAlignedFrames + timing.mRiserPostBeatFrames;
  if (totalTargetFrames > 0)
    timing.mStretchFactor = static_cast<double>(totalTargetFrames) / static_cast<double>(reversedFrames);

  return timing;
}

} // namespace rvrse
