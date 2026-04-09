/// @file test_buffer_utils.cpp
/// @brief Unit tests for BufferUtils.h — reverse, resample, fade, trim.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "BufferUtils.h"
#include "Constants.h"

#include <cmath>
#include <numeric>

using Catch::Approx;
using namespace rvrse;

// ---------------------------------------------------------------------------
// reverseBuffer
// ---------------------------------------------------------------------------

TEST_CASE("BufferUtils: reverseBuffer empty and length-1", "[bufferutils]")
{
  SECTION("Empty buffer")
  {
    std::vector<float> empty;
    reverseBuffer(empty);
    REQUIRE(empty.empty());
  }

  SECTION("Single element is identity")
  {
    std::vector<float> one = {42.0f};
    reverseBuffer(one);
    REQUIRE(one == std::vector<float>{42.0f});
  }
}

TEST_CASE("BufferUtils: reverseBuffer round-trip", "[bufferutils]")
{
  std::vector<float> buf = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
  const auto original = buf;

  reverseBuffer(buf);
  REQUIRE(buf == std::vector<float>{5.0f, 4.0f, 3.0f, 2.0f, 1.0f});

  reverseBuffer(buf);
  REQUIRE(buf == original);
}

TEST_CASE("BufferUtils: reverseBufferStereo independence", "[bufferutils]")
{
  std::vector<float> left  = {1.0f, 2.0f, 3.0f};
  std::vector<float> right = {10.0f, 20.0f, 30.0f};

  reverseBufferStereo(left, right);

  REQUIRE(left  == std::vector<float>{3.0f, 2.0f, 1.0f});
  REQUIRE(right == std::vector<float>{30.0f, 20.0f, 10.0f});
}

// ---------------------------------------------------------------------------
// resampleLinear
// ---------------------------------------------------------------------------

TEST_CASE("BufferUtils: resampleLinear same-rate passthrough", "[bufferutils]")
{
  std::vector<float> buf(256);
  std::iota(buf.begin(), buf.end(), 0.0f);

  auto result = resampleLinear(buf, 48000.0, 48000.0);
  REQUIRE(result == buf);
}

TEST_CASE("BufferUtils: resampleLinear 2x downsample", "[bufferutils]")
{
  // 1000 samples at 96kHz -> ~500 samples at 48kHz
  std::vector<float> buf(1000, 1.0f);
  auto result = resampleLinear(buf, 96000.0, 48000.0);

  REQUIRE(result.size() == Approx(500).margin(2));

  // All values should be ~1.0 since input is constant
  for (auto v : result)
    REQUIRE(v == Approx(1.0f).margin(0.01f));
}

TEST_CASE("BufferUtils: resampleLinear empty input", "[bufferutils]")
{
  std::vector<float> empty;
  auto result = resampleLinear(empty, 44100.0, 48000.0);
  REQUIRE(result.empty());
}

// ---------------------------------------------------------------------------
// applyTailFadeOut
// ---------------------------------------------------------------------------

TEST_CASE("BufferUtils: applyTailFadeOut", "[bufferutils]")
{
  SECTION("Last sample fades to zero")
  {
    std::vector<float> buf(1000, 1.0f);
    applyTailFadeOut(buf, 200);

    // First 800 samples untouched
    for (int i = 0; i < 800; ++i)
      REQUIRE(buf[i] == 1.0f);

    // Last sample should be near zero
    REQUIRE(buf[999] == Approx(0.0f).margin(0.01f));

    // Mid-fade should be ~0.5
    REQUIRE(buf[900] == Approx(0.5f).margin(0.05f));
  }

  SECTION("fadeSamples = 0 is a no-op")
  {
    std::vector<float> buf = {1.0f, 1.0f, 1.0f};
    auto original = buf;
    applyTailFadeOut(buf, 0);
    REQUIRE(buf == original);
  }

  SECTION("fadeSamples > buffer length clamps")
  {
    std::vector<float> buf(10, 1.0f);
    applyTailFadeOut(buf, 100);

    // Fade is clamped to buf length (10). Last sample gets gain = 1/10 = 0.1
    // (linear fade: gain = 1 - i/fadeLen, where fadeLen=10 and i=9 → 0.1)
    REQUIRE(buf.back() == Approx(0.1f).margin(0.05f));
    // First sample gets gain = 1.0 (i=0)
    REQUIRE(buf.front() == Approx(1.0f).margin(0.01f));
  }
}

// ---------------------------------------------------------------------------
// trimTrailingSilence
// ---------------------------------------------------------------------------

TEST_CASE("BufferUtils: trimTrailingSilence mono", "[bufferutils]")
{
  SECTION("Trims silent tail")
  {
    // 100 loud samples + 500 silent samples
    std::vector<float> buf(600, 0.0f);
    for (int i = 0; i < 100; ++i)
      buf[i] = 0.5f;

    trimTrailingSilence(buf, 0.01f, 32);

    // Should be 100 loud + 32 margin = 132
    REQUIRE(buf.size() == 132);
  }

  SECTION("All-silent buffer keeps at least 1 sample")
  {
    std::vector<float> buf(1000, 0.0f);
    trimTrailingSilence(buf, 0.01f);
    REQUIRE(buf.size() == 1);
  }

  SECTION("No silence to trim leaves buffer unchanged")
  {
    std::vector<float> buf = {1.0f, 0.5f, 0.3f, 0.1f};
    trimTrailingSilence(buf, 0.01f, 0);
    REQUIRE(buf.size() == 4);
  }

  SECTION("Empty buffer is a no-op")
  {
    std::vector<float> buf;
    trimTrailingSilence(buf, 0.01f);
    REQUIRE(buf.empty());
  }
}

TEST_CASE("BufferUtils: trimTrailingSilenceStereo equalizes lengths", "[bufferutils]")
{
  // Left is longer than right — function should equalize before trimming
  std::vector<float> left(200, 0.0f);
  std::vector<float> right(100, 0.0f);

  // Put a loud sample at position 50 in left channel only
  left[50] = 1.0f;

  trimTrailingSilenceStereo(left, right, 0.01f, 16);

  // Both should be trimmed to 50 + 1 + 16 = 67
  REQUIRE(left.size() == 67);
  REQUIRE(left.size() == right.size());
}
