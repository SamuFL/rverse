#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TransitionTiming.h"

using namespace rvrse;

TEST_CASE("TransitionTiming: derives seam-centered timing from current constants", "[transition]")
{
  SECTION("Uses tail fade as the effective seam at 1x stretch")
  {
    const TransitionTiming timing = CalculateTransitionTiming(24000, 1.0, 120.0, 48000.0);

    REQUIRE(timing.mBeatAlignedFrames == 24000);
    REQUIRE(timing.mEffectiveSeamFrames == 1500);
    REQUIRE(timing.mRiserPostBeatFrames == 750);
    REQUIRE(timing.mHitPreBeatFrames == 120);
    REQUIRE(timing.HitStartFrame() == 23880);
    REQUIRE(timing.mStretchFactor == Catch::Approx(1.03125).margin(1e-9));
  }

  SECTION("Adaptive overlap can dominate the seam window at high stretch")
  {
    const TransitionTiming timing = CalculateTransitionTiming(6000, 1.0, 120.0, 48000.0);

    REQUIRE(timing.mBeatAlignedFrames == 24000);
    REQUIRE(timing.mEffectiveSeamFrames == 3000);
    REQUIRE(timing.mRiserPostBeatFrames == 1500);
    REQUIRE(timing.mHitPreBeatFrames == 120);
    REQUIRE(timing.HitStartFrame() == 23880);
    REQUIRE(timing.mStretchFactor == Catch::Approx(4.25).margin(1e-9));
  }
}
