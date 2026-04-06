#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "RvrseProcessor.h"
#include "SampleData.h"
#include "Stutter.h"

#include <atomic>
#include <memory>
#include <mutex>

const int kNumPresets = 1;

enum EParams
{
  kParamMasterVol = 0,
  kParamStutterRate,
  kParamStutterDepth,
  kParamLush,
  kParamRiserLength,
  kParamFadeIn,
  kParamRiserVolume,
  kParamHitVolume,
  kParamDebugStage,
  kParamStretchQuality,
  kNumParams
};

enum ECtrlTags
{
  // Header
  kCtrlTagTitle = 0,
  kCtrlTagLoadButton,
  kCtrlTagSampleName,
  // Zone panels (backgrounds)
  kCtrlTagHeaderPanel,
  kCtrlTagWaveformPanel,
  kCtrlTagRiserPanel,
  kCtrlTagHitPanel,
  kCtrlTagFooterPanel,
  // Footer
  kCtrlTagVersionNumber,
  kCtrlTagMasterVolSlider,
  kCtrlTagMidiIndicator,
  // BPM display
  kCtrlTagBPMDisplay,
  // Riser panel knobs
  kCtrlTagStutterRate,
  kCtrlTagStutterDepth,
  kCtrlTagRiserSectionLabel,
  // Riser offline knobs
  kCtrlTagLush,
  kCtrlTagRiserLength,
  kCtrlTagFadeIn,
  kCtrlTagRiserVolume,
  kCtrlTagStretchQuality,
  kCtrlTagOfflineSectionLabel,
  // Hit panel
  kCtrlTagHitSectionLabel,
  kCtrlTagHitVolume,
  kCtrlTagSupportButton,
  kCtrlTagLogo,
  kCtrlTagMasterVolLabel,
  kCtrlTagMasterVolValue,
  kCtrlTagWaveformDisplay,
  kCtrlTagHitPreview,
  kNumCtrlTags
};

using namespace iplug;
using namespace igraphics;

class RVRSE final : public Plugin
{
public:
  RVRSE(const InstanceInfo& info);

  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;

#if IPLUG_EDITOR
  bool OnHostRequestingSupportedViewConfiguration(int width, int height) override { return true; }
  void OnIdle() override;
#endif
  
#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
#endif

private:
  /// Load a sample file from disk (called from UI thread, does work on background thread)
  void LoadSampleFromFile(const char* filePath);

  /// Persisted sample file path (saved/restored with DAW project)
  std::string mSampleFilePath;

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
  float mVelocityGain = 1.0f;  ///< Velocity-scaled gain for current note (0.0–1.0)

  // --- Hit voice ---
  int mHitOffset = -1;         ///< Sample offset at which hit fires (riser length in samples)
  int mSamplesFromNoteOn = -1; ///< Counter from note-on to trigger hit at mHitOffset

  // --- Note-off fade-out (anti-click) ---
  int mRiserFadeRemaining = 0; ///< Samples remaining in riser fade-out (0 = no fade active)
  int mHitFadeRemaining = 0;   ///< Samples remaining in hit fade-out
  int mFadeOutLength = 0;      ///< Total fade-out length in samples (computed from sample rate)

  // --- Offline pipeline ---
  rvrse::RvrseProcessor mProcessor;   ///< Offline pipeline orchestrator (reverb → reverse → stretch)
  std::atomic<double> mLastBPM { 0.0 };  ///< Last BPM from host (written by audio thread, read by UI)
  double mLastDisplayedBPM = -1.0;    ///< Last BPM shown in GUI (avoids redundant UI updates)
  float mLastLush = -1.0f;            ///< Last Lush value sent to processor
  double mLastRiserLength = -1.0;     ///< Last Riser Length sent to processor
  int mLastStretchQuality = -1;       ///< Last Stretch Quality sent to processor

  /// Audio-thread's local copy of the riser buffer (lock-free read from processor)
  /// NOTE: shared_ptr read/write across threads is technically a data race in C++17.
  /// In practice, swaps happen once per ProcessBlock and reads once per UI frame,
  /// making collision vanishingly unlikely on aligned pointer stores (x86/ARM64).
  /// A fully correct fix requires C++20 std::atomic<shared_ptr> or a mutex snapshot.
  std::shared_ptr<rvrse::RiserData> mRiserBuffer;

  // --- Playback positions (audio thread writes, UI thread reads for playhead) ---
  std::atomic<int> mRiserPos { -1 };  ///< Current playback position in riser buffer (-1 = not playing)
  std::atomic<int> mHitPos { -1 };    ///< Current playback position in hit sample (-1 = not playing)

  // --- Waveform display state (UI thread only) ---
  std::shared_ptr<rvrse::RiserData> mWaveformLastRiser; ///< Last riser pointer fed to waveform
  std::shared_ptr<rvrse::SampleData> mWaveformLastHit;  ///< Last hit pointer fed to waveform
  std::vector<float> mWaveformMonoBuf; ///< Temp buffer for stereo→mono mix

  // --- Stutter gate (audio thread only) ---
  rvrse::StutterState mStutterState;  ///< Per-voice stutter phase state

  // --- MIDI activity indicator ---
  std::atomic<int> mMidiActivityCounter { 0 }; ///< Incremented by audio thread on any MIDI event
  int mMidiLastSeenCounter = 0;                ///< UI thread's last seen counter value
  int mMidiCooldownFrames = 0;                 ///< OnIdle frames remaining before dimming
};
