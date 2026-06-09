#pragma once

/// @file ReverbEngine.h
/// @brief Engine-agnostic offline reverb seam for the riser pipeline.

#include <algorithm>
#include <cstddef>

namespace rvrse {

struct ReverbSettings
{
  float mLush = 0.0f;
};

inline float ClampLush(float lush)
{
  return std::clamp(lush, 0.0f, 1.0f);
}

inline float GetReverbWetGain(float lush)
{
  return ClampLush(lush);
}

inline float GetReverbDryGain(float lush)
{
  return 1.0f - (0.5f * ClampLush(lush));
}

class IReverbEngine
{
public:
  virtual ~IReverbEngine() = default;

  virtual const char* GetName() const = 0;

  virtual void ProcessStereo(const float* inL, const float* inR,
                             float* outL, float* outR,
                             size_t numFrames,
                             double sampleRate,
                             const ReverbSettings& settings) const = 0;
};

} // namespace rvrse
