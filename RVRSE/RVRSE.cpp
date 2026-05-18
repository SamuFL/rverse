#include "RVRSE.h"
#include "IPlug_include_in_plug_src.h"
#include "BufferUtils.h"
#include "Constants.h"
#include "SampleLoader.h"
#include "WaveformControl.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <thread>

#if IPLUG_EDITOR
#include "IControls.h"
#include "GUIColors.h"
#endif

#if IPLUG_EDITOR
/// Full-window background control that handles drag-and-drop file loading.
/// Attached first (bottom of Z-order) so all other controls receive mouse
/// clicks normally — iPlug2 hit-tests from front to back, so knobs, buttons,
/// and panels on top always win for clicks. This acts as a fallback drop
/// target for areas not handled by a frontmost hit-tested control; controls
/// above it may intercept drops if they implement their own OnDrop handling.
///
/// GUI THREAD ONLY — never called from the audio thread.
class DropBackgroundControl final : public IControl
{
public:
  /// @param bounds   Full plugin window bounds.
  /// @param plugin   Non-owning pointer to the plugin instance (for RequestSampleLoadFromUI).
  DropBackgroundControl(const IRECT& bounds, RVRSE* plugin)
  : IControl(bounds), mPlugin(plugin)
  {
    SetWantsMidi(false);
  }

  void Draw(IGraphics& g) override
  {
    // Intentionally empty — the real background is drawn by the panel controls above us.
  }

  void OnDrop(const char* str) override
  {
    if (!str || str[0] == '\0') return;

    if (!rvrse::IsSupportedAudioFile(str))
    {
      mPlugin->ShowUnsupportedFormatError(rvrse::GetAudioFileLoadError(str).c_str());
      return;
    }

    mPlugin->RequestSampleLoadFromUI(str);
  }

private:
  RVRSE* mPlugin; ///< Non-owning; lifetime guaranteed by plugin > GUI
};

class TransportButtonControl final : public IButtonControlBase
{
public:
  enum class EIcon
  {
    Play = 0,
    Stop
  };

  TransportButtonControl(const IRECT& bounds, EIcon icon, const IColor& accent, IActionFunction aF)
  : IButtonControlBase(bounds, aF)
  , mIcon(icon)
  , mAccent(accent)
  {
    SetWantsMidi(false);
  }

