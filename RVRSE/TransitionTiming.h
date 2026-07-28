#pragma once

/// @file TransitionTiming.h
/// @brief Shared helpers for riser/hit seam timing.

#include "Constants.h"
#include "TimeStretch.h"
#include "TrimUtils.h"

#include <algorithm>
#include <cmath>

namespace rvrse {

enum class ETransitionMode
{
  RiserRelease = 0,
  LegacyAdaptive
};

struct TransitionTiming
{
  int mBeatAnchorFrames = 0;       ///< Exact musical anchor for the hit ramp midpoint
  int mHitPreBeatFrames = 0;       ///< Early hit start before the anchor (H/2)
  int mRequestedReleaseFrames = 0; ///< User-requested release before limiting
  int mEffectiveReleaseFrames = 0; ///< Post-anchor riser duration after limiting
  int mTailFadeFrames = 0;         ///< Fade length (differs from release only in legacy mode)
  double mStretchFactor = 1.0;     ///< Target stretch ratio for the final riser render
  ETransitionMode mMode = ETransitionMode::RiserRelease;

  int HitStartFrame() const
  {
    return (std::max)(0, mBeatAnchorFrames - mHitPreBeatFrames);
  }

  int RiserEndFrame() const
  {
    return mBeatAnchorFrames + mEffectiveReleaseFrames;
  }

  bool IsReleaseLimited() const
  {
    return mMode == ETransitionMode::RiserRelease &&
           mEffectiveReleaseFrames < mRequestedReleaseFrames;
  }
};

inline int BeatsToFrames(double beats, double bpm, double sampleRate)
{
  if (beats <= 0.0 || bpm <= 0.0 || sampleRate <= 0.0)
    return 0;

  const double samplesPerBeat = (sampleRate * 60.0) / bpm;
  return (std::max)(0, static_cast<int>(std::lround(beats * samplesPerBeat)));
}

inline int ComputeHitPreBeatFrames(double sampleRate)
{
  return (std::max)(0, TrimMsToFrames(kTrimEdgeFadeMs, sampleRate) / 2);
}

inline TransitionTiming CalculateTransitionTiming(int reversedFrames,
                                                  double riserLengthBeats,
                                                  double bpm,
                                                  double sampleRate,
                                                  double requestedReleaseMs,
                                                  ETransitionMode mode = ETransitionMode::RiserRelease)
{
  TransitionTiming timing;
  timing.mMode = mode;
  timing.mBeatAnchorFrames = BeatsToFrames(riserLengthBeats, bpm, sampleRate);
  timing.mHitPreBeatFrames = ComputeHitPreBeatFrames(sampleRate);
  timing.mRequestedReleaseFrames = TrimMsToFrames(
    std::clamp(requestedReleaseMs, kRiserReleaseMinMs, kRiserReleaseMaxMs),
    sampleRate
  );

  if (reversedFrames <= 0 || riserLengthBeats <= 0.0 || bpm <= 0.0 || sampleRate <= 0.0)
    return timing;

  if (mode == ETransitionMode::LegacyAdaptive)
  {
    // Tagged v1.0 used the full adaptive overlap after the Beat Anchor. The
    // later issue #42 half-seam heuristic is intentionally not the legacy mode.
    const double baseStretchFactor = calcStretchFactor(
      reversedFrames, riserLengthBeats, bpm, sampleRate
    );
    const double adaptiveOverlapBeats = (std::min)(
      kRiserOverlapBeatsBase * (std::max)(1.0, baseStretchFactor),
      kRiserOverlapBeatsMax
    );
    timing.mEffectiveReleaseFrames = BeatsToFrames(adaptiveOverlapBeats, bpm, sampleRate);
    timing.mTailFadeFrames = BeatsToFrames(
      (std::max)(kRiserTailFadeBeats, adaptiveOverlapBeats), bpm, sampleRate
    );
  }
  else
  {
    timing.mEffectiveReleaseFrames = (std::min)(
      timing.mRequestedReleaseFrames, timing.mBeatAnchorFrames
    );
    timing.mTailFadeFrames = timing.mEffectiveReleaseFrames;
  }

  const int totalTargetFrames = timing.RiserEndFrame();
  if (totalTargetFrames > 0)
    timing.mStretchFactor = static_cast<double>(totalTargetFrames) / static_cast<double>(reversedFrames);

  return timing;
}

} // namespace rvrse
