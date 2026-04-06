#pragma once

/// @file TimeStretch.h
/// @brief Spectral time-stretcher for the offline pipeline.
///        Uses signalsmith-stretch (MIT, spectral, polyphonic-aware) for
///        high-quality stretching without pitch change.
///        Offline use only — never called from the audio thread.
///
///        Implementation lives in TimeStretch.cpp, compiled with -O2 in
///        non-MSVC Debug builds — spectral FFT code is orders of magnitude slower at -O0.

#include "Constants.h"

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
/// @param quality        Stretcher quality preset (High or Low).
/// @return  Stretched output buffer
///
/// @note Allocates the output buffer internally. Offline use only.
std::vector<float> stretchBuffer(const std::vector<float>& input,
                                 double stretchFactor,
                                 double sampleRate = 44100.0,
                                 EStretchQuality quality = kStretchQualityHigh);

/// Apply spectral time-stretching to stereo buffers (deinterleaved L/R).
///
/// @param inputL / inputR   Source channel buffers
/// @param stretchFactor     Ratio of output to input length (> 0)
/// @param[out] outputL      Stretched left channel
/// @param[out] outputR      Stretched right channel
/// @param sampleRate        Sample rate in Hz
/// @param quality           Stretcher quality preset (High or Low).
///
/// @note Uses a single stereo stretcher instance for phase-coherent output.
///       Offline use only.
void stretchBufferStereo(const std::vector<float>& inputL,
                         const std::vector<float>& inputR,
                         double stretchFactor,
                         std::vector<float>& outputL,
                         std::vector<float>& outputR,
                         double sampleRate = 44100.0,
                         EStretchQuality quality = kStretchQualityHigh);

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
