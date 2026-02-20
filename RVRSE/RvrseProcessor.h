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

/// Result of the offline pipeline — the ready-to-play riser buffer.
struct RiserData
{
  std::vector<float> mLeft;    ///< Left channel of final_riser[]
  std::vector<float> mRight;   ///< Right channel of final_riser[]
  double mSampleRate = 0.0;    ///< Sample rate of the riser data

  int NumFrames() const { return static_cast<int>(mLeft.size()); }
  bool IsReady() const { return !mLeft.empty() && mSampleRate > 0.0; }
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
    }
    rebuildAsync(EPipelineStage::Reverb);
  }

  /// Set the riser length in beats. Only re-stretches (skips reverb + reverse).
  void setRiserLength(double beats)
  {
    {
      std::lock_guard<std::mutex> lock(mParamMutex);
      if (std::abs(mRiserLengthBeats - beats) < 1e-9) return;
      mRiserLengthBeats = beats;
    }
    rebuildAsync(EPipelineStage::Stretch);
  }

  /// Set the host BPM. Only re-stretches.
  void setBPM(double bpm)
  {
    {
      std::lock_guard<std::mutex> lock(mParamMutex);
      if (std::abs(mBPM - bpm) < 1e-9) return;
      mBPM = bpm;
    }
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
    }
    rebuildAsync(EPipelineStage::Reverb);
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

  /// Execute the pipeline synchronously (called on background thread).
  void runPipeline(EPipelineStage fromStage, int generation)
  {
    mProcessing.store(true, std::memory_order_release);

    // Snapshot parameters under lock
    std::shared_ptr<SampleData> sample;
    float lush;
    double riserLengthBeats, bpm, sampleRate;
    std::vector<float> cachedRevL, cachedRevR;

    {
      std::lock_guard<std::mutex> lock(mParamMutex);
      sample = mSourceSample;
      lush = mLush;
      riserLengthBeats = mRiserLengthBeats;
      bpm = mBPM;
      sampleRate = mOutputSampleRate;

      if (fromStage == EPipelineStage::Stretch)
      {
        cachedRevL = mCachedReversedL;
        cachedRevR = mCachedReversedR;
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

      // --- Stage 1: Reverb ---
      const size_t numFrames = srcL.size();
      std::vector<float> lushedL(numFrames);
      std::vector<float> lushedR(numFrames);

      applyReverbStereo(
        srcL.data(), srcR.data(),
        lushedL.data(), lushedR.data(),
        numFrames, processingRate, lush
      );

      // Abort check
      if (mGeneration.load(std::memory_order_acquire) != generation)
      {
        mProcessing.store(false, std::memory_order_release);
        return;
      }

      // --- Stage 2: Reverse ---
      reverseBufferStereo(lushedL, lushedR);
      reversedL = std::move(lushedL);
      reversedR = std::move(lushedR);

      // Cache the reversed buffers for future stretch-only rebuilds
      {
        std::lock_guard<std::mutex> lock(mParamMutex);
        mCachedReversedL = reversedL;
        mCachedReversedR = reversedR;
      }
    }
    else
    {
      // Use cached reversed buffers (stretch-only rebuild)
      reversedL = std::move(cachedRevL);
      reversedR = std::move(cachedRevR);
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
                        riser->mLeft, riser->mRight);
    riser->mSampleRate = sampleRate;

    // --- Stage 4: Tail fade-out (1 beat) ---
    // Fade the end of the riser to zero over 1 beat so that the reversed
    // transient doesn't clash with the dry hit that fires immediately after.
    const double samplesPerBeat = (sampleRate * 60.0) / bpm;
    const int fadeSamples = std::max(1, static_cast<int>(samplesPerBeat));
    applyTailFadeOutStereo(riser->mLeft, riser->mRight, fadeSamples);

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
  double mRiserLengthBeats = kRiserLengthDefault;
  double mBPM = kDefaultBPM;
  double mOutputSampleRate = 44100.0;

  // --- Cached intermediate buffers (protected by mParamMutex) ---
  std::vector<float> mCachedReversedL;
  std::vector<float> mCachedReversedR;

  // --- Output (lock-free handoff to audio thread) ---
  std::shared_ptr<RiserData> mRiserOutput;
  std::atomic<bool> mNewRiserReady { false };

  // --- Build management ---
  std::atomic<int> mGeneration { 0 };   ///< Incremented on every rebuild request
  std::atomic<bool> mProcessing { false };
};

} // namespace rvrse
