/// @file test_helpers.h
/// @brief Shared utilities for RVRSE unit tests.

#pragma once

#include "Constants.h"

#include <cmath>
#include <vector>

namespace rvrse::test {

/// Generate a mono sine wave buffer.
/// @param numFrames   Number of samples to generate
/// @param freqHz      Frequency in Hz (e.g. 440.0)
/// @param sampleRate  Sample rate in Hz (e.g. 48000.0)
/// @param amplitude   Peak amplitude (default 1.0)
/// @return Buffer of float samples
inline std::vector<float> generateSine(size_t numFrames, double freqHz,
                                       double sampleRate, float amplitude = 1.0f)
{
  std::vector<float> buf(numFrames);
  const double phaseInc = kTwoPi * freqHz / sampleRate;
  for (size_t i = 0; i < numFrames; ++i)
    buf[i] = amplitude * static_cast<float>(std::sin(phaseInc * static_cast<double>(i)));
  return buf;
}

} // namespace rvrse::test
