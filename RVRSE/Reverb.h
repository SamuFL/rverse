#pragma once

/// @file Reverb.h
/// @brief Schroeder/Moorer-style algorithmic reverb for the offline pipeline.
///        Parallel comb filters feeding series allpass filters.
///        Offline only — must never be called from the audio thread.

#include "Constants.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace rvrse {

// ---------------------------------------------------------------------------
// Internal helper: simple circular delay buffer
// ---------------------------------------------------------------------------
namespace detail {

class DelayBuffer
{
public:
  void resize(int maxDelay)
  {
    mBuffer.assign(static_cast<size_t>(maxDelay), 0.0f);
    mWritePos = 0;
  }

  void clear()
  {
    std::fill(mBuffer.begin(), mBuffer.end(), 0.0f);
    mWritePos = 0;
  }

  /// Write one sample and advance the write pointer.
  void write(float sample)
  {
    mBuffer[static_cast<size_t>(mWritePos)] = sample;
    mWritePos = (mWritePos + 1) % static_cast<int>(mBuffer.size());
  }

  /// Read a sample from `delaySamples` ago.
  float read(int delaySamples) const
  {
    int readPos = mWritePos - delaySamples;
    if (readPos < 0) readPos += static_cast<int>(mBuffer.size());
    return mBuffer[static_cast<size_t>(readPos)];
  }

private:
  std::vector<float> mBuffer;
  int mWritePos = 0;
};

} // namespace detail

// ---------------------------------------------------------------------------
// Comb filter — feedback comb filter with optional low-pass damping
// ---------------------------------------------------------------------------
class CombFilter
{
public:
  /// @param delaySamples   Delay length in samples
  /// @param feedback       Feedback coefficient (0–1)
  /// @param damping        Low-pass damping coefficient (0–1, 0 = no damping)
  void init(int delaySamples, float feedback, float damping)
  {
    mDelay = delaySamples;
    mFeedback = feedback;
    mDamping = damping;
    mPrevFilterOut = 0.0f;
    mBuf.resize(delaySamples + 1);
    mBuf.clear();
  }

  void clear()
  {
    mBuf.clear();
    mPrevFilterOut = 0.0f;
  }

  float process(float input)
  {
    float delayed = mBuf.read(mDelay);

    // One-pole low-pass on the feedback path (Moorer extension)
    float filtered = delayed * (1.0f - mDamping) + mPrevFilterOut * mDamping;
    mPrevFilterOut = filtered;

    mBuf.write(input + filtered * mFeedback);
    return delayed;
  }

private:
  detail::DelayBuffer mBuf;
  int mDelay = 0;
  float mFeedback = 0.0f;
  float mDamping = 0.0f;
  float mPrevFilterOut = 0.0f;
};

// ---------------------------------------------------------------------------
// Allpass filter — first-order Schroeder allpass
// ---------------------------------------------------------------------------
class AllpassFilter
{
public:
  /// @param delaySamples   Delay length in samples
  /// @param feedback       Gain coefficient (typically 0.5–0.7)
  void init(int delaySamples, float feedback)
  {
    mDelay = delaySamples;
    mFeedback = feedback;
    mBuf.resize(delaySamples + 1);
    mBuf.clear();
  }

  void clear()
  {
    mBuf.clear();
  }

  float process(float input)
  {
    float delayed = mBuf.read(mDelay);
    float output = -input + delayed;
    mBuf.write(input + delayed * mFeedback);
    return output;
  }

private:
  detail::DelayBuffer mBuf;
  int mDelay = 0;
  float mFeedback = 0.0f;
};

// ---------------------------------------------------------------------------
// Schroeder reverb engine — 8 parallel combs → 4 series allpasses
// ---------------------------------------------------------------------------
class SchroederReverb
{
public:
  /// Configure the reverb for a given sample rate and room size.
  /// @param sampleRate  Sample rate in Hz
  /// @param roomSize    Room size factor (0.0–1.0), maps from Lush knob
  void init(double sampleRate, float roomSize)
  {
    // Scale delay times by room size and sample rate.
    // Base delay times in ms (classic Schroeder values, slightly detuned to reduce ringing):
    //   Combs:    29.7, 37.1, 41.1, 43.7, 31.7, 36.7, 40.1, 44.7  ms
    //   Allpasses: 5.0,  1.7,  3.3,  1.1  ms

    const float sampleRateF = static_cast<float>(sampleRate);

    // Room size scales the delay times: small room = shorter delays, large room = longer
    const float roomFactor = kReverbMinRoomFactor + roomSize * (kReverbMaxRoomFactor - kReverbMinRoomFactor);

    // Feedback coefficient — longer decay for larger rooms
    const float feedback = kReverbMinFeedback + roomSize * (kReverbMaxFeedback - kReverbMinFeedback);

    // Damping — slightly more damping in larger rooms (warmer sound)
    const float damping = kReverbMinDamping + roomSize * (kReverbMaxDamping - kReverbMinDamping);

    auto msToSamples = [sampleRateF, roomFactor](float ms) -> int {
      return std::max(1, static_cast<int>(ms * roomFactor * sampleRateF / 1000.0f));
    };

    // Initialise 8 comb filters with detuned delay times
    static constexpr float kCombDelaysMs[kNumCombs] = {
      29.7f, 37.1f, 41.1f, 43.7f, 31.7f, 36.7f, 40.1f, 44.7f
    };

    for (int i = 0; i < kNumCombs; ++i)
    {
      mCombs[i].init(msToSamples(kCombDelaysMs[i]), feedback, damping);
    }

    // Initialise 4 allpass filters in series
    static constexpr float kAllpassDelaysMs[kNumAllpasses] = {
      5.0f, 1.7f, 3.3f, 1.1f
    };

    for (int i = 0; i < kNumAllpasses; ++i)
    {
      mAllpasses[i].init(msToSamples(kAllpassDelaysMs[i]), kReverbAllpassGain);
    }
  }

