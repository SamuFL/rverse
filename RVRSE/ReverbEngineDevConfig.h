#pragma once

/// @file ReverbEngineDevConfig.h
/// @brief Developer-facing selection and tuning for reverb engine evaluation.

namespace rvrse {

enum class EReverbEngineKind
{
  CurrentTuned,
  AirwindowsMatrixVerb,
  WDLVerbEngine
};

struct CurrentSchroederReverbDevTuning
{
  float mMinRoomFactor = 1.0f;
  float mMaxRoomFactor = 1.0f;
  float mMinFeedback = 0.90f;
  float mMaxFeedback = 0.985f;
  float mMinDamping = 0.03f;
  float mMaxDamping = 0.10f;
  float mAllpassGain = 0.62f;
  float mInputGain = 0.10f;
  bool mScaleDelayByRoomFactor = false;
  int mStereoSpreadSamplesAt44100 = 23;
};

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
  bool mAlignWetOnset = false;
  float mOnsetPeakFraction = 0.15f;
  float mOnsetAbsoluteFloor = 0.0001f;
  double mOnsetSmoothingMs = 0.5;
  double mOnsetHoldMs = 0.25;
  double mOnsetMaxTrimMs = 300.0;
  double mOnsetFadeInMs = 5.0;
  double mHeadFadeInMs = 25.0;
};

struct WDLReverbDevTuning
{
  float mRoomSizeMin = 0.55f;
  float mRoomSizeMax = 0.90f;
  float mDampingMin = 0.10f;
  float mDampingMax = 0.45f;
  float mWidth = 1.0f;
};

constexpr EReverbEngineKind kActiveReverbEngine = EReverbEngineKind::AirwindowsMatrixVerb;
constexpr bool kEnableReverbTiming = false;
constexpr CurrentSchroederReverbDevTuning kCurrentSchroederReverbDevTuning {};
constexpr AirwindowsReverbDevTuning kAirwindowsReverbDevTuning {};
constexpr WDLReverbDevTuning kWDLReverbDevTuning {};

} // namespace rvrse
