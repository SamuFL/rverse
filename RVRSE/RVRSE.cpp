#include "RVRSE.h"
#include "IPlug_include_in_plug_src.h"
#include "Constants.h"
#include "SampleLoader.h"

#include <algorithm>
#include <thread>

#if IPLUG_EDITOR
#include "IControls.h"
#endif

RVRSE::RVRSE(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamMasterVol)->InitDouble("Master Volume", 100., 0., 100.0, 0.01, "%");

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS);
  };
  
  mLayoutFunc = [&](IGraphics* pGraphics) {
    const IRECT bounds = pGraphics->GetBounds();
    const IRECT innerBounds = bounds.GetPadded(-10.f);
    const IRECT versionBounds = innerBounds.GetFromTRHC(300, 20);
    const IRECT titleBounds = innerBounds.GetCentredInside(300, 60).GetVShifted(-80.f);
    const IRECT loadBtnBounds = innerBounds.GetCentredInside(200, 40);
    const IRECT sampleNameBounds = innerBounds.GetCentredInside(600, 24).GetVShifted(40.f);

    if (pGraphics->NControls()) {
      pGraphics->GetBackgroundControl()->SetTargetAndDrawRECTs(bounds);
      pGraphics->GetControlWithTag(kCtrlTagTitle)->SetTargetAndDrawRECTs(titleBounds);
      pGraphics->GetControlWithTag(kCtrlTagVersionNumber)->SetTargetAndDrawRECTs(versionBounds);
      pGraphics->GetControlWithTag(kCtrlTagLoadButton)->SetTargetAndDrawRECTs(loadBtnBounds);
      pGraphics->GetControlWithTag(kCtrlTagSampleName)->SetTargetAndDrawRECTs(sampleNameBounds);
      return;
    }

    pGraphics->SetLayoutOnResize(true);
    pGraphics->AttachCornerResizer(EUIResizerMode::Size, true);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->AttachPanelBackground(COLOR_DARK_GRAY);

    // Title
    pGraphics->AttachControl(new ITextControl(titleBounds, "RVRSE",
      IText(40, COLOR_WHITE)), kCtrlTagTitle);

    // Version info
    WDL_String buildInfoStr;
    GetBuildInfoStr(buildInfoStr, __DATE__, __TIME__);
    pGraphics->AttachControl(new ITextControl(versionBounds, buildInfoStr.Get(),
      IText(12, COLOR_LIGHT_GRAY).WithAlign(EAlign::Far)), kCtrlTagVersionNumber);

    // Load Sample button
    const IVStyle buttonStyle = DEFAULT_STYLE
      .WithColor(kFG, IColor(255, 100, 180, 255))
      .WithColor(kBG, IColor(255, 40, 40, 40))
      .WithRoundness(0.3f);

    pGraphics->AttachControl(new IVButtonControl(loadBtnBounds, [this](IControl* pCaller) {
      // Spawn file dialog from the UI thread
      WDL_String fileName;
      WDL_String path;
      pCaller->GetUI()->PromptForFile(fileName, path, EFileAction::Open,
        rvrse::kSupportedAudioExts,
        [this, pCaller](const WDL_String& fileName, const WDL_String& path) {
          // Note: on macOS, iPlug2 sets fileName to the FULL path,
          // and path to the directory with trailing slash.
          if (fileName.GetLength() > 0)
          {
            LoadSampleFromFile(fileName.Get());
          }
        });
    }, "LOAD SAMPLE", buttonStyle), kCtrlTagLoadButton);

    // Sample name display
    pGraphics->AttachControl(new ITextControl(sampleNameBounds, "No sample loaded",
      IText(14, IColor(200, 180, 180, 180))), kCtrlTagSampleName);
  };
#endif
}

