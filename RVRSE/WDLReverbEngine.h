#pragma once

/// @file WDLReverbEngine.h
/// @brief Cockos WDL reverb adapter for the offline reverb seam.

#include "ReverbEngine.h"
#include "ReverbEngineDevConfig.h"

#include "../iPlug2/WDL/verbengine.h"

#include <algorithm>
#include <vector>

namespace rvrse {

class WDLReverbEngineAdapter final : public IReverbEngine
{
public:
  const char* GetName() const override { return "WDLVerbEngine"; }

  void ProcessStereo(const float* inL, const float* inR,
                     float* outL, float* outR,
                     size_t numFrames,
                     double sampleRate,
                     const ReverbSettings& settings) const override
  {
    const float lush = ClampLush(settings.mLush);

    WDL_ReverbEngine engine;
    engine.SetSampleRate(sampleRate);
    engine.SetRoomSize(kWDLReverbDevTuning.mRoomSizeMin
                       + lush * (kWDLReverbDevTuning.mRoomSizeMax - kWDLReverbDevTuning.mRoomSizeMin));
    engine.SetDampening(kWDLReverbDevTuning.mDampingMin
                        + lush * (kWDLReverbDevTuning.mDampingMax - kWDLReverbDevTuning.mDampingMin));
    engine.SetWidth(kWDLReverbDevTuning.mWidth);
    engine.Reset(true);

    std::vector<double> wetInL(numFrames);
    std::vector<double> wetInR(numFrames);
    std::vector<double> wetOutL(numFrames);
    std::vector<double> wetOutR(numFrames);

    for (size_t i = 0; i < numFrames; ++i)
    {
      wetInL[i] = inL[i];
      wetInR[i] = inR[i];
    }

    engine.ProcessSampleBlock(wetInL.data(), wetInR.data(), wetOutL.data(), wetOutR.data(),
                              static_cast<int>(numFrames));

    const double dry = GetReverbDryGain(lush);
    const double wet = GetReverbWetGain(lush);
    for (size_t i = 0; i < numFrames; ++i)
    {
      outL[i] = static_cast<float>(inL[i] * dry + wetOutL[i] * wet);
      outR[i] = static_cast<float>(inR[i] * dry + wetOutR[i] * wet);
    }
  }
};

} // namespace rvrse
