/// @file test_reverb.cpp
/// @brief Unit tests for the offline reverb seam and Airwindows adapter.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "AirwindowsReverbEngine.h"
#include "RvrseProcessor.h"
#include "ReverbEngineFactory.h"
#include "Constants.h"
#include "test_helpers.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

using Catch::Approx;
using namespace rvrse;

namespace {

/// Calculate RMS of a buffer.
float rms(const std::vector<float>& buf)
{
  if (buf.empty()) return 0.0f;
  double sum = 0.0;
  for (auto v : buf)
    sum += static_cast<double>(v) * v;
  return static_cast<float>(std::sqrt(sum / buf.size()));
}

/// Check that all samples are finite (no NaN or Inf).
bool allFinite(const std::vector<float>& buf)
{
  return std::all_of(buf.begin(), buf.end(),
    [](float v) { return std::isfinite(v); });
}

size_t firstAboveAbs(const std::vector<float>& buf, float threshold)
{
  for (size_t i = 0; i < buf.size(); ++i)
  {
    if (std::abs(buf[i]) > threshold)
      return i;
  }

  return buf.size();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Lush blend mapping
// ---------------------------------------------------------------------------

TEST_CASE("Reverb: lush blend keeps half dry at full lush", "[reverb]")
{
  REQUIRE(GetReverbDryGain(-1.0f) == Approx(1.0f));
  REQUIRE(GetReverbWetGain(-1.0f) == Approx(0.0f));
  REQUIRE(GetReverbDryGain(0.0f) == Approx(1.0f));
  REQUIRE(GetReverbWetGain(0.0f) == Approx(0.0f));
  REQUIRE(GetReverbDryGain(0.5f) == Approx(0.75f));
  REQUIRE(GetReverbWetGain(0.5f) == Approx(0.5f));
  REQUIRE(GetReverbDryGain(1.0f) == Approx(0.5f));
  REQUIRE(GetReverbWetGain(1.0f) == Approx(1.0f));
  REQUIRE(GetReverbDryGain(2.0f) == Approx(0.5f));
  REQUIRE(GetReverbWetGain(2.0f) == Approx(1.0f));
}

TEST_CASE("Reverb engine factory: current adapter is available", "[reverb]")
{
  auto engine = MakeActiveReverbEngine();
  REQUIRE(engine != nullptr);
  REQUIRE(std::string(engine->GetName()) == "AirwindowsMatrixVerb");
}

TEST_CASE("RvrseProcessor: quiet high-lush source survives reverb tail trimming", "[reverb]")
{
  const double sampleRate = 44100.0;
  const size_t burstFrames = 2048;
  const size_t totalFrames = 4096;

  auto burstL = rvrse::test::generateSine(burstFrames, 440.0, sampleRate, 0.064f);
  auto burstR = rvrse::test::generateSine(burstFrames, 330.0, sampleRate, 0.045f);

  auto sample = std::make_shared<SampleData>();
  sample->mLeft.assign(totalFrames, 0.0f);
  sample->mRight.assign(totalFrames, 0.0f);
  std::copy(burstL.begin(), burstL.end(), sample->mLeft.begin());
  std::copy(burstR.begin(), burstR.end(), sample->mRight.begin());
  sample->mSampleRate = sampleRate;
  sample->mNumChannels = 2;
  sample->mFilePath = "synthetic-quiet.wav";
  sample->mFileName = "synthetic-quiet.wav";

  RvrseProcessor processor;
  auto riser = processor.RunReverbPipelineForTests(sample, sampleRate, 0.60f, 1);
  REQUIRE(riser != nullptr);
  REQUIRE(0.5f * (rms(riser->mLeft) + rms(riser->mRight)) > 0.0001f);
}

TEST_CASE("Reverb engine adapter: Airwindows candidate produces finite output", "[reverb]")
{
  const size_t N = 8192;
  std::vector<float> inputL(N, 0.0f);
  std::vector<float> inputR(N, 0.0f);
  inputL[0] = 1.0f;
  inputR[0] = 1.0f;

  std::vector<float> outputL(N, 0.0f);
  std::vector<float> outputR(N, 0.0f);
  AirwindowsReverbEngine engine;
  engine.ProcessStereo(inputL.data(), inputR.data(),
                       outputL.data(), outputR.data(),
                       N, 48000.0, ReverbSettings { 0.7f });

  REQUIRE(allFinite(outputL));
  REQUIRE(allFinite(outputR));
  REQUIRE(rms(outputL) > 0.001f);
  REQUIRE(rms(outputR) > 0.001f);
}

TEST_CASE("Reverb engine adapter: Airwindows candidate can run without input predelay", "[reverb]")
{
  const size_t N = 4096;
  std::vector<float> inputL(N, 0.0f);
  std::vector<float> inputR(N, 0.0f);
  inputL[0] = 1.0f;
  inputR[0] = 1.0f;

  std::vector<float> defaultL(N, 0.0f);
  std::vector<float> defaultR(N, 0.0f);
  std::vector<float> zeroPreDelayL(N, 0.0f);
  std::vector<float> zeroPreDelayR(N, 0.0f);

  auto engineWithPreDelay = std::make_unique<AirwindowsMatrixVerb>();
  engineWithPreDelay->SetSampleRate(48000.0);
  engineWithPreDelay->ProcessStereo(inputL.data(), inputR.data(),
                                    defaultL.data(), defaultR.data(),
                                    N, AirwindowsMatrixVerbParams {
                                         0.90f, 0.15f, 0.10f, 0.05f, 0.85f, 0.50f, 1.0f, 1.0f
                                       });

  auto engineWithoutPreDelay = std::make_unique<AirwindowsMatrixVerb>();
  engineWithoutPreDelay->SetSampleRate(48000.0);
  engineWithoutPreDelay->ProcessStereo(inputL.data(), inputR.data(),
                                       zeroPreDelayL.data(), zeroPreDelayR.data(),
                                       N, AirwindowsMatrixVerbParams {
                                            0.90f, 0.15f, 0.10f, 0.05f, 0.85f, 0.50f, 0.0f, 1.0f
                                          });

  REQUIRE(allFinite(zeroPreDelayL));
  REQUIRE(allFinite(zeroPreDelayR));
  REQUIRE(firstAboveAbs(zeroPreDelayL, 1.0e-6f) < firstAboveAbs(defaultL, 1.0e-6f));
  REQUIRE(firstAboveAbs(zeroPreDelayR, 1.0e-6f) < firstAboveAbs(defaultR, 1.0e-6f));
}

TEST_CASE("Reverb engine adapter: Airwindows candidate is safe on worker thread", "[reverb]")
{
  const size_t N = 48000 * 2;
  const double sampleRate = 48000.0;
  auto inputL = rvrse::test::generateSine(N, 440.0, sampleRate, 0.8f);
  auto inputR = rvrse::test::generateSine(N, 660.0, sampleRate, 0.8f);
  std::vector<float> outputL(N, 0.0f);
  std::vector<float> outputR(N, 0.0f);

  std::thread worker([&]() {
    AirwindowsReverbEngine engine;
    engine.ProcessStereo(inputL.data(), inputR.data(),
                         outputL.data(), outputR.data(),
                         N, sampleRate, ReverbSettings { 0.7f });
  });

  worker.join();
  REQUIRE(allFinite(outputL));
  REQUIRE(allFinite(outputR));
}