  void Draw(IGraphics& g) override
  {
    const bool active = GetValue() > 0.5;
    const bool disabled = IsDisabled();
    const float contrast = disabled ? -GRAYED_ALPHA : 0.f;
    const IRECT buttonRect = mRECT.GetPadded(-1.f);

    IColor fill = active ? mAccent.WithOpacity(0.9f) : rvrse::gui::kColorDarkGrey.WithOpacity(0.45f);
    if (!active && mMouseIsOver && !disabled)
      fill = mAccent.WithOpacity(0.18f);

    const IColor frame = mAccent.WithOpacity(active ? 1.f : 0.7f).WithContrast(contrast);
    const IColor icon = (active ? rvrse::gui::kColorDark : mAccent).WithContrast(contrast);

    g.FillRoundRect(fill.WithContrast(contrast), buttonRect, 5.f);
    g.DrawRoundRect(frame, buttonRect, 5.f, nullptr, 1.25f);

    const float iconSide = std::max(8.f, buttonRect.H() - 10.f);
    const IRECT iconRect = buttonRect.GetCentredInside(iconSide, iconSide);
    if (mIcon == EIcon::Play)
    {
      const float x1 = iconRect.L + iconRect.W() * 0.22f;
      const float y1 = iconRect.T + iconRect.H() * 0.14f;
      const float x2 = iconRect.L + iconRect.W() * 0.22f;
      const float y2 = iconRect.B - iconRect.H() * 0.14f;
      const float x3 = iconRect.R - iconRect.W() * 0.1f;
      const float y3 = iconRect.MH();
      g.FillTriangle(icon, x1, y1, x2, y2, x3, y3);
    }
    else
    {
      const float stopSide = iconRect.W() * 0.62f;
      g.FillRect(icon, iconRect.GetCentredInside(stopSide, stopSide));
    }
  }

private:
  EIcon mIcon;
  IColor mAccent;
};
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
    const float gap      = kZoneGap;
    // Header and footer never shrink below their design size
    const float headerH  = std::max(kHeaderHeight, kHeaderHeight * scaleY);
    const float footerH  = std::max(kFooterHeight, kFooterHeight * scaleY);
    // Panels need a minimum height for their fixed-layout knobs/labels
    constexpr float kMinPanelH = 246.f;
    // At or above default size: waveform scales proportionally.
    // When shrinking: waveform absorbs squeeze to protect panels.
    const float fixedH   = headerH + footerH + gap * 4.f;
    const float proportionalWaveH = kWaveformHeight * scaleY;
    const float maxWaveH = h - fixedH - kMinPanelH;       // what's left after panels get their min
    const float waveH    = std::max(40.f, std::min(proportionalWaveH, maxWaveH));

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
    const auto waveformDisplayBounds = [](const IRECT& rect) {
      return rect.GetPadded(-6.f);
    };
    const auto waveformTransportBounds = [](const IRECT& rect) {
      return rect.GetPadded(-6.f).GetFromBottom(38.f).GetVShifted(-5.f);
    };
    const auto waveformTransportBackgroundBounds = [](const IRECT& rect) {
      return rect.GetCentredInside(132.f, 30.f);
    };
    const auto transportButtonBounds = [](const IRECT& rect, int idx) {
      constexpr float kButtonWidth = 48.f;
      constexpr float kButtonHeight = 24.f;
      constexpr float kButtonGap = 10.f;
      const IRECT cluster = rect.GetCentredInside(kButtonWidth * 2.f + kButtonGap, kButtonHeight);
      return (idx == 0) ? cluster.GetFromLeft(kButtonWidth) : cluster.GetFromRight(kButtonWidth);
    };

    // ── Resize path — reposition existing controls ─────────────────────
    if (pGraphics->NControls()) {
      pGraphics->GetBackgroundControl()->SetTargetAndDrawRECTs(b);
      pGraphics->GetControlWithTag(kCtrlTagHeaderPanel)->SetTargetAndDrawRECTs(headerRect);
      pGraphics->GetControlWithTag(kCtrlTagWaveformPanel)->SetTargetAndDrawRECTs(waveformRect);
      pGraphics->GetControlWithTag(kCtrlTagWaveformDisplay)->SetTargetAndDrawRECTs(waveformDisplayBounds(waveformRect));
      const IRECT transportArea = waveformTransportBounds(waveformRect);
      pGraphics->GetControlWithTag(kCtrlTagPreviewTransportBg)->SetTargetAndDrawRECTs(waveformTransportBackgroundBounds(transportArea));
      pGraphics->GetControlWithTag(kCtrlTagPreviewPlay)->SetTargetAndDrawRECTs(transportButtonBounds(transportArea, 0));
      pGraphics->GetControlWithTag(kCtrlTagPreviewStop)->SetTargetAndDrawRECTs(transportButtonBounds(transportArea, 1));
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
      const IRECT masterArea = footerRect.GetPadded(-8.f).GetFromLeft(220.f);
      const IRECT masterLabelRow = masterArea.GetFromTop(22.f);
      const IRECT masterSliderBounds = masterArea.GetReducedFromTop(20.f).GetCentredInside(200.f, 24.f);
      pGraphics->GetControlWithTag(kCtrlTagMasterVolLabel)->SetTargetAndDrawRECTs(masterLabelRow.GetFromLeft(110.f));
      pGraphics->GetControlWithTag(kCtrlTagMasterVolValue)->SetTargetAndDrawRECTs(masterLabelRow.GetFromRight(110.f));
      pGraphics->GetControlWithTag(kCtrlTagMasterVolSlider)->SetTargetAndDrawRECTs(masterSliderBounds);
      const IRECT midiBounds = footerRect.GetPadded(-8.f).GetMidHPadded(30.f);
      pGraphics->GetControlWithTag(kCtrlTagMidiIndicator)->SetTargetAndDrawRECTs(midiBounds);
      // Riser panel contents
      const IRECT riserLabelBounds = riserRect.GetPadded(-8.f).GetFromTop(20.f);
      pGraphics->GetControlWithTag(kCtrlTagRiserSectionLabel)->SetTargetAndDrawRECTs(riserLabelBounds);
      const IRECT knobArea = riserRect.GetPadded(-8.f).GetReducedFromTop(28.f).GetFromTop(110.f);
      const IRECT stutterRateBounds = knobArea.GetGridCell(0, 1, 3).GetCentredInside(95.f, 110.f);
      pGraphics->GetControlWithTag(kCtrlTagStutterRate)->SetTargetAndDrawRECTs(stutterRateBounds);
      const IRECT stutterDepthBounds = knobArea.GetGridCell(1, 1, 3).GetCentredInside(95.f, 110.f);
      pGraphics->GetControlWithTag(kCtrlTagStutterDepth)->SetTargetAndDrawRECTs(stutterDepthBounds);
      const IRECT riserVolBounds = knobArea.GetGridCell(2, 1, 3).GetCentredInside(95.f, 110.f);
      pGraphics->GetControlWithTag(kCtrlTagRiserVolume)->SetTargetAndDrawRECTs(riserVolBounds);
      // Offline section
      const IRECT offlineArea = riserRect.GetPadded(-8.f).GetReducedFromTop(144.f);
      const IRECT offlineLabelBounds = offlineArea.GetFromTop(18.f);
      pGraphics->GetControlWithTag(kCtrlTagOfflineSectionLabel)->SetTargetAndDrawRECTs(offlineLabelBounds);
      const IRECT offlineKnobRow = offlineArea.GetReducedFromTop(22.f).GetFromTop(80.f);
      pGraphics->GetControlWithTag(kCtrlTagLush)->SetTargetAndDrawRECTs(
        offlineKnobRow.GetGridCell(0, 1, 4).GetCentredInside(60.f, 80.f));
      pGraphics->GetControlWithTag(kCtrlTagRiserLength)->SetTargetAndDrawRECTs(
        offlineKnobRow.GetGridCell(1, 1, 4).GetCentredInside(60.f, 80.f));
      pGraphics->GetControlWithTag(kCtrlTagFadeIn)->SetTargetAndDrawRECTs(
        offlineKnobRow.GetGridCell(2, 1, 4).GetCentredInside(60.f, 80.f));
      const IRECT stretchBounds = offlineKnobRow.GetGridCell(3, 1, 4).GetCentredInside(90.f, 50.f);
      pGraphics->GetControlWithTag(kCtrlTagStretchQuality)->SetTargetAndDrawRECTs(stretchBounds);
      // Hit panel
      const IRECT hitLabelBounds = hitRect.GetPadded(-8.f).GetFromTop(20.f);
      pGraphics->GetControlWithTag(kCtrlTagHitSectionLabel)->SetTargetAndDrawRECTs(hitLabelBounds);
      const IRECT hitVolArea = hitRect.GetPadded(-8.f).GetReducedFromTop(28.f).GetFromTop(110.f);
      pGraphics->GetControlWithTag(kCtrlTagHitVolume)->SetTargetAndDrawRECTs(
        hitVolArea.GetCentredInside(95.f, 110.f));
      const IRECT hitPreviewArea = hitRect.GetPadded(-10.f)
        .GetReducedFromTop(140.f).GetReducedFromBottom(84.f);
      pGraphics->GetControlWithTag(kCtrlTagHitPreview)->SetTargetAndDrawRECTs(hitPreviewArea);
      const IRECT bottomArea = hitRect.GetPadded(-10.f).GetFromBottom(80.f);
      const IRECT logoArea = bottomArea.GetFromRight(120.f);
      pGraphics->GetControlWithTag(kCtrlTagLogo)->SetTargetAndDrawRECTs(logoArea);
      pGraphics->GetControlWithTag(kCtrlTagSupportButton)->SetTargetAndDrawRECTs(
        bottomArea.GetReducedFromLeft(8.f).GetFromLeft(140.f).GetCentredInside(130.f, 26.f));
      pGraphics->GetControlWithTag(kCtrlTagDropOverlay)->SetTargetAndDrawRECTs(b);
      return;
    }

    // ── First-time setup ───────────────────────────────────────────────
    pGraphics->SetLayoutOnResize(true);
    pGraphics->AttachCornerResizer(EUIResizerMode::Size, true);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Roboto-Bold", ROBOTO_BOLD_FN);

    // Reset waveform tracking so OnIdle re-feeds data to the new controls
    mWaveformLastRiser.reset();
    mWaveformLastHit.reset();

    // Main background
    pGraphics->AttachPanelBackground(kColorDark);

    // Drop background — attached immediately after the panel background (bottom of Z-order).
    // iPlug2 hit-tests front-to-back, so all controls above this receive clicks first.
    // Drops that land anywhere on the window reach this control via OnDrop.
    pGraphics->AttachControl(new DropBackgroundControl(b, this), kCtrlTagDropOverlay);

    // Helper: draw a filled rounded rect panel
    auto MakeRoundedPanel = [](const IRECT& bounds, const IColor& color, float radius) {
      return new ILambdaControl(bounds, [color, radius](ILambdaControl* pCaller, IGraphics& g, IRECT& r) {
        g.FillRoundRect(color, r, radius);
      }, DEFAULT_ANIMATION_DURATION, false, false);
    };

    // Zone panels — rounded rects for waveform, riser, hit panels
    pGraphics->AttachControl(new IPanelControl(headerRect, kColorHeaderBg), kCtrlTagHeaderPanel);
    pGraphics->AttachControl(MakeRoundedPanel(waveformRect, kColorWaveformBg, 6.f), kCtrlTagWaveformPanel);
    pGraphics->AttachControl(new rvrse::WaveformControl(waveformDisplayBounds(waveformRect)), kCtrlTagWaveformDisplay);
    const IRECT transportArea = waveformTransportBounds(waveformRect);
    pGraphics->AttachControl(new ILambdaControl(
      waveformTransportBackgroundBounds(transportArea),
      [](ILambdaControl* pCaller, IGraphics& g, IRECT& r) {
        g.FillRoundRect(kColorDark.WithOpacity(0.82f), r, 7.f);
        g.DrawRoundRect(kColorSeparator.WithOpacity(0.9f), r, 7.f, nullptr, 1.f);
      }, DEFAULT_ANIMATION_DURATION, false, false), kCtrlTagPreviewTransportBg);
    pGraphics->GetControlWithTag(kCtrlTagPreviewTransportBg)->SetIgnoreMouse(true);
    pGraphics->AttachControl(new TransportButtonControl(
      transportButtonBounds(transportArea, 0), TransportButtonControl::EIcon::Play, kColorGold,
      [this](IControl* pCaller) {
        mPreviewCommandQueue.Push({PreviewCommand::EType::Play, rvrse::kPreviewTriggerMidiNote, rvrse::kPreviewTriggerVelocity});
        DefaultClickActionFunc(pCaller);
      }), kCtrlTagPreviewPlay);
    pGraphics->AttachControl(new TransportButtonControl(
      transportButtonBounds(transportArea, 1), TransportButtonControl::EIcon::Stop, kColorBlue,
      [this](IControl* pCaller) {
        mPreviewCommandQueue.Push({PreviewCommand::EType::Stop, rvrse::kPreviewTriggerMidiNote, 0});
        DefaultClickActionFunc(pCaller);
      }), kCtrlTagPreviewStop);
    pGraphics->GetControlWithTag(kCtrlTagPreviewPlay)->SetDisabled(true);
    pGraphics->GetControlWithTag(kCtrlTagPreviewStop)->SetDisabled(true);

    // Wire drag-and-drop into the waveform display — most intuitive drop target.
    if (auto* pWave = static_cast<rvrse::WaveformControl*>(pGraphics->GetControlWithTag(kCtrlTagWaveformDisplay)))
    {
      pWave->SetDropCallback([this](const char* str) {
        if (!str || str[0] == '\0') return;
        if (!rvrse::IsSupportedAudioFile(str))
        {
          ShowUnsupportedFormatError(rvrse::GetAudioFileLoadError(str).c_str());
          return;
        }
        RequestSampleLoadFromUI(str);
      });
    }
    pGraphics->AttachControl(MakeRoundedPanel(riserRect, kColorDarkGrey, 6.f), kCtrlTagRiserPanel);
    pGraphics->AttachControl(MakeRoundedPanel(hitRect, kColorDarkGrey, 6.f), kCtrlTagHitPanel);
    pGraphics->AttachControl(new IPanelControl(footerRect, kColorHeaderBg), kCtrlTagFooterPanel);

    // ── Header contents ────────────────────────────────────────────────
    // Title: top-left, bold gold
    const IRECT titleBounds = headerRect.GetPadded(-8.f).GetFromLeft(300.f).GetFromTop(headerH * 0.65f);
    pGraphics->AttachControl(new ITextControl(titleBounds, "RVRSE",
      IText(44, kColorGold, "Roboto-Bold", EAlign::Near, EVAlign::Middle)), kCtrlTagTitle);

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
      .WithLabelText(IText(15, kColorGold, "Roboto-Regular", EAlign::Center, EVAlign::Middle));

    pGraphics->AttachControl(new IVButtonControl(loadBtnBounds, [this](IControl* pCaller) {
      WDL_String fileName;
      WDL_String path;
      pCaller->GetUI()->PromptForFile(fileName, path, EFileAction::Open,
        rvrse::kSupportedAudioExts,
        [this](const WDL_String& fileName, const WDL_String& path) {
          if (fileName.GetLength() > 0)
          {
            std::filesystem::path selectedPath(fileName.Get());

            if (!selectedPath.is_absolute() && path.GetLength() > 0)
              selectedPath = std::filesystem::path(path.Get()) / selectedPath;

            const std::string normalizedPath = selectedPath.lexically_normal().string();
            RequestSampleLoadFromUI(normalizedPath.c_str());
          }
        });
    }, "LOAD SAMPLE", loadBtnStyle), kCtrlTagLoadButton);

    // Sample name: top-right area
    const IRECT sampleBounds = headerRect.GetPadded(-8.f).GetFromRight(300.f).GetFromTop(headerH * 0.6f);
    pGraphics->AttachControl(new ITextControl(sampleBounds, "No sample loaded",
      IText(15, kColorTextPrimary, "Roboto-Regular", EAlign::Far, EVAlign::Middle)), kCtrlTagSampleName);

    // BPM display: bottom-right area — updated from ProcessBlock when host tempo changes
    const IRECT bpmBounds = headerRect.GetPadded(-8.f).GetFromRight(300.f).GetFromBottom(headerH * 0.4f);
    pGraphics->AttachControl(new ITextControl(bpmBounds, "BPM: —",
      IText(14, kColorTextSecondary, "Roboto-Regular", EAlign::Far, EVAlign::Middle)), kCtrlTagBPMDisplay);

    // Version string (footer — right)
    const IRECT versionBounds = footerRect.GetPadded(-8.f).GetFromRight(200.f);
    pGraphics->AttachControl(new ITextControl(versionBounds, "RVRSE v" PLUG_VERSION_STR,
      IText(13, kColorTextPrimary, "Roboto-Regular", EAlign::Far)), kCtrlTagVersionNumber);

    // ── Footer contents ─────────────────────────────────────────────────
    // Master Volume horizontal slider (left side of footer)
    // Master volume: label + value above, slider below
    const IRECT masterArea = footerRect.GetPadded(-8.f).GetFromLeft(220.f);
    const IRECT masterLabelRow = masterArea.GetFromTop(22.f);
    const IRECT masterSliderBounds = masterArea.GetReducedFromTop(20.f).GetCentredInside(200.f, 24.f);

    pGraphics->AttachControl(new ITextControl(masterLabelRow.GetFromLeft(110.f), "VOLUME",
      IText(14, kColorTextPrimary, "Roboto-Regular", EAlign::Center, EVAlign::Middle)), kCtrlTagMasterVolLabel);
    pGraphics->AttachControl(new ITextControl(masterLabelRow.GetFromRight(110.f), "100%",
      IText(14, kColorTextSecondary, "Roboto-Regular", EAlign::Center, EVAlign::Middle)), kCtrlTagMasterVolValue);

    const IVStyle sliderStyle = DEFAULT_STYLE
      .WithColor(kFG, kColorGold)
      .WithColor(kBG, IColor(0, 0, 0, 0))
      .WithColor(kFR, kColorKnobTrack)
      .WithColor(kX1, kColorGold)
      .WithColor(kHL, kColorGold.WithOpacity(0.15f))
      .WithDrawFrame(false)
      .WithDrawShadows(false)
      .WithEmboss(false)
      .WithShowLabel(false)
      .WithShowValue(false)
      .WithRoundness(1.f);

    pGraphics->AttachControl(new IVSliderControl(masterSliderBounds, kParamMasterVol,
      "", sliderStyle, true, EDirection::Horizontal, DEFAULT_GEARING, 8.f, 2.f), kCtrlTagMasterVolSlider);

    // MIDI indicator (center of footer) — blue dot, lights up on MIDI input
    const IRECT midiBounds = footerRect.GetPadded(-8.f).GetMidHPadded(30.f);
    pGraphics->AttachControl(new ILambdaControl(midiBounds,
      [](ILambdaControl* pCaller, IGraphics& g, IRECT& r) {
        const float dotR = 4.f;
        const float cx = r.MW();
        const float cy = r.MH();
        const float opacity = 0.15f + 0.85f * static_cast<float>(pCaller->GetValue());
        g.FillCircle(rvrse::gui::kColorBlue.WithOpacity(opacity), cx, cy, dotR);
        const IText label(11, rvrse::gui::kColorTextPrimary, "Roboto-Regular", EAlign::Center, EVAlign::Top);
        IRECT textR = r.GetFromBottom(12.f);
        g.DrawText(label, "MIDI", textR);
      }, DEFAULT_ANIMATION_DURATION, false, false), kCtrlTagMidiIndicator);

    // ── Riser panel contents ────────────────────────────────────────────
    // Section label at top of riser panel
    const IRECT riserLabelBounds = riserRect.GetPadded(-8.f).GetFromTop(20.f);
    pGraphics->AttachControl(new ITextControl(riserLabelBounds, "RISER — REAL-TIME",
      IText(17, kColorGold, "Roboto-Bold", EAlign::Near, EVAlign::Middle)), kCtrlTagRiserSectionLabel);

    // Knob style: solid gold body for real-time knobs
    const IColor kKnobBodyGold {255, 0x7A, 0x68, 0x28}; // rich warm gold
    const IVStyle knobStyle = DEFAULT_STYLE
      .WithColor(kFG, kKnobBodyGold)
      .WithColor(kBG, IColor(0, 0, 0, 0))
      .WithColor(kFR, IColor(255, 0x4A, 0x3A, 0x10)) // dark brown outline — visible against bright gold
      .WithColor(kX1, kColorGold)          // arc fill
      .WithColor(kHL, kColorGold)           // bright gold body on press
      .WithColor(kPR, kColorGold)
      .WithDrawFrame(true)
      .WithFrameThickness(1.f)
      .WithDrawShadows(false)
      .WithEmboss(false)
      .WithShowLabel(true)
      .WithShowValue(true)
      .WithRoundness(1.f)
      .WithWidgetFrac(0.75f)
      .WithLabelText(IText(15, kColorTextPrimary, "Roboto-Regular", EAlign::Center, EVAlign::Bottom))
      .WithValueText(IText(14, kColorTextSecondary, "Roboto-Regular", EAlign::Center, EVAlign::Top));

    // Stutter Rate + Depth + Riser Volume — real-time knobs, prominently sized
    const IRECT knobArea = riserRect.GetPadded(-8.f).GetReducedFromTop(28.f).GetFromTop(110.f);
    const IRECT stutterRateBounds = knobArea.GetGridCell(0, 1, 3).GetCentredInside(95.f, 110.f);
    const IRECT stutterDepthBounds = knobArea.GetGridCell(1, 1, 3).GetCentredInside(95.f, 110.f);
    const IRECT riserVolBounds = knobArea.GetGridCell(2, 1, 3).GetCentredInside(95.f, 110.f);

    pGraphics->AttachControl(new IVKnobControl(stutterRateBounds, kParamStutterRate,
      "RATE", knobStyle, true), kCtrlTagStutterRate);
    pGraphics->AttachControl(new IVKnobControl(stutterDepthBounds, kParamStutterDepth,
      "DEPTH", knobStyle, true), kCtrlTagStutterDepth);
    pGraphics->AttachControl(new IVKnobControl(riserVolBounds, kParamRiserVolume,
      "VOLUME", knobStyle, true), kCtrlTagRiserVolume);

    // ── Riser panel: offline section ────────────────────────────────────
    const IRECT offlineArea = riserRect.GetPadded(-8.f).GetReducedFromTop(144.f);

    // Offline section label
    const IRECT offlineLabelBounds = offlineArea.GetFromTop(18.f);
    pGraphics->AttachControl(new ITextControl(offlineLabelBounds, "OFFLINE",
      IText(17, kColorSteel, "Roboto-Bold", EAlign::Near, EVAlign::Middle)), kCtrlTagOfflineSectionLabel);

    // Steel knob style for offline params (smaller widget)
    const IColor kKnobBodyWhite {255, 0x6A, 0x70, 0x78}; // cool white-grey tint
    const IVStyle offlineKnobStyle = knobStyle
      .WithColor(kFG, kKnobBodyWhite)
      .WithColor(kFR, IColor(255, 0x80, 0x88, 0x90)) // light silver ring
      .WithColor(kX1, kColorSteel)
      .WithColor(kHL, kColorWhite)          // white on press
      .WithColor(kPR, kColorWhite)
      .WithWidgetFrac(0.8f);

    // Row of offline knobs: Lush | Length | Fade In | [Stretch toggle]
    const IRECT offlineKnobRow = offlineArea.GetReducedFromTop(22.f).GetFromTop(80.f);
    const IRECT lushBounds    = offlineKnobRow.GetGridCell(0, 1, 4).GetCentredInside(60.f, 80.f);
    const IRECT lengthBounds  = offlineKnobRow.GetGridCell(1, 1, 4).GetCentredInside(60.f, 80.f);
    const IRECT fadeInBounds  = offlineKnobRow.GetGridCell(2, 1, 4).GetCentredInside(60.f, 80.f);

    pGraphics->AttachControl(new IVKnobControl(lushBounds, kParamLush,
      "LUSH", offlineKnobStyle, true), kCtrlTagLush);
    pGraphics->AttachControl(new IVKnobControl(lengthBounds, kParamRiserLength,
      "LENGTH", offlineKnobStyle, true), kCtrlTagRiserLength);
    pGraphics->AttachControl(new IVKnobControl(fadeInBounds, kParamFadeIn,
      "FADE IN", offlineKnobStyle, true), kCtrlTagFadeIn);

    // Stretch Quality — horizontal tab switch (HIGH | LOW)
    const IVStyle toggleStyle = DEFAULT_STYLE
      .WithColor(kFG, kColorDarkGrey)
      .WithColor(kBG, IColor(0, 0, 0, 0))
      .WithColor(kFR, kColorSteel)
      .WithColor(kHL, kColorSteel.WithOpacity(0.2f))
      .WithColor(kX1, kColorSteel)
      .WithDrawFrame(true)
      .WithFrameThickness(1.f)
      .WithDrawShadows(false)
      .WithEmboss(false)
      .WithRoundness(0.3f)
      .WithShowLabel(true)
      .WithWidgetFrac(0.55f)
      .WithLabelText(IText(11, kColorTextPrimary, "Roboto-Regular", EAlign::Center, EVAlign::Bottom))
      .WithValueText(IText(12, kColorTextSecondary, "Roboto-Regular", EAlign::Center, EVAlign::Middle));

    const IRECT stretchBounds = offlineKnobRow.GetGridCell(3, 1, 4).GetCentredInside(90.f, 50.f);
    pGraphics->AttachControl(new IVTabSwitchControl(stretchBounds, kParamStretchQuality,
      {"HIGH", "LOW"}, "QUALITY", toggleStyle, EVShape::Rectangle, EDirection::Horizontal), kCtrlTagStretchQuality);

    // ── Hit panel contents ──────────────────────────────────────────────
    // Section label
    const IRECT hitLabelBounds = hitRect.GetPadded(-8.f).GetFromTop(20.f);
    pGraphics->AttachControl(new ITextControl(hitLabelBounds, "HIT",
      IText(17, kColorBlue, "Roboto-Bold", EAlign::Near, EVAlign::Middle)), kCtrlTagHitSectionLabel);

    // Hit Volume knob (blue accent — blue body)
    const IColor kKnobBodyBlue {255, 0x28, 0x4A, 0x7A}; // deep blue body
    const IVStyle hitKnobStyle = knobStyle
      .WithColor(kFG, kKnobBodyBlue)
      .WithColor(kFR, IColor(255, 0x10, 0x30, 0x58)) // dark blue outline
      .WithColor(kX1, kColorBlue)
      .WithColor(kHL, kColorBlue)
      .WithColor(kPR, kColorBlue);

    const IRECT hitVolArea = hitRect.GetPadded(-8.f).GetReducedFromTop(28.f).GetFromTop(110.f);
    const IRECT hitVolBounds = hitVolArea.GetCentredInside(95.f, 110.f);
    pGraphics->AttachControl(new IVKnobControl(hitVolBounds, kParamHitVolume,
      "VOLUME", hitKnobStyle, true), kCtrlTagHitVolume);

    // Hit waveform preview — between volume knob and bottom area
    const IRECT hitPreviewArea = hitRect.GetPadded(-10.f)
      .GetReducedFromTop(140.f)   // below volume knob
      .GetReducedFromBottom(84.f); // above logo/donate
    pGraphics->AttachControl(new rvrse::HitPreviewControl(hitPreviewArea), kCtrlTagHitPreview);

    // Logo (PNG bitmap) — lower-right
    const IBitmap logoBitmap = pGraphics->LoadBitmap(LOGO_FN);
    const IRECT bottomArea = hitRect.GetPadded(-10.f).GetFromBottom(80.f);
    const IRECT logoArea = bottomArea.GetFromRight(120.f);
    pGraphics->AttachControl(new IBitmapControl(logoArea.GetCentredInside(80.f, 80.f), logoBitmap), kCtrlTagLogo);

    // Donate button — lower-left, vertically centered with logo
    const IVStyle supportStyle = DEFAULT_STYLE
      .WithColor(kFG, kColorDarkGrey)
      .WithColor(kBG, IColor(0, 0, 0, 0))
      .WithColor(kPR, kColorDarkGrey)
      .WithColor(kFR, kColorBlue)
      .WithColor(kHL, kColorBlue.WithOpacity(0.1f))
      .WithDrawFrame(true)
      .WithFrameThickness(1.f)
      .WithDrawShadows(false)
      .WithEmboss(false)
      .WithRoundness(0.3f)
      .WithShowValue(false)
      .WithLabelText(IText(14, kColorBlue, "Roboto-Regular", EAlign::Center, EVAlign::Middle));

    const IRECT supportBounds = bottomArea.GetReducedFromLeft(8.f).GetFromLeft(140.f).GetCentredInside(130.f, 26.f);
    pGraphics->AttachControl(new IVButtonControl(supportBounds, [](IControl* pCaller) {
      pCaller->GetUI()->OpenURL("https://samufl.com/#/portal/support");
    }, "DONATE", supportStyle), kCtrlTagSupportButton);

    // Purely decorative/read-only controls should not swallow file drops.
    for (const int tag : {
           kCtrlTagTitle,
           kCtrlTagSampleName,
           kCtrlTagHeaderPanel,
           kCtrlTagWaveformPanel,
           kCtrlTagRiserPanel,
           kCtrlTagHitPanel,
           kCtrlTagFooterPanel,
           kCtrlTagVersionNumber,
           kCtrlTagMidiIndicator,
           kCtrlTagBPMDisplay,
           kCtrlTagRiserSectionLabel,
           kCtrlTagOfflineSectionLabel,
           kCtrlTagHitSectionLabel,
           kCtrlTagMasterVolLabel,
           kCtrlTagMasterVolValue,
           kCtrlTagHitPreview,
           kCtrlTagLogo
         })
    {
      if (auto* pCtrl = pGraphics->GetControlWithTag(tag))
        pCtrl->SetIgnoreMouse(true);
    }

    // Restore sample name if already loaded
    if (!mSampleFilePath.empty())
    {
      if (auto* pCtrl = pGraphics->GetControlWithTag(kCtrlTagSampleName))
      {
        const auto state = mLoadState.load();
        const auto playSample = std::atomic_load(&mPlaySample);

        if (state == rvrse::ESampleLoadState::Ready && playSample && playSample->IsLoaded())
        {
          WDL_String displayStr;
          displayStr.SetFormatted(256, "%s (%d Hz, %s, %.1fs)",
            playSample->mFileName.c_str(),
            static_cast<int>(playSample->mSampleRate),
            playSample->mNumChannels == 1 ? "mono" : "stereo",
            static_cast<double>(playSample->NumFrames()) / playSample->mSampleRate);
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

void RVRSE::ClearLoadedSampleState()
{
  mSampleFilePath.clear();

  {
    std::lock_guard<std::mutex> lock(mSampleMutex);
    mHitSample.reset();
  }

  std::atomic_store(&mPlaySample, std::shared_ptr<rvrse::SampleData> {});
  std::atomic_store(&mRiserBuffer, std::shared_ptr<rvrse::RiserData> {});
  mProcessor.setSample(nullptr);
  mNewSampleReady.store(false, std::memory_order_release);
  mLoadState.store(rvrse::ESampleLoadState::Empty);
  mRiserPos.store(-1, std::memory_order_relaxed);
  mHitPos.store(-1, std::memory_order_relaxed);
}

void RVRSE::QueueSampleLoadError(const char* errorMessage, bool clearLoadedState)
{
  std::lock_guard<std::mutex> lock(mStatusTextMutex);
  mPendingSampleStatusText = "No sample loaded";
  mPendingSampleAlertText = errorMessage ? errorMessage : "Unsupported format";
  mPendingSampleStateClear = clearLoadedState;
}

#if IPLUG_EDITOR
void RVRSE::OnIdle()
{
  // Drain CC→UI param updates queued by the audio thread
  ParamTuple p;
  while (mCCParamQueue.Pop(p))
  {
    SendParameterValueFromDelegate(p.idx, p.value, false);
  }

  // Update BPM display when host tempo changes
  const double currentBPM = mLastBPM.load(std::memory_order_relaxed);
  if (GetUI() && std::abs(currentBPM - mLastDisplayedBPM) > 0.01)
  {
    mLastDisplayedBPM = currentBPM;
    if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagBPMDisplay))
    {
      WDL_String bpmStr;
      if (currentBPM > 0.0)
        bpmStr.SetFormatted(32, "BPM: %.1f", currentBPM);
      else
        bpmStr.Set("BPM: —");
      pCtrl->As<ITextControl>()->SetStr(bpmStr.Get());
      pCtrl->SetDirty(false);
    }
  }

  // Update master volume display
  if (GetUI())
  {
    const double vol = GetParam(kParamMasterVol)->Value();
    if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagMasterVolValue))
    {
      WDL_String volStr;
      volStr.SetFormatted(16, "%.0f%%", vol);
      if (strcmp(pCtrl->As<ITextControl>()->GetStr(), volStr.Get()) != 0)
      {
        pCtrl->As<ITextControl>()->SetStr(volStr.Get());
        pCtrl->SetDirty(false);
      }
    }
  }

  // Update MIDI activity indicator
  if (GetUI())
  {
    const int counter = mMidiActivityCounter.load(std::memory_order_relaxed);
    if (counter != mMidiLastSeenCounter)
    {
      mMidiLastSeenCounter = counter;
      mMidiCooldownFrames = 8; // ~130ms at 60fps
    }
    if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagMidiIndicator))
    {
      const double val = (mMidiCooldownFrames > 0) ? 1.0 : 0.0;
      if (pCtrl->GetValue() != val)
      {
        pCtrl->SetValue(val);
        pCtrl->SetDirty(false);
      }
    }
    if (mMidiCooldownFrames > 0) mMidiCooldownFrames--;
  }

  // Publish any sample status text prepared by the background loader.
  if (GetUI())
  {
    std::string sampleStatusText;
    std::string sampleAlertText;
    bool clearSampleState = false;
    {
      std::lock_guard<std::mutex> lock(mStatusTextMutex);
      sampleStatusText = mPendingSampleStatusText;
      sampleAlertText = std::move(mPendingSampleAlertText);
      mPendingSampleAlertText.clear();
      clearSampleState = mPendingSampleStateClear;
      mPendingSampleStateClear = false;
    }

    if (clearSampleState)
      ClearLoadedSampleState();

    if (!sampleStatusText.empty() && sampleStatusText != mLastSampleStatusText)
    {
      if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagSampleName))
      {
        pCtrl->As<ITextControl>()->SetStr(sampleStatusText.c_str());
        pCtrl->SetDirty(false);
        mLastSampleStatusText = std::move(sampleStatusText);
      }
    }

    if (!sampleAlertText.empty())
      GetUI()->ShowMessageBox(sampleAlertText.c_str(), "Sample Load Error", kMB_OK);
  }

  if (GetUI())
  {
    const auto playbackHit = std::atomic_load(&mPlaySample);
    const bool hasPlayableSample = mLoadState.load(std::memory_order_relaxed) == rvrse::ESampleLoadState::Ready &&
                                   playbackHit && playbackHit->IsLoaded();
    const bool playbackActive = mRiserPos.load(std::memory_order_relaxed) >= 0 ||
                                mHitPos.load(std::memory_order_relaxed) >= 0;

    if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagPreviewPlay))
    {
      const bool shouldDisable = !hasPlayableSample;
      if (pCtrl->IsDisabled() != shouldDisable)
        pCtrl->SetDisabled(shouldDisable);

      const double activeValue = playbackActive ? 1.0 : 0.0;
      if (std::abs(pCtrl->GetValue() - activeValue) > 1e-6)
      {
        pCtrl->SetValue(activeValue);
        pCtrl->SetDirty(false);
      }
    }

    if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagPreviewStop))
    {
      const bool shouldDisable = !playbackActive;
      if (pCtrl->IsDisabled() != shouldDisable)
        pCtrl->SetDisabled(shouldDisable);

      if (std::abs(pCtrl->GetValue()) > 1e-6)
      {
        pCtrl->SetValue(0.0);
        pCtrl->SetDirty(false);
      }
    }
  }

  // Update waveform display
  if (GetUI())
  {
    auto* pWaveform = dynamic_cast<rvrse::WaveformControl*>(
      GetUI()->GetControlWithTag(kCtrlTagWaveformDisplay));
    auto* pHitPreview = dynamic_cast<rvrse::HitPreviewControl*>(
      GetUI()->GetControlWithTag(kCtrlTagHitPreview));
    if (pWaveform)
    {
      const auto riser = std::atomic_load(&mRiserBuffer);
      const auto playbackHit = std::atomic_load(&mPlaySample);
      std::shared_ptr<rvrse::SampleData> hit;

      {
        std::lock_guard<std::mutex> lock(mSampleMutex);
        hit = mHitSample;
      }

      // Feed riser data when pointer changes
      if (riser && riser->IsReady() && riser != mWaveformLastRiser)
      {
        const int nFrames = riser->NumFrames();
        mWaveformMonoBuf.resize(nFrames);
        for (int i = 0; i < nFrames; ++i)
          mWaveformMonoBuf[i] = (riser->mLeft[i] + riser->mRight[i]) * 0.5f;
        pWaveform->SetRiserData(mWaveformMonoBuf.data(), nFrames);
        mWaveformLastRiser = riser;
      }
      else if (!riser && mWaveformLastRiser)
      {
        pWaveform->SetRiserData(nullptr, 0);
        mWaveformLastRiser.reset();
      }

      // Feed hit data when pointer changes
      if (hit && hit->IsLoaded() && hit != mWaveformLastHit)
      {
        const int nFrames = hit->NumFrames();
        mWaveformMonoBuf.resize(nFrames);
        for (int i = 0; i < nFrames; ++i)
          mWaveformMonoBuf[i] = (hit->mLeft[i] + (hit->mNumChannels > 1 ? hit->mRight[i] : hit->mLeft[i])) * 0.5f;
        pWaveform->SetHitData(mWaveformMonoBuf.data(), nFrames);

        // Also feed the hit preview control
        if (pHitPreview)
          pHitPreview->SetData(mWaveformMonoBuf.data(), nFrames);

        mWaveformLastHit = hit;
      }
      else if (!hit && mWaveformLastHit)
      {
        pWaveform->SetHitData(nullptr, 0);
        if (pHitPreview)
          pHitPreview->SetData(nullptr, 0);
        mWaveformLastHit.reset();
      }

      // Update visual volume and fade-in envelope
      pWaveform->SetRiserVolumeDb(static_cast<float>(GetParam(kParamRiserVolume)->Value()));
      pWaveform->SetHitVolumeDb(static_cast<float>(GetParam(kParamHitVolume)->Value()));
      pWaveform->SetFadeInFrac(static_cast<float>(GetParam(kParamFadeIn)->Value()) / 100.f);

      // Update playhead position
      if (riser || playbackHit)
      {
        const int riserFrames = riser ? riser->NumFrames() : 0;
        const int hitFrames = playbackHit ? playbackHit->NumFrames() : 0;
        const int totalFrames = riserFrames + hitFrames;
        if (totalFrames > 0)
        {
          float pos = -1.f;
          const int riserPos = mRiserPos.load(std::memory_order_relaxed);
          const int hitPos = mHitPos.load(std::memory_order_relaxed);
          if (riserPos >= 0 && riser)
            pos = static_cast<float>(riserPos) / static_cast<float>(totalFrames);
          else if (hitPos >= 0 && playbackHit)
            pos = static_cast<float>(riserFrames + hitPos) / static_cast<float>(totalFrames);
          pWaveform->SetPlayheadPos(pos);
        }
        else
        {
          pWaveform->SetPlayheadPos(-1.f);
        }
      }
      else
      {
        pWaveform->SetPlayheadPos(-1.f);
      }
    }
  }
}
#endif

