#include "RVRSE.h"
#include "IPlug_include_in_plug_src.h"
#include "BufferUtils.h"
#include "Constants.h"
#include "SampleLoader.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <thread>

#if IPLUG_EDITOR
#include "IControls.h"
#include "GUIColors.h"
#endif

RVRSE::RVRSE(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamMasterVol)->InitDouble("Master Volume", 100., 0., 100.0, 0.01, "%");
  GetParam(kParamStutterRate)->InitDouble("Stutter Rate",
    rvrse::kStutterRateDefaultHz, rvrse::kStutterRateMinHz, rvrse::kStutterRateMaxHz, 0.1, "Hz");
  GetParam(kParamStutterDepth)->InitDouble("Stutter Depth",
    rvrse::kStutterDepthDefault / 100.0, 0., 1.0, 0.01, "");
  GetParam(kParamLush)->InitDouble("Lush",
    rvrse::kLushDefault, 0., 100.0, 0.1, "%");
  GetParam(kParamRiserLength)->InitEnum("Riser Length", rvrse::kRiserLengthDefault, {
    "1/4", "1/2", "1", "2", "4", "8", "16"
  });
  GetParam(kParamFadeIn)->InitDouble("Fade In",
    rvrse::kFadeInDefault, 0., 100.0, 0.1, "%");
  GetParam(kParamRiserVolume)->InitDouble("Riser Volume",
    rvrse::kRiserVolumeDefault, rvrse::kVolumeMinDb, rvrse::kVolumeMaxDb, 0.1, "dB");
  GetParam(kParamHitVolume)->InitDouble("Hit Volume",
    rvrse::kHitVolumeDefault, rvrse::kVolumeMinDb, rvrse::kVolumeMaxDb, 0.1, "dB");
  GetParam(kParamDebugStage)->InitEnum("Debug Stage", rvrse::kDebugNormal, {
    "Normal", "Reverbed", "Reversed", "Riser Only"
  });
  GetParam(kParamStretchQuality)->InitEnum("Stretch Quality", rvrse::kStretchQualityDefault, {
    "High", "Low"
  });

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS);
  };
  
  mLayoutFunc = [&](IGraphics* pGraphics) {
    using namespace rvrse::gui;
    const IRECT b = pGraphics->GetBounds();
    const float w = b.W();
    const float h = b.H();

    // ── Scale layout proportions relative to default 1024×620 ──────────
    const float scaleY = h / 620.f;
    const float headerH  = kHeaderHeight  * scaleY;
    const float footerH  = kFooterHeight  * scaleY;
    const float waveH    = kWaveformHeight * scaleY;
    const float gap      = kZoneGap;

    // ── Zone rects ─────────────────────────────────────────────────────
    const IRECT headerRect   = b.GetFromTop(headerH);
    const IRECT footerRect   = b.GetFromBottom(footerH);
    const IRECT waveformRect = IRECT(b.L + gap, headerRect.B + gap,
                                     b.R - gap, headerRect.B + gap + waveH);
    const float panelTop    = waveformRect.B + gap;
    const float panelBottom = footerRect.T - gap;
    const float panelMid    = b.L + gap + (w - 2.f * gap) * kRiserPanelPct;
    const IRECT riserRect   = IRECT(b.L + gap, panelTop, panelMid - gap * 0.5f, panelBottom);
    const IRECT hitRect     = IRECT(panelMid + gap * 0.5f, panelTop, b.R - gap, panelBottom);

    // ── Resize path — reposition existing controls ─────────────────────
    if (pGraphics->NControls()) {
      pGraphics->GetBackgroundControl()->SetTargetAndDrawRECTs(b);
      pGraphics->GetControlWithTag(kCtrlTagHeaderPanel)->SetTargetAndDrawRECTs(headerRect);
      pGraphics->GetControlWithTag(kCtrlTagWaveformPanel)->SetTargetAndDrawRECTs(waveformRect);
      pGraphics->GetControlWithTag(kCtrlTagRiserPanel)->SetTargetAndDrawRECTs(riserRect);
      pGraphics->GetControlWithTag(kCtrlTagHitPanel)->SetTargetAndDrawRECTs(hitRect);
      pGraphics->GetControlWithTag(kCtrlTagFooterPanel)->SetTargetAndDrawRECTs(footerRect);

      // Reposition header contents
      const IRECT titleBounds = headerRect.GetPadded(-8.f).GetFromLeft(300.f).GetFromTop(headerH * 0.65f);
      pGraphics->GetControlWithTag(kCtrlTagTitle)->SetTargetAndDrawRECTs(titleBounds);
      const IRECT loadBtnBounds = headerRect.GetCentredInside(160.f, 34.f);
      pGraphics->GetControlWithTag(kCtrlTagLoadButton)->SetTargetAndDrawRECTs(loadBtnBounds);
      const IRECT sampleBounds = headerRect.GetPadded(-8.f).GetFromRight(300.f).GetFromTop(headerH * 0.6f);
      pGraphics->GetControlWithTag(kCtrlTagSampleName)->SetTargetAndDrawRECTs(sampleBounds);
      const IRECT bpmBounds = headerRect.GetPadded(-8.f).GetFromRight(300.f).GetFromBottom(headerH * 0.4f);
      pGraphics->GetControlWithTag(kCtrlTagBPMDisplay)->SetTargetAndDrawRECTs(bpmBounds);
      const IRECT versionBounds = footerRect.GetPadded(-8.f).GetFromRight(200.f);
      pGraphics->GetControlWithTag(kCtrlTagVersionNumber)->SetTargetAndDrawRECTs(versionBounds);
      // Footer: master vol slider + MIDI indicator
      const IRECT masterSliderBounds = footerRect.GetPadded(-8.f).GetFromLeft(220.f).GetCentredInside(200.f, 30.f);
      pGraphics->GetControlWithTag(kCtrlTagMasterVolSlider)->SetTargetAndDrawRECTs(masterSliderBounds);
      const IRECT midiBounds = footerRect.GetPadded(-8.f).GetMidHPadded(30.f);
      pGraphics->GetControlWithTag(kCtrlTagMidiIndicator)->SetTargetAndDrawRECTs(midiBounds);
      // Riser panel contents
      const IRECT riserLabelBounds = riserRect.GetPadded(-8.f).GetFromTop(20.f);
      pGraphics->GetControlWithTag(kCtrlTagRiserSectionLabel)->SetTargetAndDrawRECTs(riserLabelBounds);
      const IRECT knobArea = riserRect.GetPadded(-8.f).GetReducedFromTop(28.f).GetFromTop(90.f);
      const IRECT stutterRateBounds = knobArea.GetGridCell(0, 1, 2).GetCentredInside(75.f, 90.f);
      pGraphics->GetControlWithTag(kCtrlTagStutterRate)->SetTargetAndDrawRECTs(stutterRateBounds);
      const IRECT stutterDepthBounds = knobArea.GetGridCell(1, 1, 2).GetCentredInside(75.f, 90.f);
      pGraphics->GetControlWithTag(kCtrlTagStutterDepth)->SetTargetAndDrawRECTs(stutterDepthBounds);
      return;
    }

    // ── First-time setup ───────────────────────────────────────────────
    pGraphics->SetLayoutOnResize(true);
    pGraphics->AttachCornerResizer(EUIResizerMode::Size, true);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Roboto-Bold", ROBOTO_BOLD_FN);

    // Main background
    pGraphics->AttachPanelBackground(kColorDark);

    // Helper: draw a filled rounded rect panel
    auto MakeRoundedPanel = [](const IRECT& bounds, const IColor& color, float radius) {
      return new ILambdaControl(bounds, [color, radius](ILambdaControl* pCaller, IGraphics& g, IRECT& r) {
        g.FillRoundRect(color, r, radius);
      }, DEFAULT_ANIMATION_DURATION, false, false);
    };

    // Zone panels — rounded rects for waveform, riser, hit panels
    pGraphics->AttachControl(new IPanelControl(headerRect, kColorHeaderBg), kCtrlTagHeaderPanel);
    pGraphics->AttachControl(MakeRoundedPanel(waveformRect, kColorWaveformBg, 6.f), kCtrlTagWaveformPanel);
    pGraphics->AttachControl(MakeRoundedPanel(riserRect, kColorDarkGrey, 6.f), kCtrlTagRiserPanel);
    pGraphics->AttachControl(MakeRoundedPanel(hitRect, kColorDarkGrey, 6.f), kCtrlTagHitPanel);
    pGraphics->AttachControl(new IPanelControl(footerRect, kColorHeaderBg), kCtrlTagFooterPanel);

    // ── Header contents ────────────────────────────────────────────────
    // Title: top-left, bold gold
    const IRECT titleBounds = headerRect.GetPadded(-8.f).GetFromLeft(300.f).GetFromTop(headerH * 0.65f);
    pGraphics->AttachControl(new ITextControl(titleBounds, "RVRSE",
      IText(40, kColorGold, "Roboto-Bold", EAlign::Near, EVAlign::Middle)), kCtrlTagTitle);

    // Load Sample button: centered
    const IRECT loadBtnBounds = headerRect.GetCentredInside(160.f, 32.f).GetVShifted(1.f);
    const IVStyle loadBtnStyle = DEFAULT_STYLE
      .WithColor(kFG, kColorDarkGrey)
      .WithColor(kBG, IColor(0, 0, 0, 0))
      .WithColor(kPR, kColorDarkGrey)
      .WithColor(kFR, kColorGold)
      .WithColor(kHL, kColorGold.WithOpacity(0.1f))
      .WithDrawFrame(true)
      .WithFrameThickness(1.5f)
      .WithDrawShadows(false)
      .WithEmboss(false)
      .WithRoundness(0.3f)
      .WithShowValue(false)
      .WithLabelText(IText(13, kColorGold, "Roboto-Regular", EAlign::Center, EVAlign::Middle));

    pGraphics->AttachControl(new IVButtonControl(loadBtnBounds, [this](IControl* pCaller) {
      WDL_String fileName;
      WDL_String path;
      pCaller->GetUI()->PromptForFile(fileName, path, EFileAction::Open,
        rvrse::kSupportedAudioExts,
        [this, pCaller](const WDL_String& fileName, const WDL_String& path) {
          if (fileName.GetLength() > 0)
            LoadSampleFromFile(fileName.Get());
        });
    }, "LOAD SAMPLE", loadBtnStyle), kCtrlTagLoadButton);

    // Sample name: top-right area
    const IRECT sampleBounds = headerRect.GetPadded(-8.f).GetFromRight(300.f).GetFromTop(headerH * 0.6f);
    pGraphics->AttachControl(new ITextControl(sampleBounds, "No sample loaded",
      IText(11, kColorTextSecondary, "Roboto-Regular", EAlign::Far, EVAlign::Middle)), kCtrlTagSampleName);

    // BPM display: bottom-right area — updated from ProcessBlock when host tempo changes
    const IRECT bpmBounds = headerRect.GetPadded(-8.f).GetFromRight(300.f).GetFromBottom(headerH * 0.4f);
    pGraphics->AttachControl(new ITextControl(bpmBounds, "BPM: —",
      IText(11, kColorTextMuted, "Roboto-Regular", EAlign::Far, EVAlign::Middle)), kCtrlTagBPMDisplay);

    // Version string (footer — right)
    const IRECT versionBounds = footerRect.GetPadded(-8.f).GetFromRight(200.f);
    pGraphics->AttachControl(new ITextControl(versionBounds, "RVRSE v" PLUG_VERSION_STR,
      IText(11, kColorTextMuted, "Roboto-Regular", EAlign::Far)), kCtrlTagVersionNumber);

    // ── Footer contents ─────────────────────────────────────────────────
    // Master Volume horizontal slider (left side of footer)
    const IVStyle sliderStyle = DEFAULT_STYLE
      .WithColor(kFG, kColorGold)
      .WithColor(kBG, IColor(0, 0, 0, 0))
      .WithColor(kFR, kColorKnobTrack)
      .WithColor(kX1, kColorGold)          // filled track color
      .WithColor(kHL, kColorGold.WithOpacity(0.15f))
      .WithDrawFrame(false)
      .WithDrawShadows(false)
      .WithEmboss(false)
      .WithShowLabel(true)
      .WithShowValue(true)
      .WithRoundness(1.f)
      .WithLabelText(IText(11, kColorTextSecondary, "Roboto-Regular", EAlign::Near, EVAlign::Middle))
      .WithValueText(IText(11, kColorTextSecondary, "Roboto-Regular", EAlign::Far, EVAlign::Middle));

    const IRECT masterSliderBounds = footerRect.GetPadded(-8.f).GetFromLeft(220.f).GetCentredInside(200.f, 30.f);
    pGraphics->AttachControl(new IVSliderControl(masterSliderBounds, kParamMasterVol,
      "VOLUME", sliderStyle, true, EDirection::Horizontal, DEFAULT_GEARING, 8.f, 2.f), kCtrlTagMasterVolSlider);

    // MIDI indicator (center of footer) — blue dot, lights up on MIDI input
    const IRECT midiBounds = footerRect.GetPadded(-8.f).GetMidHPadded(30.f);
    pGraphics->AttachControl(new ILambdaControl(midiBounds,
      [](ILambdaControl* pCaller, IGraphics& g, IRECT& r) {
        const float dotR = 4.f;
        const float cx = r.MW();
        const float cy = r.MH();
        // Dim blue dot (will brighten when MIDI active — wired later)
        g.FillCircle(rvrse::gui::kColorBlue.WithOpacity(0.3f), cx, cy, dotR);
        const IText label(9, rvrse::gui::kColorTextMuted, "Roboto-Regular", EAlign::Center, EVAlign::Top);
        IRECT textR = r.GetFromBottom(12.f);
        g.DrawText(label, "MIDI", textR);
      }, DEFAULT_ANIMATION_DURATION, false, false), kCtrlTagMidiIndicator);

    // ── Riser panel contents ────────────────────────────────────────────
    // Section label at top of riser panel
    const IRECT riserLabelBounds = riserRect.GetPadded(-8.f).GetFromTop(20.f);
    pGraphics->AttachControl(new ITextControl(riserLabelBounds, "RISER — REAL-TIME",
      IText(12, kColorGold, "Roboto-Bold", EAlign::Near, EVAlign::Middle)), kCtrlTagRiserSectionLabel);

    // Knob style: gold arc on dark track
    const IVStyle knobStyle = DEFAULT_STYLE
      .WithColor(kFG, kColorDarkGrey)
      .WithColor(kBG, IColor(0, 0, 0, 0))
      .WithColor(kFR, kColorKnobTrack)
      .WithColor(kX1, kColorGold)          // arc fill
      .WithColor(kHL, kColorGold.WithOpacity(0.1f))
      .WithDrawFrame(false)
      .WithDrawShadows(false)
      .WithEmboss(false)
      .WithShowLabel(true)
      .WithShowValue(true)
      .WithRoundness(1.f)
      .WithWidgetFrac(0.75f)
      .WithLabelText(IText(11, kColorTextSecondary, "Roboto-Regular", EAlign::Center, EVAlign::Bottom))
      .WithValueText(IText(10, kColorTextMuted, "Roboto-Regular", EAlign::Center, EVAlign::Top));

    // Stutter Rate + Depth — side by side in top portion of riser panel
    const IRECT knobArea = riserRect.GetPadded(-8.f).GetReducedFromTop(28.f).GetFromTop(90.f);
    const IRECT stutterRateBounds = knobArea.GetGridCell(0, 1, 2).GetCentredInside(75.f, 90.f);
    const IRECT stutterDepthBounds = knobArea.GetGridCell(1, 1, 2).GetCentredInside(75.f, 90.f);

    pGraphics->AttachControl(new IVKnobControl(stutterRateBounds, kParamStutterRate,
      "RATE", knobStyle, true), kCtrlTagStutterRate);
    pGraphics->AttachControl(new IVKnobControl(stutterDepthBounds, kParamStutterDepth,
      "DEPTH", knobStyle, true), kCtrlTagStutterDepth);

    // Restore sample name if already loaded
    if (!mSampleFilePath.empty())
    {
      if (auto* pCtrl = pGraphics->GetControlWithTag(kCtrlTagSampleName))
      {
        const auto state = mLoadState.load();
        if (state == rvrse::ESampleLoadState::Ready && mPlaySample && mPlaySample->IsLoaded())
        {
          WDL_String displayStr;
          displayStr.SetFormatted(256, "%s (%d Hz, %s, %.1fs)",
            mPlaySample->mFileName.c_str(),
            static_cast<int>(mPlaySample->mSampleRate),
            mPlaySample->mNumChannels == 1 ? "mono" : "stereo",
            static_cast<double>(mPlaySample->NumFrames()) / mPlaySample->mSampleRate);
          pCtrl->As<ITextControl>()->SetStr(displayStr.Get());
        }
        else if (state == rvrse::ESampleLoadState::Loading)
        {
          pCtrl->As<ITextControl>()->SetStr("Loading...");
        }
        else if (state == rvrse::ESampleLoadState::Error)
        {
          WDL_String errStr;
          errStr.SetFormatted(256, "Missing: %s",
            rvrse::ExtractFileName(mSampleFilePath).c_str());
          pCtrl->As<ITextControl>()->SetStr(errStr.Get());
        }
      }
    }
  };
