#pragma once

/// @file CurrentSchroederReverbEngine.h
/// @brief Adapter that exposes the current Schroeder/Moorer reverb via IReverbEngine.

#include "Reverb.h"
#include "ReverbEngine.h"
#include "ReverbEngineDevConfig.h"

namespace rvrse {

inline SchroederReverbTuning MakeCurrentSchroederReverbTuning()
{
  return SchroederReverbTuning {
    kCurrentSchroederReverbDevTuning.mMinRoomFactor,
    kCurrentSchroederReverbDevTuning.mMaxRoomFactor,
    kCurrentSchroederReverbDevTuning.mMinFeedback,
    kCurrentSchroederReverbDevTuning.mMaxFeedback,
    kCurrentSchroederReverbDevTuning.mMinDamping,
    kCurrentSchroederReverbDevTuning.mMaxDamping,
    kCurrentSchroederReverbDevTuning.mAllpassGain,
    kCurrentSchroederReverbDevTuning.mInputGain,
    kCurrentSchroederReverbDevTuning.mScaleDelayByRoomFactor,
    kCurrentSchroederReverbDevTuning.mStereoSpreadSamplesAt44100,
    { 1277, 1356, 1422, 1491, 1557, 1617, 1685, 1748 },
    { 556, 441, 341, 225 }
  };
}

class CurrentSchroederReverbEngine final : public IReverbEngine
{
public:
  const char* GetName() const override { return "CurrentTuned"; }

  void ProcessStereo(const float* inL, const float* inR,
                     float* outL, float* outR,
                     size_t numFrames,
                     double sampleRate,
                     const ReverbSettings& settings) const override
  {
    applyReverbStereo(inL, inR, outL, outR, numFrames, sampleRate,
                      settings.mLush, MakeCurrentSchroederReverbTuning());
  }
};

} // namespace rvrse
