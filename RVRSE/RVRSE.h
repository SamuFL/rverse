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

  /// The loaded hit sample — written by background loader, read by audio thread.
  /// Access is guarded by mSampleMutex for the swap, then the audio thread uses
  /// the shared_ptr copy lock-free (shared_ptr copies are atomic on the refcount).
  std::shared_ptr<rvrse::SampleData> mHitSample;
  std::mutex mSampleMutex; ///< Protects mHitSample swap (not held on audio thread)

  /// Current load state for UI feedback
  std::atomic<rvrse::ESampleLoadState> mLoadState { rvrse::ESampleLoadState::Empty };
};