void RVRSE::LoadSampleFromFile(const char* filePath)
{
  mLoadState.store(rvrse::ESampleLoadState::Loading);

  // Update UI to show loading state
  if (GetUI())
  {
    if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagSampleName))
      pCtrl->As<ITextControl>()->SetStr("Loading...");
  }

  // Do the actual loading on a background thread to avoid blocking the UI
  std::string pathCopy(filePath);
  std::thread([this, pathCopy]() {
    auto result = rvrse::LoadSample(pathCopy);

    if (result.success)
    {
      auto newSample = std::make_shared<rvrse::SampleData>(std::move(result.data));

      {
        std::lock_guard<std::mutex> lock(mSampleMutex);
        mHitSample = newSample;
      }

      // Signal the audio thread that a new sample is ready
      mNewSampleReady.store(true, std::memory_order_release);
      mLoadState.store(rvrse::ESampleLoadState::Ready);

      // Update UI on the main thread via SendControlMsgFromDelegate isn't ideal,
      // so we use GetUI() — this is safe because we check and the UI lambda captures
      // will run on the next UI tick
      if (GetUI())
      {
        if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagSampleName))
        {
          WDL_String displayStr;
          displayStr.SetFormatted(256, "%s (%d Hz, %s, %.1fs)",
            newSample->mFileName.c_str(),
            static_cast<int>(newSample->mSampleRate),
            newSample->mNumChannels == 1 ? "mono" : "stereo",
            static_cast<double>(newSample->NumFrames()) / newSample->mSampleRate);
          pCtrl->As<ITextControl>()->SetStr(displayStr.Get());
          pCtrl->SetDirty(false);
        }
      }
    }
    else
    {
      mLoadState.store(rvrse::ESampleLoadState::Error);

      if (GetUI())
      {
        if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagSampleName))
        {
          WDL_String errStr;
          errStr.SetFormatted(256, "Error: %s", result.errorMessage.c_str());
          pCtrl->As<ITextControl>()->SetStr(errStr.Get());
          pCtrl->SetDirty(false);
        }
      }
    }
  }).detach();
}

#if IPLUG_DSP
void RVRSE::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const int nChans = NOutChansConnected();
  const double masterVol = GetParam(kParamMasterVol)->Value() / 100.0;

  // Check if a new sample is ready from the loader thread (lock-free flag)
  if (mNewSampleReady.load(std::memory_order_acquire))
  {
    {
      std::lock_guard<std::mutex> lock(mSampleMutex);
      mPlaySample = mHitSample;
    }
    mNewSampleReady.store(false, std::memory_order_release);
    mPlaybackPos = -1; // Reset playback when new sample arrives
  }

  // Get local pointer — no further locking needed
  const auto& sample = mPlaySample;

  for (int s = 0; s < nFrames; s++)
  {
    // Process MIDI events at this sample offset
    while (!mMidiQueue.Empty() && mMidiQueue.Peek().mOffset <= s)
    {
      const IMidiMsg& msg = mMidiQueue.Peek();

      if (msg.StatusMsg() == IMidiMsg::kNoteOn && msg.Velocity() > 0)
      {
        // Note-on: restart playback from the beginning
        mPlaybackPos = 0;
        mVelocityGain = static_cast<float>(msg.Velocity()) / 127.0f;
        mFadeOutRemaining = 0; // Cancel any active fade-out
      }
      else if (msg.StatusMsg() == IMidiMsg::kNoteOff ||
               (msg.StatusMsg() == IMidiMsg::kNoteOn && msg.Velocity() == 0))
      {
        // Note-off: begin fade-out instead of hard stop
        if (mPlaybackPos >= 0)
        {
          mFadeOutRemaining = mFadeOutLength;
        }
      }

      mMidiQueue.Remove();
    }

    // Generate output
    float outL = 0.0f;
    float outR = 0.0f;

    if (mPlaybackPos >= 0 && sample && sample->IsLoaded())
    {
      if (mPlaybackPos < sample->NumFrames())
      {
        outL = sample->mLeft[mPlaybackPos] * mVelocityGain;
        outR = sample->mRight[mPlaybackPos] * mVelocityGain;

        // Apply fade-out envelope if active
        if (mFadeOutRemaining > 0)
        {
          const float fadeGain = static_cast<float>(mFadeOutRemaining) / static_cast<float>(mFadeOutLength);
          outL *= fadeGain;
          outR *= fadeGain;
          mFadeOutRemaining--;

          if (mFadeOutRemaining == 0)
          {
            // Fade complete — stop playback
            mPlaybackPos = -1;
          }
        }

        if (mPlaybackPos >= 0) mPlaybackPos++;
      }
      else
      {
        // Reached end of sample — stop playback
        mPlaybackPos = -1;
      }
    }

    // Apply master volume and write to output
    if (nChans >= 1) outputs[0][s] = outL * masterVol;
    if (nChans >= 2) outputs[1][s] = outR * masterVol;
  }

  mMidiQueue.Flush(nFrames);
}

void RVRSE::ProcessMidiMsg(const IMidiMsg& msg)
{
  TRACE
  mMidiQueue.Add(msg);
}

void RVRSE::OnReset()
{
  mMidiQueue.Resize(GetBlockSize());
  // Compute fade-out length from sample rate (e.g., 5ms at 44100 Hz = 220 samples)
  mFadeOutLength = std::max(1, static_cast<int>(GetSampleRate() * rvrse::kNoteOffFadeMs / 1000.0));
}
#endif
