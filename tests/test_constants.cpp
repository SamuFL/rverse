/// @file test_constants.cpp
/// @brief Relational invariant tests for Constants.h.

#include <catch2/catch_test_macros.hpp>

#include "Constants.h"

using namespace rvrse;

TEST_CASE("Constants: relational invariants", "[constants]")
{
  SECTION("Riser length discrete values")
  {
    REQUIRE(kNumRiserLengths == 7);
    REQUIRE(kRiserLengthDefault >= 0);
    REQUIRE(kRiserLengthDefault < kNumRiserLengths);
    REQUIRE(kRiserLengthValues[0] == 0.25);
    REQUIRE(kRiserLengthValues[kNumRiserLengths - 1] == 16.0);
    // Values must be strictly increasing
    for (int i = 1; i < kNumRiserLengths; ++i)
      REQUIRE(kRiserLengthValues[i] > kRiserLengthValues[i - 1]);
  }

  SECTION("Stutter rate range")
  {
    REQUIRE(kStutterRateMinHz >= 0.0);
    REQUIRE(kStutterRateMinHz <= kStutterRateDefaultHz);
    REQUIRE(kStutterRateDefaultHz <= kStutterRateMaxHz);
    REQUIRE(kStutterRateMaxHz > 0.0);
  }

  SECTION("Reverb stability — feedback < 1.0")
  {
    REQUIRE(kReverbMinFeedback > 0.0f);
    REQUIRE(kReverbMinFeedback < 1.0f);
    REQUIRE(kReverbMaxFeedback > kReverbMinFeedback);
    REQUIRE(kReverbMaxFeedback < 1.0f);
  }

  SECTION("Reverb stability — allpass gain < 1.0")
  {
    REQUIRE(kReverbAllpassGain > 0.0f);
    REQUIRE(kReverbAllpassGain < 1.0f);
  }

  SECTION("Reverb damping in valid range (0, 1)")
  {
    REQUIRE(kReverbMinDamping > 0.0f);
    REQUIRE(kReverbMinDamping < 1.0f);
    REQUIRE(kReverbMaxDamping >= kReverbMinDamping);
    REQUIRE(kReverbMaxDamping < 1.0f);
  }

  SECTION("Room factor ordering")
  {
    REQUIRE(kReverbMinRoomFactor > 0.0f);
    REQUIRE(kReverbMaxRoomFactor > kReverbMinRoomFactor);
  }

  SECTION("Tail fade is a valid beat fraction")
  {
    REQUIRE(kRiserTailFadeBeats >= 0.0);
    REQUIRE(kRiserTailFadeBeats <= 1.0);
  }

  SECTION("Max sample frames formula")
  {
    REQUIRE(kMaxSampleFrames == kMaxSampleLengthSeconds * 192000);
  }

  SECTION("Timing constants are positive")
  {
    REQUIRE(kDefaultBPM > 0.0);
    REQUIRE(kNoteOffFadeMs > 0.0);
    REQUIRE(kStutterFadeMs > 0.0);
    REQUIRE(kReverbTailSeconds > 0.0);
  }

  SECTION("Silence threshold is positive")
  {
    REQUIRE(kSilenceThreshold > 0.0f);
    REQUIRE(kSilenceThreshold < 1.0f);
  }
}
