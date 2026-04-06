#pragma once

/// @file RvrseProcessor.h
/// @brief Offline pipeline orchestrator: reverb → reverse → time-stretch.
///        Runs on a background thread, never on the audio thread.
///        On parameter changes, rebuilds from the earliest changed stage onwards.

#include "BufferUtils.h"
#include "Constants.h"
#include "Reverb.h"
#include "SampleData.h"
#include "TimeStretch.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace rvrse {

/// Result of the offline pipeline — the ready-to-play riser buffer plus
/// intermediate debug buffers for diagnostic playback.
struct RiserData
{
  std::vector<float> mLeft;    ///< Left channel of final_riser[] (stretched + faded)
  std::vector<float> mRight;   ///< Right channel of final_riser[]
  double mSampleRate = 0.0;    ///< Sample rate of the riser data

  // Debug: intermediate pipeline buffers (populated alongside the main output)
  std::vector<float> mReverbedL;  ///< After reverb, before reverse/stretch
  std::vector<float> mReverbedR;
  std::vector<float> mReversedL;  ///< After reverse, before stretch
  std::vector<float> mReversedR;

  int NumFrames() const { return static_cast<int>(mLeft.size()); }
  bool IsReady() const { return !mLeft.empty() && mSampleRate > 0.0; }

  /// @return Number of frames in the reverbed debug buffer
  int ReverbedFrames() const { return static_cast<int>(mReverbedL.size()); }
  /// @return Number of frames in the reversed debug buffer
  int ReversedFrames() const { return static_cast<int>(mReversedL.size()); }
};

/// The offline pipeline orchestrator.
///
/// Usage:
///   1. Call `setSample()` when a new hit sample is loaded.
///   2. Call `setLush()` / `setRiserLength()` / `setBPM()` when parameters change.
///   3. Each setter triggers an async rebuild from the appropriate pipeline stage.
///   4. Poll `isNewRiserReady()` from the audio thread; if true, call `consumeRiser()`.
///
/// Thread safety:
///   - Setters may be called from any thread (UI, main, etc.)
///   - `isNewRiserReady()` and `consumeRiser()` are safe from the audio thread (lock-free read)
///   - The pipeline itself runs on a detached background thread
class RvrseProcessor
{
public:
  // --- Setters (trigger async rebuild) ---

  /// Set the source hit sample. Triggers a full pipeline rebuild (reverb → reverse → stretch).
  void setSample(std::shared_ptr<SampleData> sample)
  {
    {
      std::lock_guard<std::mutex> lock(mParamMutex);
      mSourceSample = std::move(sample);
      // Clear cached intermediate buffers since the source changed
      mCachedReversedL.clear();
      mCachedReversedR.clear();
      mCachedReverbedL.clear();
      mCachedReverbedR.clear();
    }
    rebuildAsync(EPipelineStage::Reverb);
  }

  /// Set the Lush amount (0.0–1.0). Triggers rebuild from reverb stage onwards.
  void setLush(float lush)
  {
    {
      std::lock_guard<std::mutex> lock(mParamMutex);
      if (std::abs(mLush - lush) < 1e-6f) return; // No change
      mLush = lush;
      // Clear cached reversed buffer since reverb output changed
      mCachedReversedL.clear();
      mCachedReversedR.clear();
      mCachedReverbedL.clear();
      mCachedReverbedR.clear();
    }
    rebuildAsync(EPipelineStage::Reverb);
  }

  /// Set the riser length in beats. Only re-stretches (skips reverb + reverse).
  /// When offline, performs a synchronous rebuild so bounce/export picks up the
  /// change immediately. In real-time mode, uses async to avoid blocking audio.
  void setRiserLength(double beats, bool offline = false)
  {
    {
      std::lock_guard<std::mutex> lock(mParamMutex);
      if (std::abs(mRiserLengthBeats - beats) < 1e-9) return;
      mRiserLengthBeats = beats;
    }
    if (offline)
      rebuildStretchSync();
    else
      rebuildAsync(EPipelineStage::Stretch);
  }

  /// Set the host BPM. Only re-stretches.
  /// Same offline/real-time branching as setRiserLength.
  void setBPM(double bpm, bool offline = false)
  {
    {
      std::lock_guard<std::mutex> lock(mParamMutex);
      if (std::abs(mBPM - bpm) < 1e-9) return;
      mBPM = bpm;
    }
    if (offline)
      rebuildStretchSync();
    else
      rebuildAsync(EPipelineStage::Stretch);
  }