#if IPLUG_EDITOR
void RVRSE::ShowUnsupportedFormatError(const char* errorMessage)
{
  if (!GetUI()) return;

  ClearLoadedSampleState();
  QueueSampleLoadError(errorMessage);
}
#endif

void RVRSE::LoadSampleFromFile(const char* filePath)
{
  mSampleFilePath = filePath;
  mLoadState.store(rvrse::ESampleLoadState::Loading);
  {
    std::lock_guard<std::mutex> lock(mStatusTextMutex);
    mPendingSampleStatusText = "Loading...";
    mLastSampleStatusText.clear();
  }

  // Update UI to show loading state
  if (GetUI())
  {
    if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagSampleName))
    {
      pCtrl->As<ITextControl>()->SetStr("Loading...");
      pCtrl->SetDirty(false);
    }
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

      {
        WDL_String displayStr;
        displayStr.SetFormatted(256, "%s (%d Hz, %s, %.1fs)",
          newSample->mFileName.c_str(),
          static_cast<int>(newSample->mSampleRate),
          newSample->mNumChannels == 1 ? "mono" : "stereo",
          static_cast<double>(newSample->NumFrames()) / newSample->mSampleRate);

        std::lock_guard<std::mutex> lock(mStatusTextMutex);
        mPendingSampleStatusText = displayStr.Get();
      }
    }
    else
    {
      QueueSampleLoadError(result.errorMessage.c_str(), true);
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
        ClearLoadedSampleState();
        mSampleFilePath = restoredPath;
        mLoadState.store(rvrse::ESampleLoadState::Error);

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
      ClearLoadedSampleState();
    }
  }

  return UnserializeParams(chunk, startPos);
}

