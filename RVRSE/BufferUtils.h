#pragma once

/// @file BufferUtils.h
/// @brief Stateless buffer manipulation utilities for the offline pipeline.
///        Never called from the audio thread.

#include <algorithm>
#include <cmath>
#include <vector>

namespace rvrse {

/// Reverse a mono buffer in-place.
/// @param buf  Buffer to reverse
inline void reverseBuffer(std::vector<float>& buf)
{
  std::reverse(buf.begin(), buf.end());
}

/// Reverse stereo buffers in-place (deinterleaved L/R).
/// Both channels are reversed independently so stereo imaging is preserved.
/// @param left   Left channel buffer
/// @param right  Right channel buffer
inline void reverseBufferStereo(std::vector<float>& left, std::vector<float>& right)
{
  std::reverse(left.begin(), left.end());
  std::reverse(right.begin(), right.end());
}

// ---------------------------------------------------------------------------
// Linear resampling
// ---------------------------------------------------------------------------

/// Resample a mono buffer from one sample rate to another using linear interpolation.
/// @param input       Source buffer at sourceSR
/// @param sourceSR    Source sample rate (e.g. 96000)
/// @param targetSR    Target sample rate (e.g. 48000)
/// @return Resampled buffer at targetSR
inline std::vector<float> resampleLinear(const std::vector<float>& input,
                                         double sourceSR, double targetSR)
{
  if (input.empty() || sourceSR <= 0.0 || targetSR <= 0.0)
    return input;

  if (std::abs(sourceSR - targetSR) < 0.5)
    return input; // Same rate — no resampling needed

  const double ratio = sourceSR / targetSR;
  const int outputLen = std::max(1, static_cast<int>(std::round(input.size() / ratio)));

  std::vector<float> output(static_cast<size_t>(outputLen));

  for (int i = 0; i < outputLen; ++i)
  {
    const double srcPos = static_cast<double>(i) * ratio;
    const int idx0 = static_cast<int>(srcPos);
    const float frac = static_cast<float>(srcPos - idx0);

    const int idx1 = std::min(idx0 + 1, static_cast<int>(input.size()) - 1);

    if (idx0 >= static_cast<int>(input.size()))
    {
      output[static_cast<size_t>(i)] = 0.0f;
    }
    else
    {
      output[static_cast<size_t>(i)] = input[static_cast<size_t>(idx0)] * (1.0f - frac)
                                     + input[static_cast<size_t>(idx1)] * frac;
    }
  }

  return output;
}

/// Resample stereo buffers from one sample rate to another.
/// @param inL / inR     Source channel buffers at sourceSR
/// @param sourceSR      Source sample rate
/// @param targetSR      Target sample rate
/// @param[out] outL / outR  Resampled channel buffers at targetSR
inline void resampleLinearStereo(const std::vector<float>& inL,
                                 const std::vector<float>& inR,
                                 double sourceSR, double targetSR,
                                 std::vector<float>& outL,
                                 std::vector<float>& outR)
{
  outL = resampleLinear(inL, sourceSR, targetSR);
  outR = resampleLinear(inR, sourceSR, targetSR);
}

// ---------------------------------------------------------------------------
// Fade utilities
// ---------------------------------------------------------------------------

/// Apply a linear fade-out to the last N samples of a mono buffer.
/// @param buf            Buffer to fade (modified in-place)
/// @param fadeSamples    Number of samples over which to fade to zero
inline void applyTailFadeOut(std::vector<float>& buf, int fadeSamples)
{
  if (buf.empty() || fadeSamples <= 0) return;

  const int len = static_cast<int>(buf.size());
  const int fadeLen = std::min(fadeSamples, len);
  const int fadeStart = len - fadeLen;

  for (int i = 0; i < fadeLen; ++i)
  {
    const float gain = 1.0f - static_cast<float>(i) / static_cast<float>(fadeLen);
    buf[static_cast<size_t>(fadeStart + i)] *= gain;
  }
}

/// Apply a linear fade-out to the last N samples of stereo buffers.
/// @param left / right   Buffers to fade (modified in-place)
/// @param fadeSamples    Number of samples over which to fade to zero
inline void applyTailFadeOutStereo(std::vector<float>& left,
                                   std::vector<float>& right,
                                   int fadeSamples)
{
  applyTailFadeOut(left, fadeSamples);
  applyTailFadeOut(right, fadeSamples);
}

/// Apply a linear fade-in to the first N samples of a mono buffer.
inline void applyHeadFadeIn(std::vector<float>& buf, int fadeSamples)
{
  if (buf.empty() || fadeSamples <= 0) return;

  const int fadeLen = std::min(fadeSamples, static_cast<int>(buf.size()));
  if (fadeLen == 1)
  {
    buf[0] = 0.0f;
    return;
  }

  for (int i = 0; i < fadeLen; ++i)
  {
    const float gain = static_cast<float>(i) / static_cast<float>(fadeLen - 1);
    buf[static_cast<size_t>(i)] *= gain;
  }
}

/// Apply a linear fade-in to the first N samples of stereo buffers.
inline void applyHeadFadeInStereo(std::vector<float>& left,
                                  std::vector<float>& right,
                                  int fadeSamples)
{
  applyHeadFadeIn(left, fadeSamples);
  applyHeadFadeIn(right, fadeSamples);
}

/// Apply a short fade-in and fade-out around a region inside a mono buffer.
/// The fade is applied at the region start and region end, leaving the rest untouched.
inline void applyRegionEdgeFade(std::vector<float>& buf,
                                int startFrame,
                                int endFrameExclusive,
                                int fadeSamples)
{
  if (buf.empty() || fadeSamples <= 0)
    return;

  startFrame = std::clamp(startFrame, 0, static_cast<int>(buf.size()));
  endFrameExclusive = std::clamp(endFrameExclusive, startFrame, static_cast<int>(buf.size()));
  const int regionLen = endFrameExclusive - startFrame;
  const int fadeLen = std::min(fadeSamples, regionLen / 2);
  if (fadeLen <= 0)
    return;

  if (fadeLen == 1)
  {
    buf[static_cast<size_t>(startFrame)] = 0.0f;
    buf[static_cast<size_t>(endFrameExclusive - 1)] = 0.0f;
    return;
  }

  for (int i = 0; i < fadeLen; ++i)
  {
    const float denom = static_cast<float>(fadeLen - 1);
    const float inGain = static_cast<float>(i) / denom;
    const float outGain = static_cast<float>(fadeLen - i - 1) / denom;
    buf[static_cast<size_t>(startFrame + i)] *= inGain;
    buf[static_cast<size_t>(endFrameExclusive - fadeLen + i)] *= outGain;
  }
}

/// Apply a short fade-in and fade-out around a region inside stereo buffers.
inline void applyRegionEdgeFadeStereo(std::vector<float>& left,
                                      std::vector<float>& right,
                                      int startFrame,
                                      int endFrameExclusive,
                                      int fadeSamples)
{
  applyRegionEdgeFade(left, startFrame, endFrameExclusive, fadeSamples);
  applyRegionEdgeFade(right, startFrame, endFrameExclusive, fadeSamples);
}

// ---------------------------------------------------------------------------
// Silence trimming
// ---------------------------------------------------------------------------

/// Trim trailing near-silent samples from a mono buffer.
/// Scans from the end and removes samples whose absolute value is below
/// `threshold`. A small margin of `marginSamples` is kept after the last
/// above-threshold sample to avoid cutting into the natural decay.
///
/// @param buf             Buffer to trim (modified in-place)
/// @param threshold       Amplitude below which samples count as silent
/// @param marginSamples   Extra samples to keep beyond the last loud sample
inline void trimTrailingSilence(std::vector<float>& buf, float threshold,
                                int marginSamples = 64)
{
  if (buf.empty()) return;

  int lastLoud = -1;
  for (int i = static_cast<int>(buf.size()) - 1; i >= 0; --i)
  {
    if (std::abs(buf[static_cast<size_t>(i)]) >= threshold)
    {
      lastLoud = i;
      break;
    }
  }

  if (lastLoud < 0)
  {
    // Entire buffer is silent — keep at least 1 sample
    buf.resize(1);
    return;
  }

  const int newSize = std::min(static_cast<int>(buf.size()),
                               lastLoud + 1 + marginSamples);
  buf.resize(static_cast<size_t>(newSize));
}

/// Trim trailing near-silent samples from stereo buffers.
/// Uses the maximum of both channels at each frame to decide the trim point.
///
/// @param left / right    Buffers to trim (modified in-place, both resized equally)
/// @param threshold       Amplitude below which samples count as silent
/// @param marginSamples   Extra samples to keep beyond the last loud sample
inline void trimTrailingSilenceStereo(std::vector<float>& left,
                                      std::vector<float>& right,
                                      float threshold,
                                      int marginSamples = 64)
{
  if (left.empty() && right.empty()) return;

  // Ensure both channels are the same length (pad shorter with silence)
  if (left.size() != right.size())
  {
    const size_t maxLen = std::max(left.size(), right.size());
    left.resize(maxLen, 0.0f);
    right.resize(maxLen, 0.0f);
  }

  const int len = static_cast<int>(left.size());
  int lastLoud = -1;

  for (int i = len - 1; i >= 0; --i)
  {
    const float absL = std::abs(left[static_cast<size_t>(i)]);
    const float absR = std::abs(right[static_cast<size_t>(i)]);

    if (std::max(absL, absR) >= threshold)
    {
      lastLoud = i;
      break;
    }
  }

  if (lastLoud < 0)
  {
    left.resize(1);
    right.resize(1);
    return;
  }

  const int newSize = std::min(len, lastLoud + 1 + marginSamples);
  left.resize(static_cast<size_t>(newSize));
  right.resize(static_cast<size_t>(newSize));
}

/// Detect the first sustained, meaningful onset in stereo material using a
/// smoothed envelope, relative threshold, and minimum hold time.
inline size_t findSustainedEnvelopeOnsetStereo(const std::vector<float>& left,
                                               const std::vector<float>& right,
                                               float peakFraction,
                                               float absoluteFloor,
                                               int smoothingFrames,
                                               int holdFrames,
                                               int maxSearchFrames)
{
  const size_t len = std::min(left.size(), right.size());
  if (len == 0) return 0;

  const size_t searchLen = std::min(len, static_cast<size_t>(std::max(1, maxSearchFrames)));
  const int smoothing = std::max(1, smoothingFrames);
  const int hold = std::max(1, holdFrames);
  const size_t required = static_cast<size_t>(std::max(smoothing, hold));
  if (searchLen < required) return 0;

  std::vector<float> envelope(searchLen, 0.0f);
  float runningSum = 0.0f;
  for (size_t i = 0; i < searchLen; ++i)
  {
    const float magnitude = std::max(std::abs(left[i]), std::abs(right[i]));
    runningSum += magnitude;
    if (i >= static_cast<size_t>(smoothing))
      runningSum -= envelope[i - static_cast<size_t>(smoothing)];
    envelope[i] = magnitude;
  }

  std::vector<float> smoothed(searchLen, 0.0f);
  runningSum = 0.0f;
  float peakEnvelope = 0.0f;
  for (size_t i = 0; i < searchLen; ++i)
  {
    runningSum += envelope[i];
    if (i >= static_cast<size_t>(smoothing))
      runningSum -= envelope[i - static_cast<size_t>(smoothing)];
    const int window = static_cast<int>(std::min(i + 1, static_cast<size_t>(smoothing)));
    smoothed[i] = runningSum / static_cast<float>(window);
    peakEnvelope = std::max(peakEnvelope, smoothed[i]);
  }

  const float threshold = std::max(absoluteFloor, peakEnvelope * peakFraction);
  if (peakEnvelope < threshold)
    return 0;

  for (size_t start = 0; start + static_cast<size_t>(hold) <= searchLen; ++start)
  {
    bool sustained = true;
    for (size_t i = 0; i < static_cast<size_t>(hold); ++i)
    {
      if (smoothed[start + i] < threshold)
      {
        sustained = false;
        break;
      }
    }

    if (sustained)
      return start;
  }

  return 0;
}

/// Shift a stereo buffer left by the given onset and zero-pad the end. Returns
/// the number of frames removed from the front.
inline size_t shiftBufferLeftStereo(std::vector<float>& left,
                                    std::vector<float>& right,
                                    size_t frames)
{
  if (left.size() != right.size())
  {
    const size_t maxLen = std::max(left.size(), right.size());
    left.resize(maxLen, 0.0f);
    right.resize(maxLen, 0.0f);
  }

  const size_t len = left.size();
  if (len == 0 || frames == 0)
    return 0;

  const size_t shift = std::min(frames, len - 1);
  if (shift == 0)
    return 0;

  for (size_t i = 0; i + shift < len; ++i)
  {
    left[i] = left[i + shift];
    right[i] = right[i + shift];
  }

  for (size_t i = len - shift; i < len; ++i)
  {
    left[i] = 0.0f;
    right[i] = 0.0f;
  }

  return shift;
}

} // namespace rvrse
