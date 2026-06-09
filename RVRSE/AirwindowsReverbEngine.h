#pragma once

/// @file AirwindowsReverbEngine.h
/// @brief Airwindows MatrixVerb adapter for the offline reverb seam.

#include "AirwindowsMatrixVerb.h"
#include "ReverbEngine.h"
#include "ReverbEngineDevConfig.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace rvrse {

inline AirwindowsMatrixVerbParams MakeAirwindowsMatrixVerbParams(const ReverbSettings& settings)
{
  const float lush = ClampLush(settings.mLush);

  return AirwindowsMatrixVerbParams {
    kAirwindowsReverbDevTuning.mFilterMin + lush * (kAirwindowsReverbDevTuning.mFilterMax - kAirwindowsReverbDevTuning.mFilterMin),
    kAirwindowsReverbDevTuning.mDampingMin + lush * (kAirwindowsReverbDevTuning.mDampingMax - kAirwindowsReverbDevTuning.mDampingMin),
    kAirwindowsReverbDevTuning.mSpeed,
    kAirwindowsReverbDevTuning.mVibrato,
    kAirwindowsReverbDevTuning.mRoomSizeMin + lush * (kAirwindowsReverbDevTuning.mRoomSizeMax - kAirwindowsReverbDevTuning.mRoomSizeMin),
    kAirwindowsReverbDevTuning.mFlavor,
    kAirwindowsReverbDevTuning.mPreDelayScale,
    1.0f
  };
}

class AirwindowsReverbEngine final : public IReverbEngine
{
public:
  const char* GetName() const override { return "AirwindowsMatrixVerb"; }

  void ProcessStereo(const float* inL, const float* inR,
                     float* outL, float* outR,
                     size_t numFrames,
                     double sampleRate,
                     const ReverbSettings& settings) const override
  {
    const float dryGain = GetReverbDryGain(settings.mLush);
    const float wetGain = GetReverbWetGain(settings.mLush);

    if (wetGain <= 0.0f || numFrames == 0)
    {
      std::copy_n(inL, numFrames, outL);
      std::copy_n(inR, numFrames, outR);
      return;
    }

    auto processor = std::make_unique<AirwindowsMatrixVerb>();
    processor->SetSampleRate(sampleRate);
    std::vector<float> wetL(numFrames, 0.0f);
    std::vector<float> wetR(numFrames, 0.0f);
    processor->ProcessStereo(inL, inR, wetL.data(), wetR.data(), numFrames, MakeAirwindowsMatrixVerbParams(settings));

    for (size_t i = 0; i < numFrames; ++i)
    {
      outL[i] = inL[i] * dryGain + wetL[i] * wetGain * kAirwindowsReverbDevTuning.mWetOutputGain;
      outR[i] = inR[i] * dryGain + wetR[i] * wetGain * kAirwindowsReverbDevTuning.mWetOutputGain;
    }
  }
};

} // namespace rvrse