#if IPLUG_DSP
void RVRSE::TriggerPlayback(int& riserPos,
                            int& hitPos,
                            const std::shared_ptr<rvrse::RiserData>& riser,
                            const std::shared_ptr<rvrse::SampleData>& hit,
                            int debugStage,
                            int velocity)
{
  mVelocityGain = std::clamp(static_cast<float>(velocity) / 127.0f, 0.0f, 1.0f);
  mRiserFadeRemaining = 0;
  mHitFadeRemaining = 0;
  rvrse::stutterReset(mStutterState);

  if (riser && riser->IsReady())
  {
    riserPos = 0;
    hitPos = -1;

#ifndef NDEBUG
    if (debugStage == rvrse::kDebugNormal)
    {
      mSamplesFromNoteOn = 0;
      mHitOffset = riser->mBeatAlignedFrames;
    }
    else
    {
      mSamplesFromNoteOn = -1;
    }
#else
    mSamplesFromNoteOn = 0;
    mHitOffset = riser->mBeatAlignedFrames;
#endif
  }
  else if (hit && hit->IsLoaded())
  {
    riserPos = -1;
    mSamplesFromNoteOn = -1;
    hitPos = 0;
  }
}

void RVRSE::StopPlayback(int& riserPos, int& hitPos)
{
  if (riserPos >= 0)
    mRiserFadeRemaining = mFadeOutLength;
  if (hitPos >= 0)
    mHitFadeRemaining = mFadeOutLength;

  mSamplesFromNoteOn = -1;
}

