/// @file test_smoke.cpp
/// @brief Smoke tests — verify that the test framework works and DSP headers compile.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Constants.h"
#include "BufferUtils.h"
#include "Reverb.h"
#include "TimeStretch.h"
#include "Stutter.h"

using Catch::Approx;

// ---------------------------------------------------------------------------
// Smoke: framework is alive
// ---------------------------------------------------------------------------

TEST_CASE("Smoke: Catch2 framework works", "[smoke]")
{
  REQUIRE(1 + 1 == 2);
}

// ---------------------------------------------------------------------------
// Smoke: DSP headers compile and basic invariants hold
// ---------------------------------------------------------------------------

TEST_CASE("Smoke: Constants are sane", "[smoke]")
{
  REQUIRE(rvrse::kStutterRateMinHz == 0.0);
  REQUIRE(rvrse::kStutterRateMaxHz > rvrse::kStutterRateMinHz);
  REQUIRE(rvrse::kStutterFadeMs > 0.0);
  REQUIRE(rvrse::kRiserLengthDefault >= rvrse::kRiserLengthMin);
  REQUIRE(rvrse::kRiserLengthDefault <= rvrse::kRiserLengthMax);
}

TEST_CASE("Smoke: reverseBuffer round-trips", "[smoke]")
{
  std::vector<float> buf = {1.0f, 2.0f, 3.0f, 4.0f};
  auto rev = buf;
  rvrse::reverseBuffer(rev);
  REQUIRE(rev == std::vector<float>{4.0f, 3.0f, 2.0f, 1.0f});

  rvrse::reverseBuffer(rev);
  REQUIRE(rev == buf);
}

TEST_CASE("Smoke: stretchBuffer identity", "[smoke]")
{
  std::vector<float> buf(512, 0.5f);
  auto stretched = rvrse::stretchBuffer(buf, 1.0);
  REQUIRE(stretched == buf);
}

TEST_CASE("Smoke: stutterProcess returns 1.0 when off", "[smoke]")
{
  rvrse::StutterState state;
  float gain = rvrse::stutterProcess(state, 0.0f, 1.0f, 44100.0);
  // With smoother, first sample should still be ~1.0 (starts at 1.0)
  REQUIRE(gain == Approx(1.0f).margin(0.01f));
}

TEST_CASE("Smoke: stutterReset zeroes phase", "[smoke]")
{
  rvrse::StutterState state;
  state.phase = 999;
  state.smoothedGain = 0.5f;
  rvrse::stutterReset(state);
  REQUIRE(state.phase == 0);
  REQUIRE(state.smoothedGain == 1.0f);
}
