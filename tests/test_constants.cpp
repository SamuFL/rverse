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

  SECTION("Tail fade is a valid beat fraction")
  {
    REQUIRE(kRiserTailFadeBeats >= 0.0);
    REQUIRE(kRiserTailFadeBeats <= 1.0);
  }

  SECTION("Riser Release range contains the default")
  {
    REQUIRE(kRiserReleaseMinMs == 0.0);
    REQUIRE(kRiserReleaseMinMs < kRiserReleaseDefaultMs);
    REQUIRE(kRiserReleaseDefaultMs <= kRiserReleaseMaxMs);
    REQUIRE(kRiserReleaseStepMs > 0.0);
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
