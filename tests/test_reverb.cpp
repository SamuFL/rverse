/// @file test_reverb.cpp
/// @brief Unit tests for Reverb.h — Schroeder/Moorer algorithmic reverb.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "AirwindowsReverbEngine.h"
#define private public
#include "RvrseProcessor.h"
#undef private
#include "Reverb.h"
#include "ReverbEngineFactory.h"
#include "Constants.h"
#include "WDLReverbEngine.h"
#include "test_helpers.h"

#include <algorithm>
#include <cmath>
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

// ---------------------------------------------------------------------------
// Lush = 0 → dry passthrough
// ---------------------------------------------------------------------------

TEST_CASE("Reverb: lush=0 is passthrough", "[reverb]")
{
  const size_t N = 1024;
  std::vector<float> input(N);
  std::iota(input.begin(), input.end(), 0.0f);

  std::vector<float> output(N, -1.0f);
  applyReverb(input.data(), output.data(), N, 48000.0, 0.0f);

  REQUIRE(output == input);
}

// ---------------------------------------------------------------------------
// Impulse response has energy
// ---------------------------------------------------------------------------

TEST_CASE("Reverb: impulse response is non-silent", "[reverb]")
{
  // Feed a single 1.0 sample followed by silence — reverb tail should ring
  const size_t N = 48000; // 1 second at 48kHz
  std::vector<float> input(N, 0.0f);
  input[0] = 1.0f;

  std::vector<float> output(N, 0.0f);
  applyReverb(input.data(), output.data(), N, 48000.0, 0.8f);

  // The output should have noticeable energy from the reverb tail
  float energy = rms(output);
  REQUIRE(energy > 0.001f);

  // The tail should extend well beyond the first sample
  // Check that there's energy in the second half
  float lateEnergy = 0.0f;
  for (size_t i = N / 2; i < N; ++i)
    lateEnergy += output[i] * output[i];
  REQUIRE(lateEnergy > 0.0f);
}

// ---------------------------------------------------------------------------
// No NaN/Inf in output (stability check)
// ---------------------------------------------------------------------------

TEST_CASE("Reverb: no NaN/Inf with white noise input", "[reverb]")
{
  const size_t N = 8192;
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

  std::vector<float> input(N);
  for (auto& v : input)
    v = dist(rng);

  // Test at multiple lush values
  for (float lush : {0.1f, 0.5f, 0.8f, 1.0f})
  {
    std::vector<float> output(N);
    applyReverb(input.data(), output.data(), N, 48000.0, lush);
    REQUIRE(allFinite(output));
  }
}

// ---------------------------------------------------------------------------
// Wet mix stays within reasonable headroom
// ---------------------------------------------------------------------------

TEST_CASE("Reverb: output stays within headroom", "[reverb]")
{
  // Unit-amplitude 440 Hz sine wave — reverb should not produce runaway gain
  const size_t N = 8192;
  const double sampleRate = 48000.0;
  auto input = rvrse::test::generateSine(N, 440.0, sampleRate);

  std::vector<float> output(N);
  applyReverb(input.data(), output.data(), N, 48000.0, 1.0f);

  float peak = *std::max_element(output.begin(), output.end(),
    [](float a, float b) { return std::abs(a) < std::abs(b); });

  // Peak should be within a reasonable bound.
  // Schroeder reverb with 8 combs can accumulate energy — allow up to 8.0 for unit input.
  REQUIRE(std::abs(peak) < 8.0f);
}

// ---------------------------------------------------------------------------
// Energy increases with lush
// ---------------------------------------------------------------------------

TEST_CASE("Reverb: tail energy increases with lush amount", "[reverb]")
{
  const size_t N = 48000; // 1 second
  std::vector<float> input(N, 0.0f);
  input[0] = 1.0f; // Impulse

  std::vector<float> outLow(N), outHigh(N);
  applyReverb(input.data(), outLow.data(), N, 48000.0, 0.25f);
  applyReverb(input.data(), outHigh.data(), N, 48000.0, 0.75f);

  // Compare tail-only energy (skip sample 0 where the dry impulse dominates).
  // lushAmount controls both room size and wet gain, so comparing full RMS would
  // overemphasize the retained dry anchor at the start of the impulse.
  auto tailRms = [](const std::vector<float>& buf, size_t start) {
    double sum = 0.0;
    for (size_t i = start; i < buf.size(); ++i)
      sum += static_cast<double>(buf[i]) * buf[i];
    return static_cast<float>(std::sqrt(sum / (buf.size() - start)));
  };

  REQUIRE(tailRms(outHigh, 1) > tailRms(outLow, 1));
}

TEST_CASE("Reverb engine factory: current adapter is available", "[reverb]")
{
  auto engine = MakeActiveReverbEngine();
  REQUIRE(engine != nullptr);
  if constexpr (kActiveReverbEngine == EReverbEngineKind::CurrentTuned)
    REQUIRE(std::string(engine->GetName()) == "CurrentTuned");
  else if constexpr (kActiveReverbEngine == EReverbEngineKind::AirwindowsMatrixVerb)
    REQUIRE(std::string(engine->GetName()) == "AirwindowsMatrixVerb");
  else if constexpr (kActiveReverbEngine == EReverbEngineKind::WDLVerbEngine)
    REQUIRE(std::string(engine->GetName()) == "WDLVerbEngine");
}