  /// Set the output sample rate. Triggers full rebuild if changed.
  void setSampleRate(double sr)
  {
    {
      std::lock_guard<std::mutex> lock(mParamMutex);
      if (std::abs(mOutputSampleRate - sr) < 1e-9) return;
      mOutputSampleRate = sr;
      mCachedReversedL.clear();
      mCachedReversedR.clear();
      mCachedReverbedL.clear();
      mCachedReverbedR.clear();
    }
    rebuildAsync(EPipelineStage::Reverb);
  }

  /// Set the stretch quality preset. Triggers stretch rebuild if changed.
  void setStretchQuality(EStretchQuality quality, bool offline = false)
  {
    {
      std::lock_guard<std::mutex> lock(mParamMutex);
      if (mStretchQuality == quality) return;
      mStretchQuality = quality;
    }
    if (offline)
      rebuildStretchSync();
    else
      rebuildAsync(EPipelineStage::Stretch);
  }

  // --- Audio-thread-safe polling ---

  /// @return true if a new riser buffer is ready to be consumed (lock-free).
  bool isNewRiserReady() const { return mNewRiserReady.load(std::memory_order_acquire); }

  /// Consume the new riser buffer (lock-free swap).
  /// Call this from the audio thread when `isNewRiserReady()` returns true.
  /// @return Shared pointer to the new riser data
  std::shared_ptr<RiserData> consumeRiser()
  {
    mNewRiserReady.store(false, std::memory_order_release);
    return std::atomic_load(&mRiserOutput);
  }

  /// @return true if the pipeline is currently processing
  bool isProcessing() const { return mProcessing.load(std::memory_order_acquire); }

private:
  /// Pipeline stages — rebuild starts from the specified stage onwards.
  enum class EPipelineStage
  {
    Reverb = 0, ///< Full rebuild: reverb → reverse → stretch
    Stretch     ///< Partial rebuild: time-stretch only (uses cached reversed buffer)
  };

  /// Launch a background thread to run the pipeline from the given stage.
  void rebuildAsync(EPipelineStage fromStage)
  {
    // Increment the generation counter to invalidate any in-flight builds
    mGeneration.fetch_add(1, std::memory_order_release);
    const int myGen = mGeneration.load(std::memory_order_acquire);

    std::thread([this, fromStage, myGen]() {
      runPipeline(fromStage, myGen);
    }).detach();
  }

  /// Perform a stretch-only rebuild synchronously on the calling thread.
  /// Falls back to async if cached reversed buffers aren't available yet
  /// (i.e. the initial reverb+reverse pass hasn't completed).
  ///
  /// Only called during offline/bounce rendering where real-time deadlines
  /// don't apply. Never call this during real-time playback.
  void rebuildStretchSync()
  {
    // Invalidate in-flight builds and capture our generation token
    const int myGen = mGeneration.fetch_add(1, std::memory_order_release) + 1;

    // Snapshot parameters under lock
    std::shared_ptr<SampleData> sample;
    double riserLengthBeats, bpm, sampleRate;
    EStretchQuality quality;
    std::vector<float> cachedRevL, cachedRevR;
    std::vector<float> cachedRvbL, cachedRvbR;

    {
      std::lock_guard<std::mutex> lock(mParamMutex);
      sample = mSourceSample;
      riserLengthBeats = mRiserLengthBeats;
      bpm = mBPM;
      sampleRate = mOutputSampleRate;
      quality = mStretchQuality;
      cachedRevL = mCachedReversedL;
      cachedRevR = mCachedReversedR;
      cachedRvbL = mCachedReverbedL;
      cachedRvbR = mCachedReverbedR;
    }

    // No cached buffers yet — fall back to async (first load still in progress)
    if (cachedRevL.empty() || !sample || !sample->IsLoaded())
    {
      rebuildAsync(EPipelineStage::Stretch);
      return;
    }

    // --- Synchronous stretch ---
    const double stretchFactor = calcStretchFactor(
      static_cast<int>(cachedRevL.size()), riserLengthBeats, bpm, sampleRate
    );

    auto riser = std::make_shared<RiserData>();
    stretchBufferStereo(cachedRevL, cachedRevR, stretchFactor,
                        riser->mLeft, riser->mRight, sampleRate, quality);
    riser->mSampleRate = sampleRate;

    riser->mReverbedL = std::move(cachedRvbL);
    riser->mReverbedR = std::move(cachedRvbR);
    riser->mReversedL = std::move(cachedRevL);
    riser->mReversedR = std::move(cachedRevR);

    // Tail fade-out
    if (kRiserTailFadeBeats > 0.0)
    {
      const double samplesPerBeat = (sampleRate * 60.0) / bpm;
      const int fadeSamples = std::max(1, static_cast<int>(samplesPerBeat * kRiserTailFadeBeats));
      applyTailFadeOutStereo(riser->mLeft, riser->mRight, fadeSamples);
    }

    // Only publish if no newer rebuild has been requested
    if (mGeneration.load(std::memory_order_acquire) != myGen) return;

    std::atomic_store(&mRiserOutput, riser);
    mNewRiserReady.store(true, std::memory_order_release);
  }