#endif
}

#if IPLUG_EDITOR
void RVRSE::OnIdle()
{
  // Update BPM display when host tempo changes
  if (GetUI() && std::abs(mLastBPM - mLastDisplayedBPM) > 0.01)
  {
    mLastDisplayedBPM = mLastBPM;
    if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagBPMDisplay))
    {
      WDL_String bpmStr;
      if (mLastBPM > 0.0)
        bpmStr.SetFormatted(32, "BPM: %.1f", mLastBPM);
      else
        bpmStr.Set("BPM: —");
      pCtrl->As<ITextControl>()->SetStr(bpmStr.Get());
      pCtrl->SetDirty(false);
    }
  }
}
#endif

void RVRSE::LoadSampleFromFile(const char* filePath)
{
  mSampleFilePath = filePath;
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

      // Resample the hit to the DAW's output sample rate if they differ.
      // This ensures ProcessBlock can play it sample-by-sample at the correct pitch.
      const double outputSR = GetSampleRate();
      auto playSample = newSample;

      if (outputSR > 0.0 && std::abs(newSample->mSampleRate - outputSR) > 0.5)
      {
        auto resampled = std::make_shared<rvrse::SampleData>();
        rvrse::resampleLinearStereo(newSample->mLeft, newSample->mRight,
                                    newSample->mSampleRate, outputSR,
                                    resampled->mLeft, resampled->mRight);
        resampled->mSampleRate = outputSR;
        resampled->mNumChannels = newSample->mNumChannels;
        resampled->mFilePath = newSample->mFilePath;
        resampled->mFileName = newSample->mFileName;
        playSample = resampled;
      }

      {
        std::lock_guard<std::mutex> lock(mSampleMutex);
        mHitSample = playSample;
      }

      // Signal the audio thread that a new sample is ready
      mNewSampleReady.store(true, std::memory_order_release);
      mLoadState.store(rvrse::ESampleLoadState::Ready);

      // Feed the ORIGINAL sample into the offline pipeline (it resamples internally)
      mProcessor.setSample(newSample);

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

// --- State persistence (save/restore with DAW project) ---

static constexpr int kStateChunkVersion = 1;

bool RVRSE::SerializeState(IByteChunk& chunk) const
{
  chunk.Put(&kStateChunkVersion);
  chunk.PutStr(mSampleFilePath.c_str());
  return SerializeParams(chunk);
}

int RVRSE::UnserializeState(const IByteChunk& chunk, int startPos)
{
  int version = 0;
  startPos = chunk.Get(&version, startPos);
  if (startPos < 0) return startPos;

  if (version >= 1)
  {
    WDL_String pathStr;
    startPos = chunk.GetStr(pathStr, startPos);
    if (startPos < 0) return startPos;

    const std::string restoredPath(pathStr.Get());

    if (!restoredPath.empty())
    {
      if (std::filesystem::is_regular_file(restoredPath))
      {
        LoadSampleFromFile(restoredPath.c_str());
      }
      else
      {
        mSampleFilePath = restoredPath;
        mLoadState.store(rvrse::ESampleLoadState::Error);

        // Clear stale audio data so playback matches the "Missing" state
        {
          std::lock_guard<std::mutex> lock(mSampleMutex);
          mHitSample.reset();
        }
        mNewSampleReady.store(false, std::memory_order_release);

        if (GetUI())
        {
          if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagSampleName))
          {
            WDL_String errStr;
            errStr.SetFormatted(256, "Missing: %s",
              rvrse::ExtractFileName(restoredPath).c_str());
            pCtrl->As<ITextControl>()->SetStr(errStr.Get());
            pCtrl->SetDirty(false);
          }
        }
      }
    }
    else
    {
      // Empty path — host reset state/preset, clear everything
      mSampleFilePath.clear();
      mLoadState.store(rvrse::ESampleLoadState::Empty);
    }
  }

  return UnserializeParams(chunk, startPos);
}

