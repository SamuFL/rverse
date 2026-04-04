/// @file test_stutter.cpp
/// @brief Unit tests for Stutter.h — real-time trapezoidal gate with smoother.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Stutter.h"
#include "Constants.h"

#include <cmath>
#include <vector>

using Catch::Approx;
using namespace rvrse;

// ---------------------------------------------------------------------------
// Depth = 0 → always returns 1.0 (full passthrough)
// ---------------------------------------------------------------------------

TEST_CASE("Stutter: depth=0 is passthrough", "[stutter]")
{
  StutterState state;
  const double sr = 48000.0;
  const float rateHz = 10.0f;
  const float depth = 0.0f;

  // Process 1000 samples — all should converge to 1.0
  for (int i = 0; i < 1000; ++i)
  {
    float gain = stutterProcess(state, rateHz, depth, sr);
    REQUIRE(gain == Approx(1.0f).margin(0.01f));
  }
}

// ---------------------------------------------------------------------------
// Gate symmetry — open ≈ closed duration
// ---------------------------------------------------------------------------

TEST_CASE("Stutter: gate symmetry", "[stutter]")
{
  StutterState state;
  const double sr = 48000.0;
  const float rateHz = 4.0f;
  const float depth = 1.0f;

  const int period = static_cast<int>(sr / rateHz); // 12000 samples

  // Let the smoother settle first
  for (int i = 0; i < period * 2; ++i)
    stutterProcess(state, rateHz, depth, sr);

  // Now measure one full period
  int highCount = 0;
  int lowCount = 0;
  for (int i = 0; i < period; ++i)
  {
    float gain = stutterProcess(state, rateHz, depth, sr);
    if (gain > 0.5f)
      ++highCount;
    else
      ++lowCount;
  }

  // Should be roughly 50/50 — allow 10% tolerance for ramp regions
  const float ratio = static_cast<float>(highCount) / static_cast<float>(period);
  REQUIRE(ratio == Approx(0.5f).margin(0.1f));
}

// ---------------------------------------------------------------------------
// Smoother convergence: off → on settles within ~5ms
// ---------------------------------------------------------------------------

TEST_CASE("Stutter: smoother convergence", "[stutter]")
{
  StutterState state;
  const double sr = 48000.0;

  // Start with stutter off — gain should be 1.0
  for (int i = 0; i < 100; ++i)
    stutterProcess(state, 0.0f, 1.0f, sr);
  REQUIRE(state.smoothedGain == Approx(1.0f).margin(0.01f));

  // Engage stutter — process enough samples for a full gate cycle to complete.
  // At 8 Hz and 48kHz, one full period = 6000 samples. Process several periods
  // so the one-pole smoother has time to track the gate fully.
  const int settleTime = static_cast<int>(sr / 8.0) * 4; // 4 full periods
  for (int i = 0; i < settleTime; ++i)
    stutterProcess(state, 8.0f, 1.0f, sr);

  // After settling, smoothedGain should track the gate (not stuck at 1.0)
  // Process several more periods and check that gain is oscillating
  float minGain = 1.0f, maxGain = 0.0f;
  const int measureTime = static_cast<int>(sr / 8.0) * 3; // 3 full periods
  for (int i = 0; i < measureTime; ++i)
  {
    float gain = stutterProcess(state, 8.0f, 1.0f, sr);
    minGain = std::min(minGain, gain);
    maxGain = std::max(maxGain, gain);
  }

  // With depth=1.0, gate should swing between ~0 and ~1
  REQUIRE(minGain < 0.2f);
  REQUIRE(maxGain > 0.8f);
}

// ---------------------------------------------------------------------------
// Rate change mid-stream: no sudden jumps
// ---------------------------------------------------------------------------

TEST_CASE("Stutter: rate change has no sudden jumps", "[stutter]")
{
  StutterState state;
  const double sr = 48000.0;
  const float depth = 1.0f;

  // Process at 2 Hz for a while
  float prevGain = 1.0f;
  for (int i = 0; i < 2000; ++i)
    prevGain = stutterProcess(state, 2.0f, depth, sr);

  // Switch abruptly to 15 Hz — check that max sample-to-sample delta is bounded
  float maxDelta = 0.0f;
  for (int i = 0; i < 4000; ++i)
  {
    float gain = stutterProcess(state, 15.0f, depth, sr);
    float delta = std::abs(gain - prevGain);
    maxDelta = std::max(maxDelta, delta);
    prevGain = gain;
  }

  // The one-pole smoother should limit per-sample change.
  // At 48kHz with 2ms time constant, alpha ≈ 0.01, so max delta should be small.
  REQUIRE(maxDelta < 0.1f);
}

// ---------------------------------------------------------------------------
// Phase wraps safely over long runs
// ---------------------------------------------------------------------------

TEST_CASE("Stutter: phase wraps safely", "[stutter]")
{
  StutterState state;
  const double sr = 48000.0;

  // Process 10 seconds worth of samples at 30 Hz (max rate)
  const int N = static_cast<int>(sr * 10.0);
  float lastGain = 0.0f;
  bool allFinite = true;

  for (int i = 0; i < N; ++i)
  {
    lastGain = stutterProcess(state, 30.0f, 1.0f, sr);
    if (!std::isfinite(lastGain))
    {
      allFinite = false;
      break;
    }
  }

  REQUIRE(allFinite);
  REQUIRE(lastGain >= 0.0f);
  REQUIRE(lastGain <= 1.0f);
}

// ---------------------------------------------------------------------------
// Very slow rate (0.5 Hz) still functions
// ---------------------------------------------------------------------------

TEST_CASE("Stutter: very slow rate works", "[stutter]")
{
  StutterState state;
  const double sr = 48000.0;
  const float rateHz = 0.5f;
  const float depth = 1.0f;

  // Process 3 seconds (1.5 full periods at 0.5 Hz)
  const int N = static_cast<int>(sr * 3.0);
  float minGain = 1.0f, maxGain = 0.0f;

  for (int i = 0; i < N; ++i)
  {
    float gain = stutterProcess(state, rateHz, depth, sr);
    REQUIRE(std::isfinite(gain));
    minGain = std::min(minGain, gain);
    maxGain = std::max(maxGain, gain);
  }

  // Over 1.5 periods, we should see the full range
  REQUIRE(minGain < 0.15f);
  REQUIRE(maxGain > 0.85f);
}