TEST_CASE("Reverb engine adapter: current engine matches direct Schroeder call", "[reverb]")
{
  const size_t N = 2048;
  const double sampleRate = 48000.0;
  auto inL = rvrse::test::generateSine(N, 220.0, sampleRate);
  auto inR = rvrse::test::generateSine(N, 330.0, sampleRate);

  std::vector<float> viaAdapterL(N, 0.0f);
  std::vector<float> viaAdapterR(N, 0.0f);
  std::vector<float> directL(N, 0.0f);
  std::vector<float> directR(N, 0.0f);

  CurrentSchroederReverbEngine engine;

  engine.ProcessStereo(inL.data(), inR.data(),
                       viaAdapterL.data(), viaAdapterR.data(),
                       N, sampleRate, ReverbSettings { 0.6f });

  applyReverbStereo(inL.data(), inR.data(),
                    directL.data(), directR.data(),
                    N, sampleRate, 0.6f, MakeCurrentSchroederReverbTuning());

  REQUIRE(viaAdapterL == directL);
  REQUIRE(viaAdapterR == directR);
}

TEST_CASE("Reverb engine adapter: CurrentTuned is exact passthrough at zero lush", "[reverb]")
{
  const size_t N = 2048;
  const double sampleRate = 48000.0;
  auto inL = rvrse::test::generateSine(N, 220.0, sampleRate, 0.8f);
  auto inR = rvrse::test::generateSine(N, 330.0, sampleRate, 0.8f);

  std::vector<float> outL(N, -1.0f);
  std::vector<float> outR(N, -1.0f);

  CurrentSchroederReverbEngine engine;
  engine.ProcessStereo(inL.data(), inR.data(),
                       outL.data(), outR.data(),
                       N, sampleRate, ReverbSettings { 0.0f });

  REQUIRE(outL == inL);
  REQUIRE(outR == inR);
}

TEST_CASE("Reverb engine adapter: CurrentTuned does not collapse at high lush", "[reverb]")
{
  const size_t N = 48000;
  const double sampleRate = 48000.0;
  auto inL = rvrse::test::generateSine(N, 440.0, sampleRate, 0.8f);
  auto inR = rvrse::test::generateSine(N, 660.0, sampleRate, 0.8f);

  std::vector<float> lowL(N, 0.0f), lowR(N, 0.0f);
  std::vector<float> highL(N, 0.0f), highR(N, 0.0f);

  CurrentSchroederReverbEngine engine;
  engine.ProcessStereo(inL.data(), inR.data(),
                       lowL.data(), lowR.data(),
                       N, sampleRate, ReverbSettings { 0.25f });
  engine.ProcessStereo(inL.data(), inR.data(),
                       highL.data(), highR.data(),
                       N, sampleRate, ReverbSettings { 1.0f });

  const float lowRms = 0.5f * (rms(lowL) + rms(lowR));
  const float highRms = 0.5f * (rms(highL) + rms(highR));

  REQUIRE(highRms > lowRms * 0.45f);
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
  processor.mSourceSample = sample;
  processor.mOutputSampleRate = sampleRate;
  processor.mSequenceId = 1;
  processor.mLush = 0.60f;
  processor.mGeneration.store(1, std::memory_order_release);

  processor.runPipeline(RvrseProcessor::EPipelineStage::Reverb, 1);

  auto riser = processor.peekRiser();
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

  AirwindowsMatrixVerb engineWithPreDelay;
  engineWithPreDelay.SetSampleRate(48000.0);
  engineWithPreDelay.ProcessStereo(inputL.data(), inputR.data(),
                                   defaultL.data(), defaultR.data(),
                                   N, AirwindowsMatrixVerbParams {
                                        0.90f, 0.15f, 0.10f, 0.05f, 0.85f, 0.50f, 1.0f, 1.0f
                                      });

  AirwindowsMatrixVerb engineWithoutPreDelay;
  engineWithoutPreDelay.SetSampleRate(48000.0);
  engineWithoutPreDelay.ProcessStereo(inputL.data(), inputR.data(),
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

TEST_CASE("Reverb engine adapter: WDL candidate produces finite output", "[reverb]")
{
  const size_t N = 8192;
  std::vector<float> inputL(N, 0.0f);
  std::vector<float> inputR(N, 0.0f);
  inputL[0] = 1.0f;
  inputR[0] = 1.0f;

  std::vector<float> outputL(N, 0.0f);
  std::vector<float> outputR(N, 0.0f);
  WDLReverbEngineAdapter engine;
  engine.ProcessStereo(inputL.data(), inputR.data(),
                       outputL.data(), outputR.data(),
                       N, 48000.0, ReverbSettings { 0.7f });

  REQUIRE(allFinite(outputL));
  REQUIRE(allFinite(outputR));
  REQUIRE(rms(outputL) > 0.0001f);
  REQUIRE(rms(outputR) > 0.0001f);
}
