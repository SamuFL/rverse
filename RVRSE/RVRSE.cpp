#include "RVRSE.h"
#include "IPlug_include_in_plug_src.h"

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
    const IRECT titleBounds = innerBounds.GetCentredInside(300, 60);

    if (pGraphics->NControls()) {
      pGraphics->GetBackgroundControl()->SetTargetAndDrawRECTs(bounds);
      pGraphics->GetControlWithTag(kCtrlTagTitle)->SetTargetAndDrawRECTs(titleBounds);
      pGraphics->GetControlWithTag(kCtrlTagVersionNumber)->SetTargetAndDrawRECTs(versionBounds);
      return;
    }

    pGraphics->SetLayoutOnResize(true);
    pGraphics->AttachCornerResizer(EUIResizerMode::Size, true);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->AttachPanelBackground(COLOR_DARK_GRAY);
    pGraphics->AttachControl(new ITextControl(titleBounds, "RVRSE", IText(40, COLOR_WHITE)), kCtrlTagTitle);
    WDL_String buildInfoStr;
    GetBuildInfoStr(buildInfoStr, __DATE__, __TIME__);
    pGraphics->AttachControl(new ITextControl(versionBounds, buildInfoStr.Get(), IText(12, COLOR_LIGHT_GRAY).WithAlign(EAlign::Far)), kCtrlTagVersionNumber);
  };
#endif
}

#if IPLUG_DSP
void RVRSE::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const int nChans = NOutChansConnected();
  
  // For now, output silence — sample playback comes in Phase 1
  for (int s = 0; s < nFrames; s++) {
    for (int c = 0; c < nChans; c++) {
      outputs[c][s] = 0.0;
    }
  }
}

void RVRSE::ProcessMidiMsg(const IMidiMsg& msg)
{
  // MIDI handling will be implemented in Phase 1
  // For now, just pass through to the base class
}
#endif
