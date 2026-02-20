#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "SampleData.h"

#include <atomic>
#include <memory>
#include <mutex>

const int kNumPresets = 1;

enum EParams
{
  kParamMasterVol = 0,
  kNumParams
};

enum ECtrlTags
{
  kCtrlTagVersionNumber = 0,
  kCtrlTagTitle,
  kCtrlTagLoadButton,
  kCtrlTagSampleName
};

using namespace iplug;
using namespace igraphics;

class RVRSE final : public Plugin
{
public:
  RVRSE(const InstanceInfo& info);

#if IPLUG_EDITOR
  bool OnHostRequestingSupportedViewConfiguration(int width, int height) override { return true; }
#endif
  
#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
#endif

private:
  /// Load a sample file from disk (called from UI thread, does work on background thread)
  void LoadSampleFromFile(const char* filePath);

  // --- Sample data (offline → real-time handoff) ---

  /// The loaded hit sample — written by background loader, read by audio thread.
  std::shared_ptr<rvrse::SampleData> mHitSample;
  std::mutex mSampleMutex; ///< Protects mHitSample swap (not held on audio thread)

  /// Audio-thread's local copy of the sample pointer (lock-free read)
  std::shared_ptr<rvrse::SampleData> mPlaySample;

  /// Set to true by loader thread when a new sample is ready for the audio thread
  std::atomic<bool> mNewSampleReady { false };

  /// Current load state for UI feedback
  std::atomic<rvrse::ESampleLoadState> mLoadState { rvrse::ESampleLoadState::Empty };

  // --- Playback state (audio thread only, no locks needed) ---

  IMidiQueue mMidiQueue;       ///< Sample-accurate MIDI message queue
  int mPlaybackPos = -1;       ///< Current playback position in sample frames (-1 = not playing)
  float mVelocityGain = 1.0f;  ///< Velocity-scaled gain for current note (0.0–1.0)

  // --- Note-off fade-out (anti-click) ---
  int mFadeOutRemaining = 0;   ///< Samples remaining in fade-out (0 = no fade active)
  int mFadeOutLength = 0;      ///< Total fade-out length in samples (computed from sample rate)
};