#if IPLUG_DSP
void RVRSE::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const int nChans = NOutChansConnected();
  const double masterVol = GetParam(kParamMasterVol)->Value() / 100.0;
  const auto stutterRateHz = static_cast<float>(GetParam(kParamStutterRate)->Value());
  const float stutterDepth = static_cast<float>(GetParam(kParamStutterDepth)->Value());
  const float lush = static_cast<float>(GetParam(kParamLush)->Value() / 100.0);
  const int riserLengthIdx = static_cast<int>(GetParam(kParamRiserLength)->Value());
  const double riserLengthBeats = rvrse::kRiserLengthValues[
    std::clamp(riserLengthIdx, 0, rvrse::kNumRiserLengths - 1)];
  const float fadeInPct = static_cast<float>(GetParam(kParamFadeIn)->Value() / 100.0);
  const float riserVolumeGain = std::pow(10.0f, static_cast<float>(GetParam(kParamRiserVolume)->Value()) / 20.0f);
  const float hitVolumeGain = std::pow(10.0f, static_cast<float>(GetParam(kParamHitVolume)->Value()) / 20.0f);
  const auto debugStage = static_cast<rvrse::EDebugStage>(
    static_cast<int>(GetParam(kParamDebugStage)->Value()));
  const auto stretchQuality = static_cast<rvrse::EStretchQuality>(
    static_cast<int>(GetParam(kParamStretchQuality)->Value()));
  const double sr = GetSampleRate();

  // Read host BPM and propagate to the offline pipeline when it changes.
  // GetTempo() reads from the host-provided ITimeInfo (populated each block).
  const double hostBPM = GetTempo();
  const bool offline = GetRenderingOffline();
  if (hostBPM > 0.0 && std::abs(hostBPM - mLastBPM) > 0.01)
  {
    mLastBPM = hostBPM;
    mProcessor.setBPM(hostBPM, offline);
  }

  // Propagate Lush to the offline processor (triggers reverb rebuild)
  if (std::abs(lush - mLastLush) > 1e-4f)
  {
    mLastLush = lush;
    mProcessor.setLush(lush);
  }

  // Propagate Riser Length to the offline processor (triggers stretch rebuild).
  // Pass offline flag so the processor uses synchronous stretch during bounce.
  if (std::abs(riserLengthBeats - mLastRiserLength) > 1e-6)
  {
    mLastRiserLength = riserLengthBeats;
    mProcessor.setRiserLength(riserLengthBeats, offline);
  }

  // Propagate Stretch Quality (triggers stretch rebuild if changed)
  if (stretchQuality != mLastStretchQuality)
  {
    mLastStretchQuality = stretchQuality;
    mProcessor.setStretchQuality(stretchQuality, offline);
  }

  // Check if a new sample is ready from the loader thread (lock-free flag)
  if (mNewSampleReady.load(std::memory_order_acquire))
  {
    {
      std::lock_guard<std::mutex> lock(mSampleMutex);
      mPlaySample = mHitSample;
    }
    mNewSampleReady.store(false, std::memory_order_release);
    // Reset all voice state when new sample arrives
    mRiserPos = -1;
    mHitPos = -1;
    mSamplesFromNoteOn = -1;
  }

  // Check if a new riser buffer is ready from the offline pipeline (lock-free).
  // Latch: only swap when no riser is actively playing to avoid mid-note discontinuities.
  if (mProcessor.isNewRiserReady() && mRiserPos < 0)
  {
    mRiserBuffer = mProcessor.consumeRiser();
  }

  // Get local pointers — no further locking needed
  const auto& hit = mPlaySample;
  const auto& riser = mRiserBuffer;

  for (int s = 0; s < nFrames; s++)
  {
    // Process MIDI events at this sample offset
    while (!mMidiQueue.Empty() && mMidiQueue.Peek().mOffset <= s)
    {
      const IMidiMsg& msg = mMidiQueue.Peek();

      if (msg.StatusMsg() == IMidiMsg::kNoteOn && msg.Velocity() > 0)
      {
        // Note-on: start the riser from the beginning
        mVelocityGain = static_cast<float>(msg.Velocity()) / 127.0f;
        mRiserFadeRemaining = 0;
        mHitFadeRemaining = 0;
        rvrse::stutterReset(mStutterState);

        if (riser && riser->IsReady())
        {
          mRiserPos = 0;
          mHitPos = -1;

          // In debug modes, play the selected buffer; only trigger hit in Normal mode
          if (debugStage == rvrse::kDebugNormal)
          {
            mSamplesFromNoteOn = 0;
            mHitOffset = riser->NumFrames();
          }
          else
          {
            mSamplesFromNoteOn = -1; // No hit in debug modes
          }
        }
        else if (hit && hit->IsLoaded())
        {
          // No riser available yet — fall back to direct hit playback
          mRiserPos = -1;
          mSamplesFromNoteOn = -1;
          mHitPos = 0;
        }
      }
      else if (msg.StatusMsg() == IMidiMsg::kNoteOff ||
               (msg.StatusMsg() == IMidiMsg::kNoteOn && msg.Velocity() == 0))
      {
        // Note-off: begin fade-out on both voices and kill the hit trigger
        if (mRiserPos >= 0)
          mRiserFadeRemaining = mFadeOutLength;
        if (mHitPos >= 0)
          mHitFadeRemaining = mFadeOutLength;

        // Stop the hit trigger counter so the hit can't re-trigger after fade-out
        mSamplesFromNoteOn = -1;
      }

      else if (msg.StatusMsg() == IMidiMsg::kControlChange)
      {
        const IMidiMsg::EControlChangeMsg cc = msg.ControlChangeIdx();

        if (cc == static_cast<IMidiMsg::EControlChangeMsg>(rvrse::CC_STUTTER_RATE))
          GetParam(kParamStutterRate)->Set(msg.ControlChange(cc) * rvrse::kStutterRateMaxHz);
        else if (cc == static_cast<IMidiMsg::EControlChangeMsg>(rvrse::CC_STUTTER_DEPTH))
          GetParam(kParamStutterDepth)->Set(msg.ControlChange(cc));
      }

      mMidiQueue.Remove();
    }

    // --- Generate riser/debug output ---
    float riserL = 0.0f;
    float riserR = 0.0f;

    if (mRiserPos >= 0 && riser && riser->IsReady())
    {
      // Select the active buffer based on debug stage
      const float* bufL = riser->mLeft.data();
      const float* bufR = riser->mRight.data();
      int bufLen = riser->NumFrames();

      if (debugStage == rvrse::kDebugReverbed && riser->ReverbedFrames() > 0)
      {
        bufL = riser->mReverbedL.data();
        bufR = riser->mReverbedR.data();
        bufLen = riser->ReverbedFrames();
      }
      else if (debugStage == rvrse::kDebugReversed && riser->ReversedFrames() > 0)
      {
        bufL = riser->mReversedL.data();
        bufR = riser->mReversedR.data();
        bufLen = riser->ReversedFrames();
      }

      // Precompute fade-in length (constant for this buffer)
      const int fadeInLen = static_cast<int>(static_cast<float>(bufLen) * fadeInPct);

      if (mRiserPos < bufLen)
      {
        riserL = bufL[mRiserPos];
        riserR = bufR[mRiserPos];

        const bool isDebugRaw = (debugStage == rvrse::kDebugReverbed ||
                                 debugStage == rvrse::kDebugReversed);

        if (!isDebugRaw)
        {
          // Apply fade-in envelope over the first fadeInPct of the buffer
          if (fadeInPct > 0.0f && fadeInLen > 1 && mRiserPos < fadeInLen)
          {
            const float fadeGain = static_cast<float>(mRiserPos) / static_cast<float>(fadeInLen - 1);
            riserL *= fadeGain;
            riserR *= fadeGain;
          }

          riserL *= mVelocityGain * riserVolumeGain;
          riserR *= mVelocityGain * riserVolumeGain;

          // Apply stutter gate (per-sample, MIDI CC responsive)
          const float stutterGain = rvrse::stutterProcess(
            mStutterState, stutterRateHz, stutterDepth, sr);
          riserL *= stutterGain;
          riserR *= stutterGain;
        }
        else
        {
          // Debug raw modes: only velocity gain (no fade-in, stutter, or riser volume)
          riserL *= mVelocityGain;
          riserR *= mVelocityGain;
        }

        if (mRiserFadeRemaining > 0)
        {
          const float fadeGain = static_cast<float>(mRiserFadeRemaining) / static_cast<float>(mFadeOutLength);
          riserL *= fadeGain;
          riserR *= fadeGain;
          mRiserFadeRemaining--;
          if (mRiserFadeRemaining == 0) mRiserPos = -1;
        }

        if (mRiserPos >= 0) mRiserPos++;
      }
      else
      {
        // Buffer finished naturally
        mRiserPos = -1;
      }
    }

    // --- Check if it's time to fire the hit ---
    if (mSamplesFromNoteOn >= 0 && mHitPos < 0)
    {
      if (mSamplesFromNoteOn >= mHitOffset && hit && hit->IsLoaded())
      {
        mHitPos = 0; // Fire the hit!
      }
      mSamplesFromNoteOn++;
    }

    // --- Generate hit output ---
    float hitL = 0.0f;
    float hitR = 0.0f;

    if (mHitPos >= 0 && hit && hit->IsLoaded())
    {
      if (mHitPos < hit->NumFrames())
      {
        hitL = hit->mLeft[mHitPos] * mVelocityGain * hitVolumeGain;
        hitR = hit->mRight[mHitPos] * mVelocityGain * hitVolumeGain;

        if (mHitFadeRemaining > 0)
        {
          const float fadeGain = static_cast<float>(mHitFadeRemaining) / static_cast<float>(mFadeOutLength);
          hitL *= fadeGain;
          hitR *= fadeGain;
          mHitFadeRemaining--;
          if (mHitFadeRemaining == 0) mHitPos = -1;
        }

        if (mHitPos >= 0) mHitPos++;
      }
      else
      {
        // Hit finished naturally
        mHitPos = -1;
        mSamplesFromNoteOn = -1;
      }
    }

    // --- Mix and output (additive — both voices have independent volume) ---
    const float outL = (riserL + hitL) * static_cast<float>(masterVol);
    const float outR = (riserR + hitR) * static_cast<float>(masterVol);

    if (nChans >= 1) outputs[0][s] = outL;
    if (nChans >= 2) outputs[1][s] = outR;
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

  // Update the offline pipeline with the current sample rate
  mProcessor.setSampleRate(GetSampleRate());
}
#endif