void RVRSE::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const int nChans = NOutChansConnected();
  const double masterVol = GetParam(kParamMasterVol)->Value() / 100.0;
  auto stutterRateHz = static_cast<float>(GetParam(kParamStutterRate)->Value());
  float stutterDepth = static_cast<float>(GetParam(kParamStutterDepth)->Value());
  const float lush = static_cast<float>(GetParam(kParamLush)->Value() / 100.0);
  const int riserLengthIdx = static_cast<int>(GetParam(kParamRiserLength)->Value());
  const double riserLengthBeats = rvrse::kRiserLengthValues[
    std::clamp(riserLengthIdx, 0, rvrse::kNumRiserLengths - 1)];
  const float fadeInPct = static_cast<float>(GetParam(kParamFadeIn)->Value() / 100.0);
  const float riserVolumeGain = std::pow(10.0f, static_cast<float>(GetParam(kParamRiserVolume)->Value()) / 20.0f);
  const float hitVolumeGain = std::pow(10.0f, static_cast<float>(GetParam(kParamHitVolume)->Value()) / 20.0f);
#ifndef NDEBUG
  const auto debugStage = static_cast<rvrse::EDebugStage>(
    static_cast<int>(GetParam(kParamDebugStage)->Value()));
#else
  constexpr auto debugStage = rvrse::kDebugNormal;