  /// Reset all internal state (clear delay lines)
  void clear()
  {
    for (auto& c : mCombs) c.clear();
    for (auto& a : mAllpasses) a.clear();
  }

  /// Process a single sample through the reverb.
  /// @return The wet (reverb-only) output
  float processSample(float input)
  {
    // Sum output of all parallel comb filters
    float combSum = 0.0f;
    for (auto& c : mCombs)
    {
      combSum += c.process(input);
    }

    // Normalise by number of combs to prevent clipping
    combSum /= static_cast<float>(kNumCombs);

    // Feed through series allpass filters
    float out = combSum;
    for (auto& a : mAllpasses)
    {
      out = a.process(out);
    }

    return out;
  }

private:
  CombFilter mCombs[kNumCombs];
  AllpassFilter mAllpasses[kNumAllpasses];
};

// ---------------------------------------------------------------------------
// Public API — Stateless reverb function for the offline pipeline
// ---------------------------------------------------------------------------

/// Apply Schroeder reverb to a mono buffer.
///
/// @param in           Input buffer (numSamples floats)
/// @param out          Output buffer (numSamples floats) — may alias `in`
/// @param numSamples   Number of samples to process
/// @param sampleRate   Sample rate of the audio data
/// @param lushAmount   Reverb amount (0.0–1.0): controls room size AND wet/dry mix.
///                     At 0.0 the output is fully dry; at 1.0 fully wet with max room size.
///
/// @note This function allocates internally (for delay lines). Offline use only.
inline void applyReverb(const float* in, float* out, size_t numSamples,
                        double sampleRate, float lushAmount)
{
  lushAmount = std::clamp(lushAmount, 0.0f, 1.0f);

  if (lushAmount <= 0.0f || numSamples == 0)
  {
    // Fully dry — just copy
    if (out != in)
      std::memcpy(out, in, numSamples * sizeof(float));
    return;
  }

  // Room size scales with lush; wet gain scales with lush
  const float roomSize = lushAmount;
  const float wetGain = lushAmount;
  const float dryGain = 1.0f - wetGain;

  SchroederReverb reverb;
  reverb.init(sampleRate, roomSize);

  for (size_t i = 0; i < numSamples; ++i)
  {
    const float dry = in[i];
    const float wet = reverb.processSample(dry);
    out[i] = dry * dryGain + wet * wetGain;
  }
}

/// Apply Schroeder reverb to a stereo buffer (deinterleaved L/R).
///
/// @param inL / inR      Input channel buffers
/// @param outL / outR    Output channel buffers (may alias inputs)
/// @param numSamples     Number of sample frames (per channel)
/// @param sampleRate     Sample rate of the audio data
/// @param lushAmount     Reverb amount (0.0–1.0)
///
/// @note Uses independent reverb instances per channel for true stereo imaging.
///       Offline use only — allocates internally.
inline void applyReverbStereo(const float* inL, const float* inR,
                              float* outL, float* outR,
                              size_t numSamples,
                              double sampleRate, float lushAmount)
{
  lushAmount = std::clamp(lushAmount, 0.0f, 1.0f);

  if (lushAmount <= 0.0f || numSamples == 0)
  {
    if (outL != inL)
      std::memcpy(outL, inL, numSamples * sizeof(float));
    if (outR != inR)
      std::memcpy(outR, inR, numSamples * sizeof(float));
    return;
  }

  const float roomSize = lushAmount;
  const float wetGain = lushAmount;
  const float dryGain = 1.0f - wetGain;

  // Two independent reverb engines for stereo width
  SchroederReverb reverbL;
  SchroederReverb reverbR;
  reverbL.init(sampleRate, roomSize);
  reverbR.init(sampleRate, roomSize);

  for (size_t i = 0; i < numSamples; ++i)
  {
    const float dryL = inL[i];
    const float dryR = inR[i];
    const float wetL = reverbL.processSample(dryL);
    const float wetR = reverbR.processSample(dryR);
    outL[i] = dryL * dryGain + wetL * wetGain;
    outR[i] = dryR * dryGain + wetR * wetGain;
  }
}

} // namespace rvrse
