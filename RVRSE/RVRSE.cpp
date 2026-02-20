#include "RVRSE.h"
#include "IPlug_include_in_plug_src.h"
#include "Constants.h"
#include "SampleLoader.h"

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
    const IRECT sampleNameBounds = innerBounds.GetCentredInside(400, 24).GetVShifted(40.f);

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
          if (fileName.GetLength() > 0)
          {
            WDL_String fullPath;
            fullPath.Set(path.Get());
            fullPath.Append("/");
            fullPath.Append(fileName.Get());
            LoadSampleFromFile(fullPath.Get());
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
  
  // For now, output silence — sample playback comes in rverse-p38
  for (int s = 0; s < nFrames; s++) {
    for (int c = 0; c < nChans; c++) {
      outputs[c][s] = 0.0;
    }
  }
}

void RVRSE::ProcessMidiMsg(const IMidiMsg& msg)
{
  // MIDI handling will be implemented in rverse-p38
  // For now, just pass through to the base class
}

void RVRSE::OnReset()
{
  // Called when sample rate or block size changes
  // Will be used to resample loaded samples in future tasks
}
#endif