#endif
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
      std::atomic_store(&mPlaySample, mHitSample);
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
    std::atomic_store(&mRiserBuffer, mProcessor.consumeRiser());
  }

  // Get local pointers — no further locking needed
  const auto hit = std::atomic_load(&mPlaySample);
  const auto riser = std::atomic_load(&mRiserBuffer);

  // Local copies of atomic positions for the per-sample loop (avoids atomic
  // loads on every access; written back after the loop for UI thread visibility)
  int riserPos = mRiserPos.load(std::memory_order_relaxed);
  int hitPos   = mHitPos.load(std::memory_order_relaxed);

  PreviewCommand previewCommand;
  while (mPreviewCommandQueue.Pop(previewCommand))
  {
    if (previewCommand.type == PreviewCommand::EType::Play)
      TriggerPlayback(riserPos, hitPos, riser, hit, static_cast<int>(debugStage), previewCommand.velocity);
    else
      StopPlayback(riserPos, hitPos);
  }

  for (int s = 0; s < nFrames; s++)
  {
    // Process MIDI events at this sample offset
    while (!mMidiQueue.Empty() && mMidiQueue.Peek().mOffset <= s)
    {
      const IMidiMsg& msg = mMidiQueue.Peek();

      if (msg.StatusMsg() == IMidiMsg::kNoteOn && msg.Velocity() > 0)
      {
        TriggerPlayback(riserPos, hitPos, riser, hit, static_cast<int>(debugStage), msg.Velocity());
      }
      else if (msg.StatusMsg() == IMidiMsg::kNoteOff ||
               (msg.StatusMsg() == IMidiMsg::kNoteOn && msg.Velocity() == 0))
      {
        StopPlayback(riserPos, hitPos);
      }

      else if (msg.StatusMsg() == IMidiMsg::kControlChange)
      {
        const IMidiMsg::EControlChangeMsg cc = msg.ControlChangeIdx();

        if (cc == static_cast<IMidiMsg::EControlChangeMsg>(rvrse::CC_STUTTER_RATE))
        {
          const double val = msg.ControlChange(cc) * rvrse::kStutterRateMaxHz;
          GetParam(kParamStutterRate)->Set(val);
          mCCParamQueue.Push({kParamStutterRate, val});
          stutterRateHz = static_cast<float>(val);
        }
        else if (cc == static_cast<IMidiMsg::EControlChangeMsg>(rvrse::CC_STUTTER_DEPTH))
        {
          const double val = msg.ControlChange(cc);
          GetParam(kParamStutterDepth)->Set(val);
          mCCParamQueue.Push({kParamStutterDepth, val});
          stutterDepth = static_cast<float>(val);
        }
      }

      mMidiQueue.Remove();
    }

    // --- Generate riser/debug output ---
    float riserL = 0.0f;
    float riserR = 0.0f;

    if (riserPos >= 0 && riser && riser->IsReady())
    {
      const float* bufL = riser->mLeft.data();
      const float* bufR = riser->mRight.data();
      int bufLen = riser->NumFrames();

#ifndef NDEBUG
      // Select the active buffer based on debug stage
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
#endif

      // Precompute fade-in length (constant for this buffer)
      const int fadeInLen = static_cast<int>(static_cast<float>(bufLen) * fadeInPct);

      if (riserPos < bufLen)
      {
        riserL = bufL[riserPos];
        riserR = bufR[riserPos];

#ifndef NDEBUG
        const bool isDebugRaw = (debugStage == rvrse::kDebugReverbed ||
                                 debugStage == rvrse::kDebugReversed);
#else
        constexpr bool isDebugRaw = false;
#endif

        if (!isDebugRaw)
        {
          // Apply fade-in envelope over the first fadeInPct of the buffer
          if (fadeInPct > 0.0f && fadeInLen > 1 && riserPos < fadeInLen)
          {
            const float fadeGain = static_cast<float>(riserPos) / static_cast<float>(fadeInLen - 1);
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
          if (mRiserFadeRemaining == 0) riserPos = -1;
        }

        if (riserPos >= 0) riserPos++;
      }
      else
      {
        // Buffer finished naturally
        riserPos = -1;
      }
    }

    // --- Check if it's time to fire the hit ---
    if (mSamplesFromNoteOn >= 0 && hitPos < 0)
    {
      if (mSamplesFromNoteOn >= mHitOffset && hit && hit->IsLoaded())
      {
        hitPos = 0; // Fire the hit!
      }
      mSamplesFromNoteOn++;
    }

    // --- Generate hit output ---
    float hitL = 0.0f;
    float hitR = 0.0f;

    if (hitPos >= 0 && hit && hit->IsLoaded())
    {
      if (hitPos < hit->NumFrames())
      {
        hitL = hit->mLeft[hitPos] * mVelocityGain * hitVolumeGain;
        hitR = hit->mRight[hitPos] * mVelocityGain * hitVolumeGain;

        if (mHitFadeRemaining > 0)
        {
          const float fadeGain = static_cast<float>(mHitFadeRemaining) / static_cast<float>(mFadeOutLength);
          hitL *= fadeGain;
          hitR *= fadeGain;
          mHitFadeRemaining--;
          if (mHitFadeRemaining == 0) hitPos = -1;
        }

        if (hitPos >= 0) hitPos++;
      }
      else
      {
        // Hit finished naturally
        hitPos = -1;
        mSamplesFromNoteOn = -1;
      }
    }

    // --- Mix and output (additive — both voices have independent volume) ---
    const float outL = (riserL + hitL) * static_cast<float>(masterVol);
    const float outR = (riserR + hitR) * static_cast<float>(masterVol);

    if (nChans >= 1) outputs[0][s] = outL;
    if (nChans >= 2) outputs[1][s] = outR;
  }

  // Write back local position copies so UI thread can read them
  mRiserPos.store(riserPos, std::memory_order_relaxed);
  mHitPos.store(hitPos, std::memory_order_relaxed);

  mMidiQueue.Flush(nFrames);
}

void RVRSE::ProcessMidiMsg(const IMidiMsg& msg)
{
  ++mMidiActivityCounter;
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
