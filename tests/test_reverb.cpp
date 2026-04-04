/// @file test_reverb.cpp
/// @brief Unit tests for Reverb.h — Schroeder/Moorer algorithmic reverb.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Reverb.h"
#include "Constants.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
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

} // anonymous namespace

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
  // Unit-amplitude sine wave — reverb should not produce runaway gain
  const size_t N = 8192;
  std::vector<float> input(N);
  for (size_t i = 0; i < N; ++i)
    input[i] = std::sin(static_cast<float>(i) * 0.1f);

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

TEST_CASE("Reverb: energy increases with lush amount", "[reverb]")
{
  const size_t N = 48000; // 1 second
  std::vector<float> input(N, 0.0f);
  input[0] = 1.0f; // Impulse

  std::vector<float> outLow(N), outHigh(N);
  applyReverb(input.data(), outLow.data(), N, 48000.0, 0.25f);
  applyReverb(input.data(), outHigh.data(), N, 48000.0, 0.75f);

  REQUIRE(rms(outHigh) > rms(outLow));
}
