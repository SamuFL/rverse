#pragma once

/// @file TimeStretch.h
/// @brief Overlap-Add (OLA) time-stretcher for the offline pipeline.
///        Stretches or compresses a buffer without changing pitch.
///        Quality is acceptable for MVP — not phase-vocoder grade.
///        Offline use only — never called from the audio thread.

#include "Constants.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace rvrse {

/// Apply Overlap-Add time-stretching to a mono buffer.
///
/// @param input          Source buffer
/// @param stretchFactor  Ratio of output length to input length.
///                       > 1.0 = slower/longer, < 1.0 = faster/shorter.
///                       Must be > 0.
/// @return  Stretched output buffer
///
/// @note Allocates the output buffer internally. Offline use only.
inline std::vector<float> stretchBuffer(const std::vector<float>& input,
                                        double stretchFactor)
{
  if (input.empty() || stretchFactor <= 0.0)
    return {};

  const int inputLen = static_cast<int>(input.size());
  const int outputLen = std::max(1, static_cast<int>(std::round(inputLen * stretchFactor)));

  // No stretching needed — just return a copy
  if (std::abs(stretchFactor - 1.0) < 1e-6)
    return input;

  // OLA parameters
  const int windowSize = kOlaWindowSize;
  const int hopOut = windowSize / 2; // 50% overlap in output

  // Hop through the input at a rate determined by the stretch factor.
  // hopIn = hopOut / stretchFactor  (how far we advance in the input per output hop)
  const double hopIn = static_cast<double>(hopOut) / stretchFactor;

  // Pre-compute a Hann window for smooth overlapping
  std::vector<float> window(static_cast<size_t>(windowSize));
  for (int i = 0; i < windowSize; ++i)
  {
    const double phase = static_cast<double>(i) / static_cast<double>(windowSize - 1);
    window[static_cast<size_t>(i)] = static_cast<float>(0.5 * (1.0 - std::cos(2.0 * kPi * phase)));
  }

  // Output buffer — zero-initialised for additive overlap
  std::vector<float> output(static_cast<size_t>(outputLen), 0.0f);

  // Normalisation buffer to track the accumulated window energy at each output sample
  std::vector<float> normBuf(static_cast<size_t>(outputLen), 0.0f);

  double inputPos = 0.0;   // Floating-point read position in input
  int outputPos = 0;        // Integer write position in output

  while (outputPos < outputLen)
  {
    const int readStart = static_cast<int>(std::round(inputPos));

    for (int i = 0; i < windowSize; ++i)
    {
      const int outIdx = outputPos + i;
      if (outIdx >= outputLen) break;

      // Read from input with boundary clamping
      int inIdx = readStart + i;
      float sample = 0.0f;
      if (inIdx >= 0 && inIdx < inputLen)
        sample = input[static_cast<size_t>(inIdx)];

      const float w = window[static_cast<size_t>(i)];
      output[static_cast<size_t>(outIdx)] += sample * w;
      normBuf[static_cast<size_t>(outIdx)] += w;
    }

    inputPos += hopIn;
    outputPos += hopOut;
  }

  // Normalise by accumulated window energy to prevent amplitude changes
  for (int i = 0; i < outputLen; ++i)
  {
    if (normBuf[static_cast<size_t>(i)] > 1e-6f)
      output[static_cast<size_t>(i)] /= normBuf[static_cast<size_t>(i)];
  }

  return output;
}

/// Apply Overlap-Add time-stretching to stereo buffers (deinterleaved L/R).
///
/// @param inputL / inputR   Source channel buffers
/// @param stretchFactor     Ratio of output to input length (> 0)
/// @param[out] outputL      Stretched left channel
/// @param[out] outputR      Stretched right channel
///
/// @note Offline use only.
inline void stretchBufferStereo(const std::vector<float>& inputL,
                                const std::vector<float>& inputR,
                                double stretchFactor,
                                std::vector<float>& outputL,
                                std::vector<float>& outputR)
{
  outputL = stretchBuffer(inputL, stretchFactor);
  outputR = stretchBuffer(inputR, stretchFactor);
}

/// Calculate the stretch factor needed to fit a given number of beats.
///
/// @param bufferFrames     Number of sample frames in the reversed buffer
/// @param riserLengthBeats Desired riser length in musical beats
/// @param bpm              Host tempo in BPM
/// @param sampleRate       Audio sample rate in Hz
/// @return Stretch factor (output length / input length)
inline double calcStretchFactor(int bufferFrames, double riserLengthBeats,
                                double bpm, double sampleRate)
{
  if (bufferFrames <= 0 || riserLengthBeats <= 0.0 || bpm <= 0.0 || sampleRate <= 0.0)
    return 1.0;

  const double samplesPerBeat = (sampleRate * 60.0) / bpm;
  const double targetFrames = riserLengthBeats * samplesPerBeat;
  return targetFrames / static_cast<double>(bufferFrames);
}

} // namespace rvrse
