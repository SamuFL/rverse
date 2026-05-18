#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "ExportRender.h"

TEST_CASE("ExportRender: mixes riser and hit at beat boundary", "[export]")
{
  rvrse::RiserData riser;
  riser.mLeft = {1.0f, 1.0f, 1.0f, 1.0f};
  riser.mRight = {0.5f, 0.5f, 0.5f, 0.5f};
  riser.mSampleRate = 48000.0;
  riser.mBeatAlignedFrames = 2;

  rvrse::SampleData hit;
  hit.mLeft = {0.25f, 0.5f, 0.75f};
  hit.mRight = {0.75f, 0.5f, 0.25f};
  hit.mSampleRate = 48000.0;
  hit.mNumChannels = 2;

  rvrse::ExportRenderConfig config;
  config.mFadeInPct = 0.5f;
  config.mRiserGain = 0.5f;
  config.mHitGain = 2.0f;

  rvrse::ExportRenderData output;
  REQUIRE(rvrse::RenderNormalExport(riser, hit, config, output));

  REQUIRE(output.IsReady());
  REQUIRE(output.mSampleRate == 48000);
  REQUIRE(output.mNumFrames == 5);

  const std::vector<float> expected = {
    0.0f, 0.0f,
    0.5f, 0.25f,
    1.0f, 1.75f,
    1.5f, 1.25f,
    1.5f, 0.5f
  };

  REQUIRE(output.mInterleaved.size() == expected.size());
  for (size_t i = 0; i < expected.size(); ++i)
    REQUIRE(output.mInterleaved[i] == Catch::Approx(expected[i]).margin(1e-6));
}

TEST_CASE("ExportRender: rejects missing render inputs", "[export]")
{
  rvrse::RiserData riser;
  rvrse::SampleData hit;
  rvrse::ExportRenderData output;

  REQUIRE_FALSE(rvrse::RenderNormalExport(riser, hit, {}, output));
  REQUIRE_FALSE(output.IsReady());
}