  /// Execute the pipeline synchronously (called on background thread).
  void runPipeline(EPipelineStage fromStage, int generation)
  {
    mProcessing.store(true, std::memory_order_release);

    // Snapshot parameters under lock
    std::shared_ptr<SampleData> sample;
    float lush;
    double riserLengthBeats, bpm, sampleRate;
    EStretchQuality quality;
    std::vector<float> cachedRevL, cachedRevR;
    std::vector<float> cachedRvbL, cachedRvbR;

    {
      std::lock_guard<std::mutex> lock(mParamMutex);
      sample = mSourceSample;
      lush = mLush;
      riserLengthBeats = mRiserLengthBeats;
      bpm = mBPM;
      sampleRate = mOutputSampleRate;
      quality = mStretchQuality;

      if (fromStage == EPipelineStage::Stretch)
      {
        cachedRevL = mCachedReversedL;
        cachedRevR = mCachedReversedR;
        cachedRvbL = mCachedReverbedL;
        cachedRvbR = mCachedReverbedR;
      }
    }

    // Bail early if no sample loaded
    if (!sample || !sample->IsLoaded())
    {
      mProcessing.store(false, std::memory_order_release);
      return;
    }

    // Check if this build has been superseded
    if (mGeneration.load(std::memory_order_acquire) != generation)
    {
      mProcessing.store(false, std::memory_order_release);
      return;
    }

    std::vector<float> reversedL, reversedR;
    std::vector<float> reverbedL, reverbedR;

    if (fromStage == EPipelineStage::Reverb || cachedRevL.empty())
    {
      // --- Stage 0: Resample to output sample rate if needed ---
      // The loaded sample may be at a different rate (e.g. 96kHz file, 48kHz DAW).
      // All pipeline processing must happen at the output rate so that frame counts
      // match what ProcessBlock expects.
      std::vector<float> srcL, srcR;
      double processingRate;

      if (std::abs(sample->mSampleRate - sampleRate) > 0.5)
      {
        resampleLinearStereo(sample->mLeft, sample->mRight,
                             sample->mSampleRate, sampleRate,
                             srcL, srcR);
        processingRate = sampleRate;
      }
      else
      {
        srcL = sample->mLeft;
        srcR = sample->mRight;
        processingRate = sample->mSampleRate;
      }

      // --- Stage 1: Reverb (with tail extension) ---
      // The reverb needs room to ring out. We append silence to the source
      // buffer so the comb/allpass delay lines can decay naturally. Without
      // this, the reverb tail is truncated and the reversed riser has no
      // "whoosh" build-up — defeating the entire purpose of the effect.
      const size_t srcFrames = srcL.size();
      const size_t tailFrames = static_cast<size_t>(processingRate * kReverbTailSeconds);
      const size_t totalFrames = srcFrames + tailFrames;

      // Pad source with silence for the reverb tail
      srcL.resize(totalFrames, 0.0f);
      srcR.resize(totalFrames, 0.0f);

      std::vector<float> lushedL(totalFrames);
      std::vector<float> lushedR(totalFrames);

      applyReverbStereo(
        srcL.data(), srcR.data(),
        lushedL.data(), lushedR.data(),
        totalFrames, processingRate, lush
      );

      // Trim trailing silence so the stretcher doesn't waste work on dead air.
      // The trim is conservative — a small margin ensures no audible decay is lost.
      trimTrailingSilenceStereo(lushedL, lushedR, kSilenceThreshold);

      // Abort check
      if (mGeneration.load(std::memory_order_acquire) != generation)
      {
        mProcessing.store(false, std::memory_order_release);
        return;
      }

      // --- Stage 2: Reverse ---
      // Save pre-reverse (reverbed) buffers for debug playback
      reverbedL = lushedL;
      reverbedR = lushedR;

      reverseBufferStereo(lushedL, lushedR);
      reversedL = std::move(lushedL);
      reversedR = std::move(lushedR);

      // Cache the reversed + reverbed buffers for future stretch-only rebuilds
      {
        std::lock_guard<std::mutex> lock(mParamMutex);
        mCachedReversedL = reversedL;
        mCachedReversedR = reversedR;
        mCachedReverbedL = reverbedL;
        mCachedReverbedR = reverbedR;
      }
    }
    else
    {
      // Use cached buffers (stretch-only rebuild)
      reversedL = std::move(cachedRevL);
      reversedR = std::move(cachedRevR);
      reverbedL = std::move(cachedRvbL);
      reverbedR = std::move(cachedRvbR);
    }

    // Abort check
    if (mGeneration.load(std::memory_order_acquire) != generation)
    {
      mProcessing.store(false, std::memory_order_release);
      return;
    }

    // --- Stage 3: Time-Stretch ---
    const double stretchFactor = calcStretchFactor(
      static_cast<int>(reversedL.size()), riserLengthBeats, bpm, sampleRate
    );

    auto riser = std::make_shared<RiserData>();
    stretchBufferStereo(reversedL, reversedR, stretchFactor,
                        riser->mLeft, riser->mRight, sampleRate, quality);
    riser->mSampleRate = sampleRate;

    // Store intermediate buffers for debug playback
    riser->mReverbedL = std::move(reverbedL);
    riser->mReverbedR = std::move(reverbedR);
    riser->mReversedL = std::move(reversedL);
    riser->mReversedR = std::move(reversedR);

    // --- Stage 4: Tail fade-out ---
    // Short fade at the end of the riser so the reversed transient doesn't
    // produce a hard click when it meets the dry hit.
    // Controlled by kRiserTailFadeBeats in Constants.h. Set to 0 to disable.
    if (kRiserTailFadeBeats > 0.0)
    {
      const double samplesPerBeat = (sampleRate * 60.0) / bpm;
      const int fadeSamples = std::max(1, static_cast<int>(samplesPerBeat * kRiserTailFadeBeats));
      applyTailFadeOutStereo(riser->mLeft, riser->mRight, fadeSamples);
    }

    // Final abort check before publishing
    if (mGeneration.load(std::memory_order_acquire) != generation)
    {
      mProcessing.store(false, std::memory_order_release);
      return;
    }

    // Publish the result (atomic store for lock-free audio-thread read)
    std::atomic_store(&mRiserOutput, riser);
    mNewRiserReady.store(true, std::memory_order_release);
    mProcessing.store(false, std::memory_order_release);
  }

  // --- Parameters (protected by mParamMutex) ---
  std::mutex mParamMutex;
  std::shared_ptr<SampleData> mSourceSample;
  float mLush = static_cast<float>(kLushDefault / 100.0);
  double mRiserLengthBeats = kRiserLengthValues[kRiserLengthDefault];
  double mBPM = kDefaultBPM;
  double mOutputSampleRate = 44100.0;
  EStretchQuality mStretchQuality = static_cast<EStretchQuality>(kStretchQualityDefault);

  // --- Cached intermediate buffers (protected by mParamMutex) ---
  std::vector<float> mCachedReversedL;
  std::vector<float> mCachedReversedR;
  std::vector<float> mCachedReverbedL;
  std::vector<float> mCachedReverbedR;

  // --- Output (lock-free handoff to audio thread) ---
  std::shared_ptr<RiserData> mRiserOutput;
  std::atomic<bool> mNewRiserReady { false };

  // --- Build management ---
  std::atomic<int> mGeneration { 0 };   ///< Incremented on every rebuild request
  std::atomic<bool> mProcessing { false };
};

} // namespace rvrse
