#pragma once

/// @file ReverbEngineDevConfig.h
/// @brief Retained Airwindows tuning for the offline reverb seam.

namespace rvrse {

struct AirwindowsReverbDevTuning
{
  float mFilterMin = 0.75f;
  float mFilterMax = 0.97f;
  float mDampingMin = 0.08f;
  float mDampingMax = 0.25f;
  float mSpeed = 0.10f;
  float mVibrato = 0.0f;
  float mRoomSizeMin = 0.52f;
  float mRoomSizeMax = 0.78f;
  float mFlavor = 0.50f;
  float mPreDelayScale = 0.0f;
  float mWetOutputGain = 3.0f;
  double mHeadFadeInMs = 25.0;
};
constexpr AirwindowsReverbDevTuning kAirwindowsReverbDevTuning {};

} // namespace rvrse
