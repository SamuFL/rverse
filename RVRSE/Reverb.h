#pragma once

/// @file Reverb.h
/// @brief Schroeder/Moorer-style algorithmic reverb for the offline pipeline.
///        Parallel comb filters feeding series allpass filters.
///        Offline only — must never be called from the audio thread.

#include "Constants.h"
#include "ReverbEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace rvrse {

struct SchroederReverbTuning
{
  float mMinRoomFactor = kReverbMinRoomFactor;
  float mMaxRoomFactor = kReverbMaxRoomFactor;
  float mMinFeedback = kReverbMinFeedback;
  float mMaxFeedback = kReverbMaxFeedback;
  float mMinDamping = kReverbMinDamping;
  float mMaxDamping = kReverbMaxDamping;
  float mAllpassGain = kReverbAllpassGain;
  float mInputGain = 1.0f;
  bool mScaleDelayByRoomFactor = true;
  int mStereoSpreadSamplesAt44100 = 0;
  std::array<int, kNumCombs> mCombTuningSamplesAt44100 = {
    1310, 1636, 1813, 1927, 1398, 1618, 1768, 1971
  };
  std::array<int, kNumAllpasses> mAllpassTuningSamplesAt44100 = {
    221, 75, 146, 49
  };
};

constexpr SchroederReverbTuning kDefaultSchroederReverbTuning {};

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
  void init(double sampleRate, float roomSize,
            const SchroederReverbTuning& tuning = kDefaultSchroederReverbTuning)
  {
    const float sampleRateF = static_cast<float>(sampleRate);
    const float roomFactor = tuning.mMinRoomFactor + roomSize * (tuning.mMaxRoomFactor - tuning.mMinRoomFactor);
    const float feedback = tuning.mMinFeedback + roomSize * (tuning.mMaxFeedback - tuning.mMinFeedback);
    const float damping = tuning.mMinDamping + roomSize * (tuning.mMaxDamping - tuning.mMinDamping);
    const float delayScale = (sampleRateF / 44100.0f) * (tuning.mScaleDelayByRoomFactor ? roomFactor : 1.0f);

    auto scaledDelaySamples = [delayScale](int baseSamplesAt44100) -> int {
      return std::max(1, static_cast<int>(std::lround(baseSamplesAt44100 * delayScale)));
    };

    for (int i = 0; i < kNumCombs; ++i)
    {
      mCombs[i].init(scaledDelaySamples(tuning.mCombTuningSamplesAt44100[static_cast<size_t>(i)]),
                     feedback, damping);
    }

    for (int i = 0; i < kNumAllpasses; ++i)
    {
      mAllpasses[i].init(scaledDelaySamples(tuning.mAllpassTuningSamplesAt44100[static_cast<size_t>(i)]),
                         tuning.mAllpassGain);
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
/// @param lushAmount   Reverb amount (0.0–1.0): controls room size and blend.
///                     At 0.0 the output is fully dry; at 1.0 it keeps 50% dry
///                     while adding 100% wet at max room size.
///
/// @note This function allocates internally (for delay lines). Offline use only.
inline void applyReverb(const float* in, float* out, size_t numSamples,
                        double sampleRate, float lushAmount,
                        const SchroederReverbTuning& tuning = kDefaultSchroederReverbTuning)
{
  lushAmount = ClampLush(lushAmount);

  if (lushAmount <= 0.0f || numSamples == 0)
  {
    // Fully dry — just copy
    if (out != in)
      std::memcpy(out, in, numSamples * sizeof(float));
    return;
  }

  // Room size scales with lush; blend keeps some direct anchor even at max lush.
  const float roomSize = lushAmount;
  const float wetGain = GetReverbWetGain(lushAmount);
  const float dryGain = GetReverbDryGain(lushAmount);
  const float inputGain = tuning.mInputGain;

  SchroederReverb reverb;
  reverb.init(sampleRate, roomSize, tuning);

  for (size_t i = 0; i < numSamples; ++i)
  {
    const float dry = in[i];
    const float wet = reverb.processSample(dry * inputGain);
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
                              double sampleRate, float lushAmount,
                              const SchroederReverbTuning& tuning = kDefaultSchroederReverbTuning)
{
  lushAmount = ClampLush(lushAmount);

  if (lushAmount <= 0.0f || numSamples == 0)
  {
    if (outL != inL)
      std::memcpy(outL, inL, numSamples * sizeof(float));
    if (outR != inR)
      std::memcpy(outR, inR, numSamples * sizeof(float));
    return;
  }

  const float roomSize = lushAmount;
  const float wetGain = GetReverbWetGain(lushAmount);
  const float dryGain = GetReverbDryGain(lushAmount);
  const float inputGain = tuning.mInputGain;
  const int stereoSpread = tuning.mStereoSpreadSamplesAt44100;

  // Two independent reverb engines for stereo width
  SchroederReverb reverbL;
  SchroederReverb reverbR;
  reverbL.init(sampleRate, roomSize, tuning);
  if (stereoSpread == 0)
  {
    reverbR.init(sampleRate, roomSize, tuning);
  }
  else
  {
    SchroederReverbTuning rightTuning = tuning;
    for (int i = 0; i < kNumCombs; ++i)
      rightTuning.mCombTuningSamplesAt44100[static_cast<size_t>(i)] += stereoSpread;
    for (int i = 0; i < kNumAllpasses; ++i)
      rightTuning.mAllpassTuningSamplesAt44100[static_cast<size_t>(i)] += stereoSpread;
    rightTuning.mStereoSpreadSamplesAt44100 = 0;
    reverbR.init(sampleRate, roomSize, rightTuning);
  }

  for (size_t i = 0; i < numSamples; ++i)
  {
    const float dryL = inL[i];
    const float dryR = inR[i];
    const float wetL = reverbL.processSample(dryL * inputGain);
    const float wetR = reverbR.processSample(dryR * inputGain);
    outL[i] = dryL * dryGain + wetL * wetGain;
    outR[i] = dryR * dryGain + wetR * wetGain;
  }
}

} // namespace rvrse
