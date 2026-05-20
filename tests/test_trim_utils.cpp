#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Constants.h"
#include "TrimUtils.h"

using namespace rvrse;

TEST_CASE("TrimUtils: full range when sample is shorter than minimum region", "[trim]")
{
  const double sampleRate = 48000.0;
  const int totalFrames = MinTrimRegionFrames(sampleRate, kTrimMinRegionMs) - 1;

  const auto range = ResolveTrimRangeFrames(totalFrames, sampleRate, 50.0, 50.0, kTrimMinRegionMs);
  REQUIRE(range.mStartFrame == 0);
  REQUIRE(range.mEndFrameExclusive == totalFrames);
  REQUIRE_FALSE(CanEditTrim(totalFrames, sampleRate, kTrimMinRegionMs));
}

TEST_CASE("TrimUtils: respects start and end trim on editable sample", "[trim]")
{
  const double sampleRate = 48000.0;
  const int totalFrames = 48000;

  const auto range = ResolveTrimRangeFrames(totalFrames, sampleRate, 100.0, 250.0, kTrimMinRegionMs);

  REQUIRE(range.mStartFrame == TrimMsToFrames(100.0, sampleRate));
  REQUIRE(range.mEndFrameExclusive == totalFrames - TrimMsToFrames(250.0, sampleRate));
  REQUIRE(range.NumFrames() == totalFrames - TrimMsToFrames(100.0, sampleRate) - TrimMsToFrames(250.0, sampleRate));
}

TEST_CASE("TrimUtils: clamps invalid combination to minimum region", "[trim]")
{
  const double sampleRate = 44100.0;
  const int totalFrames = 44100;

  const auto range = ResolveTrimRangeFrames(totalFrames, sampleRate, 990.0, 980.0, kTrimMinRegionMs);

  REQUIRE(range.NumFrames() >= MinTrimRegionFrames(sampleRate, kTrimMinRegionMs));
  REQUIRE(range.mStartFrame >= 0);
  REQUIRE(range.mEndFrameExclusive <= totalFrames);
  REQUIRE(range.mEndFrameExclusive > range.mStartFrame);
}

TEST_CASE("TrimUtils: round-trips trim projections", "[trim]")
{
  const double sampleRate = 48000.0;
  const int totalFrames = 96000;

  const auto range = ResolveTrimRangeFrames(totalFrames, sampleRate, 123.0, 321.0, kTrimMinRegionMs);

  REQUIRE(TrimRangeToStartMs(range, sampleRate) == Catch::Approx(123.0).margin(1.0));
  REQUIRE(TrimRangeToEndMs(range, totalFrames, sampleRate) == Catch::Approx(321.0).margin(1.0));
}
