/// @file test_time_stretch.cpp
/// @brief Unit tests for TimeStretch.h — signalsmith-stretch based time-stretcher.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TimeStretch.h"
#include "Constants.h"

#include <cmath>
#include <numeric>
#include <vector>

using Catch::Approx;
using namespace rvrse;

// ---------------------------------------------------------------------------
// Factor 1.0 → identity
// ---------------------------------------------------------------------------

TEST_CASE("TimeStretch: factor 1.0 is identity", "[timestretch]")
{
  std::vector<float> buf(2048, 0.5f);
  auto result = stretchBuffer(buf, 1.0);
  REQUIRE(result == buf);
}

// ---------------------------------------------------------------------------
// Factor 2.0 → doubles length
// ---------------------------------------------------------------------------

TEST_CASE("TimeStretch: factor 2.0 doubles length", "[timestretch]")
{
  std::vector<float> buf(4096, 0.3f);
  auto result = stretchBuffer(buf, 2.0, 44100.0);

  // Spectral stretcher should produce exact target length
  const int expected = static_cast<int>(std::round(4096 * 2.0));
  REQUIRE(result.size() == static_cast<size_t>(expected));
}

// ---------------------------------------------------------------------------
// Factor 0.5 → halves length
// ---------------------------------------------------------------------------

TEST_CASE("TimeStretch: factor 0.5 halves length", "[timestretch]")
{
  std::vector<float> buf(4096, 0.3f);
  auto result = stretchBuffer(buf, 0.5, 44100.0);

  const int expected = static_cast<int>(std::round(4096 * 0.5));
  REQUIRE(result.size() == static_cast<size_t>(expected));
}

// ---------------------------------------------------------------------------
// Empty input → empty output
// ---------------------------------------------------------------------------

TEST_CASE("TimeStretch: empty input returns empty", "[timestretch]")
{
  std::vector<float> empty;
  auto result = stretchBuffer(empty, 2.0);
  REQUIRE(result.empty());
}

// ---------------------------------------------------------------------------
// Invalid factors
// ---------------------------------------------------------------------------

TEST_CASE("TimeStretch: invalid factor returns empty", "[timestretch]")
{
  std::vector<float> buf(1024, 0.5f);

  SECTION("Factor = 0")
  {
    auto result = stretchBuffer(buf, 0.0);
    REQUIRE(result.empty());
  }

  SECTION("Negative factor")
  {
    auto result = stretchBuffer(buf, -1.0);
    REQUIRE(result.empty());
  }
}

// ---------------------------------------------------------------------------
// Constant signal → amplitude roughly preserved
// ---------------------------------------------------------------------------

TEST_CASE("TimeStretch: constant signal amplitude roughly preserved", "[timestretch]")
{
  const float kValue = 0.5f;
  std::vector<float> buf(4096, kValue);
  auto result = stretchBuffer(buf, 1.5, 44100.0);

  REQUIRE(!result.empty());

  // Compute RMS of the middle 50% (avoid edge effects)
  const size_t start = result.size() / 4;
  const size_t end = result.size() * 3 / 4;
  double sumSq = 0.0;
  for (size_t i = start; i < end; ++i)
    sumSq += static_cast<double>(result[i]) * result[i];
  const double rms = std::sqrt(sumSq / static_cast<double>(end - start));

  // RMS of a constant 0.5 signal is 0.5. Allow generous margin for spectral artifacts.
  REQUIRE(rms == Approx(kValue).margin(0.15));
}

// ---------------------------------------------------------------------------
// Stereo stretching produces correct length
// ---------------------------------------------------------------------------

TEST_CASE("TimeStretch: stereo stretch correct length", "[timestretch]")
{
  std::vector<float> bufL(4096, 0.3f);
  std::vector<float> bufR(4096, 0.3f);
  std::vector<float> outL, outR;

  stretchBufferStereo(bufL, bufR, 2.0, outL, outR, 44100.0);

  const int expected = static_cast<int>(std::round(4096 * 2.0));
  REQUIRE(outL.size() == static_cast<size_t>(expected));
  REQUIRE(outR.size() == static_cast<size_t>(expected));
}

// ---------------------------------------------------------------------------
// calcStretchFactor correctness
// ---------------------------------------------------------------------------

TEST_CASE("TimeStretch: calcStretchFactor", "[timestretch]")
{
  // 4 beats at 120 BPM, 48kHz
  // 1 beat = 60/120 * 48000 = 24000 samples
  // 4 beats = 96000 samples
  // Source = 48000 samples (1 second)
  // Factor = 96000 / 48000 = 2.0
  const double factor = calcStretchFactor(48000, 4.0, 120.0, 48000.0);
  REQUIRE(factor == Approx(2.0).margin(0.001));

  // 2 beats at 120 BPM → factor = 48000 / 48000 = 1.0
  const double factor2 = calcStretchFactor(48000, 2.0, 120.0, 48000.0);
  REQUIRE(factor2 == Approx(1.0).margin(0.001));

  // 8 beats at 60 BPM, 44100 Hz
  // 1 beat = 60/60 * 44100 = 44100 samples
  // 8 beats = 352800 samples
  // Source = 44100 samples
  // Factor = 352800 / 44100 = 8.0
  const double factor3 = calcStretchFactor(44100, 8.0, 60.0, 44100.0);
  REQUIRE(factor3 == Approx(8.0).margin(0.001));
}
