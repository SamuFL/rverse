#pragma once

/// @file TimeStretch.h
/// @brief Spectral time-stretcher for the offline pipeline.
///        Uses signalsmith-stretch (MIT, spectral, polyphonic-aware) for
///        high-quality stretching without pitch change.
///        Offline use only — never called from the audio thread.

#include "Constants.h"

#include <signalsmith-stretch/signalsmith-stretch.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace rvrse {

/// Apply spectral time-stretching to a mono buffer.
///
/// @param input          Source buffer
/// @param stretchFactor  Ratio of output length to input length.
///                       > 1.0 = slower/longer, < 1.0 = faster/shorter.
///                       Must be > 0.
/// @param sampleRate     Sample rate in Hz (needed for signalsmith configuration).
///                       Defaults to 44100 for backward compatibility.
/// @return  Stretched output buffer
///
/// @note Allocates the output buffer internally. Offline use only.
inline std::vector<float> stretchBuffer(const std::vector<float>& input,
                                        double stretchFactor,
                                        double sampleRate = 44100.0)
{
  if (input.empty() || stretchFactor <= 0.0)
    return {};

  const int inputLen = static_cast<int>(input.size());
  const int outputLen = std::max(1, static_cast<int>(std::round(inputLen * stretchFactor)));

  // No stretching needed — just return a copy
  if (std::abs(stretchFactor - 1.0) < 1e-6)
    return input;

  // Configure signalsmith-stretch for 1 channel (mono)
  signalsmith::stretch::SignalsmithStretch<float> stretch;
  stretch.presetDefault(1, static_cast<float>(sampleRate));
  stretch.reset();

  // Provide input latency worth of initial data via seek so the stretcher
  // is primed and the output aligns correctly with the input start.
  const int inLatency = stretch.inputLatency();
  const int outLatency = stretch.outputLatency();

  // Build channel pointers for the API (it expects float*[channels])
  // Feed full input + inputLatency silence for flush
  const int totalInput = inputLen + inLatency;
  const int totalOutput = outputLen + outLatency;

  std::vector<float> paddedInput(static_cast<size_t>(totalInput), 0.0f);
  std::copy(input.begin(), input.end(), paddedInput.begin());

  std::vector<float> rawOutput(static_cast<size_t>(totalOutput), 0.0f);

  // Process in one shot — signalsmith handles the stretch ratio via
  // the input/output length ratio
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

/// Apply spectral time-stretching to stereo buffers (deinterleaved L/R).
///
/// @param inputL / inputR   Source channel buffers
/// @param stretchFactor     Ratio of output to input length (> 0)
/// @param[out] outputL      Stretched left channel
/// @param[out] outputR      Stretched right channel
/// @param sampleRate        Sample rate in Hz
///
/// @note Uses a single stereo stretcher instance for phase-coherent output.
///       Offline use only.
inline void stretchBufferStereo(const std::vector<float>& inputL,
                                const std::vector<float>& inputR,
                                double stretchFactor,
                                std::vector<float>& outputL,
                                std::vector<float>& outputR,
                                double sampleRate = 44100.0)
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

  // Configure for 2 channels (stereo) — single instance for phase coherence
  signalsmith::stretch::SignalsmithStretch<float> stretch;
  stretch.presetDefault(2, static_cast<float>(sampleRate));
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
