/// @file TimeStretch.cpp
/// @brief Implementation of spectral time-stretching functions.
///
///        This file is compiled with -O2 even in Debug builds because
///        signalsmith-stretch's FFT code is orders of magnitude slower
///        at -O0. See RVRSE/CMakeLists.txt for the per-file flag.

#include "TimeStretch.h"

#include <signalsmith-stretch/signalsmith-stretch.h>

#include <algorithm>
#include <cmath>

namespace {

/// Configure the stretcher with the appropriate quality preset.
void applyQualityPreset(signalsmith::stretch::SignalsmithStretch<float>& stretch,
                        int channels, float sampleRate, rvrse::EStretchQuality quality)
{
  if (quality == rvrse::kStretchQualityLow)
    stretch.presetCheaper(channels, sampleRate);
  else
    stretch.presetDefault(channels, sampleRate);
}

} // anonymous namespace

namespace rvrse {

std::vector<float> stretchBuffer(const std::vector<float>& input,
                                 double stretchFactor,
                                 double sampleRate,
                                 EStretchQuality quality)
{
  if (input.empty() || stretchFactor <= 0.0)
    return {};

  const int inputLen = static_cast<int>(input.size());
  const int outputLen = std::max(1, static_cast<int>(std::round(inputLen * stretchFactor)));

  // No stretching needed — just return a copy
  if (std::abs(stretchFactor - 1.0) < 1e-6)
    return input;

  // Configure signalsmith-stretch for 1 channel (mono).
  signalsmith::stretch::SignalsmithStretch<float> stretch;
  applyQualityPreset(stretch, 1, static_cast<float>(sampleRate), quality);
  stretch.reset();

  const int inLatency = stretch.inputLatency();
  const int outLatency = stretch.outputLatency();

  // Feed full input + inputLatency silence for flush
  const int totalInput = inputLen + inLatency;
  const int totalOutput = outputLen + outLatency;

  std::vector<float> paddedInput(static_cast<size_t>(totalInput), 0.0f);
  std::copy(input.begin(), input.end(), paddedInput.begin());

  std::vector<float> rawOutput(static_cast<size_t>(totalOutput), 0.0f);

  const float* inPtr = paddedInput.data();
  float* outPtr = rawOutput.data();
  stretch.process(&inPtr, totalInput, &outPtr, totalOutput);

  // Trim output latency from the front and excess from the back
  std::vector<float> output(static_cast<size_t>(outputLen), 0.0f);
  const int copyStart = outLatency;
  const int copyLen = std::min(outputLen, totalOutput - copyStart);
  if (copyLen > 0)
  {
    std::copy(rawOutput.begin() + copyStart,
              rawOutput.begin() + copyStart + copyLen,
              output.begin());
  }

  return output;
}

void stretchBufferStereo(const std::vector<float>& inputL,
                         const std::vector<float>& inputR,
                         double stretchFactor,
                         std::vector<float>& outputL,
                         std::vector<float>& outputR,
                         double sampleRate,
                         EStretchQuality quality)
{
  if (inputL.empty() || stretchFactor <= 0.0)
  {
    outputL.clear();
    outputR.clear();
    return;
  }

  // No stretching needed — just copy
  if (std::abs(stretchFactor - 1.0) < 1e-6)
  {
    outputL = inputL;
    outputR = inputR;
    return;
  }

  const int inputLen = static_cast<int>(inputL.size());
  const int outputLen = std::max(1, static_cast<int>(std::round(inputLen * stretchFactor)));

  // Configure for 2 channels (stereo) — single instance for phase coherence.
  signalsmith::stretch::SignalsmithStretch<float> stretch;
  applyQualityPreset(stretch, 2, static_cast<float>(sampleRate), quality);
  stretch.reset();

  const int inLatency = stretch.inputLatency();
  const int outLatency = stretch.outputLatency();

  const int totalInput = inputLen + inLatency;
  const int totalOutput = outputLen + outLatency;

  // Pad inputs with silence for flush
  std::vector<float> paddedL(static_cast<size_t>(totalInput), 0.0f);
  std::vector<float> paddedR(static_cast<size_t>(totalInput), 0.0f);
  std::copy(inputL.begin(), inputL.end(), paddedL.begin());
  std::copy(inputR.begin(), inputR.end(), paddedR.begin());

  std::vector<float> rawOutL(static_cast<size_t>(totalOutput), 0.0f);
  std::vector<float> rawOutR(static_cast<size_t>(totalOutput), 0.0f);

  const float* inPtrs[2] = { paddedL.data(), paddedR.data() };
  float* outPtrs[2] = { rawOutL.data(), rawOutR.data() };
  stretch.process(inPtrs, totalInput, outPtrs, totalOutput);

  // Trim output latency from the front
  outputL.resize(static_cast<size_t>(outputLen), 0.0f);
  outputR.resize(static_cast<size_t>(outputLen), 0.0f);

  const int copyStart = outLatency;
  const int copyLen = std::min(outputLen, totalOutput - copyStart);
  if (copyLen > 0)
  {
    std::copy(rawOutL.begin() + copyStart, rawOutL.begin() + copyStart + copyLen, outputL.begin());
    std::copy(rawOutR.begin() + copyStart, rawOutR.begin() + copyStart + copyLen, outputR.begin());
  }
}

} // namespace rvrse
