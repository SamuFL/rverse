#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TransitionTiming.h"

using namespace rvrse;

TEST_CASE("TransitionTiming: derives numeric Riser Release timing", "[transition]")
{
  SECTION("Zero release is a literal bypass")
  {
    const TransitionTiming timing = CalculateTransitionTiming(
      24000, 1.0, 120.0, 48000.0, 0.0
    );

    REQUIRE(timing.mBeatAnchorFrames == 24000);
    REQUIRE(timing.mHitPreBeatFrames == 120);
    REQUIRE(timing.HitStartFrame() == 23880);
    REQUIRE(timing.mRequestedReleaseFrames == 0);
    REQUIRE(timing.mEffectiveReleaseFrames == 0);
    REQUIRE(timing.mTailFadeFrames == 0);
    REQUIRE(timing.RiserEndFrame() == 24000);
    REQUIRE(timing.mStretchFactor == Catch::Approx(1.0).margin(1e-9));
  }

  SECTION("Release extends the riser after the Beat Anchor")
  {
    const TransitionTiming timing = CalculateTransitionTiming(
      24000, 1.0, 120.0, 48000.0, 50.0
    );

    REQUIRE(timing.mRequestedReleaseFrames == 2400);
    REQUIRE(timing.mEffectiveReleaseFrames == 2400);
    REQUIRE(timing.mTailFadeFrames == 2400);
    REQUIRE(timing.RiserEndFrame() == 26400);
    REQUIRE_FALSE(timing.IsReleaseLimited());
    REQUIRE(timing.mStretchFactor == Catch::Approx(1.1).margin(1e-9));
  }

  SECTION("Release is limited to the pre-anchor duration")
  {
    const TransitionTiming timing = CalculateTransitionTiming(
      6000, 0.25, 300.0, 48000.0, 500.0
    );

    REQUIRE(timing.mBeatAnchorFrames == 2400);
    REQUIRE(timing.mRequestedReleaseFrames == 24000);
    REQUIRE(timing.mEffectiveReleaseFrames == 2400);
    REQUIRE(timing.RiserEndFrame() == 4800);
    REQUIRE(timing.IsReleaseLimited());
  }
}

TEST_CASE("TransitionTiming: preserves the v1 adaptive riser tail", "[transition]")
{
  const TransitionTiming timing = CalculateTransitionTiming(
    6000, 1.0, 120.0, 48000.0, 0.0, ETransitionMode::LegacyAdaptive
  );

  REQUIRE(timing.mMode == ETransitionMode::LegacyAdaptive);
  REQUIRE(timing.mBeatAnchorFrames == 24000);
  REQUIRE(timing.mEffectiveReleaseFrames == 3000);
  REQUIRE(timing.mTailFadeFrames == 3000);
  REQUIRE(timing.RiserEndFrame() == 27000);
  REQUIRE_FALSE(timing.IsReleaseLimited());
  REQUIRE(timing.mStretchFactor == Catch::Approx(4.5).margin(1e-9));
}

TEST_CASE("TransitionTiming: converts milliseconds consistently across sample rates", "[transition]")
{
  const TransitionTiming timing = CalculateTransitionTiming(
    22050, 1.0, 120.0, 44100.0, 50.0
  );

  REQUIRE(timing.mRequestedReleaseFrames == 2205);
  REQUIRE(timing.mEffectiveReleaseFrames == 2205);
  REQUIRE(timing.RiserEndFrame() == 24255);
}
