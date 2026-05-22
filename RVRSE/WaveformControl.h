#pragma once
/// @file WaveformControl.h
/// Custom waveform display for RVRSE — shows riser (gold) + hit (blue) with playhead.

#include "IControl.h"
#include "Constants.h"
#include "GUIColors.h"
#include "TrimUtils.h"
#include <functional>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace rvrse {

using namespace iplug;
using namespace igraphics;

/// Downsampled peak data for efficient waveform drawing.
struct WaveformPeaks
{
  std::vector<float> mMin; ///< Per-bin minimum (negative)
  std::vector<float> mMax; ///< Per-bin maximum (positive)
  int mNumBins = 0;

  void Clear() { mMin.clear(); mMax.clear(); mNumBins = 0; }
  bool IsEmpty() const { return mNumBins == 0; }
};

/// Custom IControl: dual waveform (riser + hit) with animated playhead.
class WaveformControl : public IControl
{
public:
  WaveformControl(const IRECT& bounds)
  : IControl(bounds)
  {
    SetWantsMidi(false);
  }

  /// Feed new riser data (call from main thread via OnIdle).
  void SetRiserData(const float* data, int numFrames)
  {
    Downsample(data, numFrames, mRiserPeaks);
    mRiserFrames = numFrames;
    SetDirty(false);
  }

  /// Feed new hit data (call from main thread via OnIdle).
  void SetHitData(const float* data, int numFrames)
  {
    Downsample(data, numFrames, mHitPeaks);
    mHitFrames = numFrames;
    SetDirty(false);
  }

  /// Set riser volume in dB for visual scaling.
  void SetRiserVolumeDb(float dB)
  {
    const float gain = std::pow(10.f, dB / 20.f);
    if (std::abs(gain - mRiserGain) > 0.001f)
    {
      mRiserGain = gain;
      SetDirty(false);
    }
  }

  /// Set hit volume in dB for visual scaling.
  void SetHitVolumeDb(float dB)
  {
    const float gain = std::pow(10.f, dB / 20.f);
    if (std::abs(gain - mHitGain) > 0.001f)
    {
      mHitGain = gain;
      SetDirty(false);
    }
  }

  /// Set fade-in fraction (0..1 of riser length) for visual envelope.
  void SetFadeInFrac(float frac)
  {
    if (std::abs(frac - mFadeInFrac) > 0.001f)
    {
      mFadeInFrac = std::clamp(frac, 0.f, 1.f);
      SetDirty(false);
    }
  }

  /// Set playhead position as fraction [0..1] of total (riser+hit). Negative = hidden.
  void SetPlayheadPos(float pos)
  {
    if (std::abs(pos - mPlayheadPos) > 0.0001f)
    {
      mPlayheadPos = pos;
      SetDirty(false);
    }
  }

  /// Set a callback invoked when a file is dropped onto the waveform display.
  /// @param fn  Called with the dropped file path. GUI thread only.
  void SetDropCallback(std::function<void(const char*)> fn)
  {
    mDropCallback = std::move(fn);
  }

  void OnDrop(const char* str) override
  {
    if (mDropCallback && str && str[0] != '\0')
      mDropCallback(str);
  }

  void Draw(IGraphics& g) override
  {
    const IRECT r = mRECT;
    const float w = r.W();
    const float h = r.H();
    const float midY = r.MH();

    const int totalFrames = mRiserFrames + mHitFrames;
    if (totalFrames == 0)
    {
      // Empty state — draw hint text
      const IText emptyText(16, gui::kColorTextSecondary, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
      g.DrawText(emptyText, "Load a sample or drop the sample file here to see the waveform", r);
      return;
    }

    // Compute split point: riser takes proportional width
    const float riserFrac = static_cast<float>(mRiserFrames) / static_cast<float>(totalFrames);
    const float splitX = r.L + w * riserFrac;

    // Shared peak across both waveforms for correct relative amplitude
    float sharedPeak = 0.001f;
    for (int i = 0; i < mRiserPeaks.mNumBins; ++i)
    {
      sharedPeak = std::max(sharedPeak, std::abs(mRiserPeaks.mMin[i]));
      sharedPeak = std::max(sharedPeak, std::abs(mRiserPeaks.mMax[i]));
    }
    for (int i = 0; i < mHitPeaks.mNumBins; ++i)
    {
      sharedPeak = std::max(sharedPeak, std::abs(mHitPeaks.mMin[i]));
      sharedPeak = std::max(sharedPeak, std::abs(mHitPeaks.mMax[i]));
    }

    // Draw riser waveform (gold) — with volume and fade-in envelope
    if (!mRiserPeaks.IsEmpty())
    {
      DrawWaveform(g, mRiserPeaks, IRECT(r.L, r.T, splitX, r.B),
                   gui::kColorGold, gui::kColorGold.WithOpacity(0.06f),
                   sharedPeak, mRiserGain, mFadeInFrac);
    }

    // Draw hit waveform (blue) — with volume scaling
    if (!mHitPeaks.IsEmpty())
    {
      DrawWaveform(g, mHitPeaks, IRECT(splitX, r.T, r.R, r.B),
                   gui::kColorBlue, gui::kColorBlue.WithOpacity(0.06f),
                   sharedPeak, mHitGain, 0.f);
    }

    // Separator line at split point
    if (mRiserFrames > 0 && mHitFrames > 0)
    {
      g.DrawLine(gui::kColorSeparator, splitX, r.T + 4.f, splitX, r.B - 4.f, nullptr, 1.f);
    }

    // Center line (zero axis)
    g.DrawLine(gui::kColorSeparator, r.L, midY, r.R, midY, nullptr, 0.5f);

    // Playhead
    if (mPlayheadPos >= 0.f && mPlayheadPos <= 1.f)
    {
      const float px = r.L + w * mPlayheadPos;
      g.DrawLine(gui::kColorHighlight, px, r.T, px, r.B, nullptr, 1.5f);
      // Small glow effect
      g.DrawLine(gui::kColorHighlight.WithOpacity(0.3f), px - 1.f, r.T, px - 1.f, r.B, nullptr, 1.f);
      g.DrawLine(gui::kColorHighlight.WithOpacity(0.3f), px + 1.f, r.T, px + 1.f, r.B, nullptr, 1.f);
    }

    // Section labels — bottom corners
    if (mRiserFrames > 0)
    {
      const IText riserLabel(11, gui::kColorGold.WithOpacity(0.4f), "Roboto-Regular", EAlign::Near, EVAlign::Middle);
      IRECT labelR(r.L + 8.f, r.B - 18.f, splitX - 4.f, r.B - 4.f);
      g.DrawText(riserLabel, "RISER", labelR);
    }
    if (mHitFrames > 0)
    {
      const IText hitLabel(11, gui::kColorBlue.WithOpacity(0.4f), "Roboto-Regular", EAlign::Far, EVAlign::Middle);
      IRECT labelR(splitX + 4.f, r.B - 18.f, r.R - 8.f, r.B - 4.f);
      g.DrawText(hitLabel, "HIT", labelR);
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    // No interaction — purely visual
  }

private:
  /// Downsample audio buffer into peak bins for the current control width.
  void Downsample(const float* data, int numFrames, WaveformPeaks& peaks)
  {
    const int numBins = static_cast<int>(mRECT.W());
    if (numBins <= 0 || numFrames <= 0 || !data)
    {
      peaks.Clear();
      return;
    }

    peaks.mMin.resize(numBins);
    peaks.mMax.resize(numBins);
    peaks.mNumBins = numBins;

    const float framesPerBin = static_cast<float>(numFrames) / static_cast<float>(numBins);

    for (int bin = 0; bin < numBins; ++bin)
    {
      const int startFrame = static_cast<int>(bin * framesPerBin);
      const int endFrame = std::min(std::max(startFrame + 1, static_cast<int>((bin + 1) * framesPerBin)), numFrames);

      float lo = 0.f, hi = 0.f;
      for (int f = startFrame; f < endFrame; ++f)
      {
        lo = std::min(lo, data[f]);
        hi = std::max(hi, data[f]);
      }
      peaks.mMin[bin] = lo;
      peaks.mMax[bin] = hi;
    }
  }

  /// Draw a waveform from peak data within the given rect.
  /// @param normPeak   Shared peak for normalization (correct relative amplitude)
  /// @param gain       Volume multiplier (1.0 = unity)
  /// @param fadeInFrac Fade-in as fraction of total width (0 = none)
  void DrawWaveform(IGraphics& g, const WaveformPeaks& peaks, const IRECT& area,
                    const IColor& fillColor, const IColor& bgColor,
                    float normPeak, float gain = 1.f, float fadeInFrac = 0.f) const
  {
    const float w = area.W();
    const float h = area.H();
    const float midY = area.MH();
    const float halfH = h * 0.38f;
    const int numPx = static_cast<int>(w);
    if (numPx <= 0 || peaks.mNumBins <= 0) return;

    const float binsPerPixel = static_cast<float>(peaks.mNumBins) / static_cast<float>(numPx);
    const float fadeInPx = fadeInFrac * w;

    // Pass 1: per-pixel min/max scanning ALL bins that map to each pixel
    thread_local std::vector<float> pxMin, pxMax;
    pxMin.resize(numPx);
    pxMax.resize(numPx);

    for (int px = 0; px < numPx; ++px)
    {
      const int binStart = static_cast<int>(px * binsPerPixel);
      int binEnd = static_cast<int>((px + 1) * binsPerPixel);
      if (binEnd <= binStart) binEnd = binStart + 1;
      if (binEnd > peaks.mNumBins) binEnd = peaks.mNumBins;

      float lo = 0.f, hi = 0.f;
      for (int b = binStart; b < binEnd; ++b)
      {
        lo = std::min(lo, peaks.mMin[b]);
        hi = std::max(hi, peaks.mMax[b]);
      }
      pxMin[px] = lo;
      pxMax[px] = hi;
    }

    // Pass 2: draw with gain and fade-in.
    // Only apply 3-pixel smoothing when the section is wide enough that
    // individual bins are ~1 pixel each. When narrow (high binsPerPixel),
    // smoothing destroys sharp transients.
    const bool smooth = binsPerPixel < 1.5f;

    for (int px = 0; px < numPx; ++px)
    {
      float lo = pxMin[px];
      float hi = pxMax[px];

      if (smooth)
      {
        int count = 1;
        if (px > 0) { lo += pxMin[px - 1]; hi += pxMax[px - 1]; ++count; }
        if (px < numPx - 1) { lo += pxMin[px + 1]; hi += pxMax[px + 1]; ++count; }
        lo /= static_cast<float>(count);
        hi /= static_cast<float>(count);
      }

      float minVal = (lo / normPeak) * gain;
      float maxVal = (hi / normPeak) * gain;

      if (fadeInPx > 0.f && px < static_cast<int>(fadeInPx))
      {
        const float env = static_cast<float>(px) / fadeInPx;
        minVal *= env;
        maxVal *= env;
      }

      minVal = std::clamp(minVal, -1.f, 1.f);
      maxVal = std::clamp(maxVal, -1.f, 1.f);

      const float x = area.L + static_cast<float>(px);
      const float y1 = midY - maxVal * halfH;
      const float y2 = midY - minVal * halfH;

      g.DrawLine(bgColor, x, midY - halfH, x, midY + halfH, nullptr, 1.f);
      g.DrawLine(fillColor, x, y1, x, y2, nullptr, 1.f);
    }
  }

  WaveformPeaks mRiserPeaks;
  WaveformPeaks mHitPeaks;
  int mRiserFrames = 0;
  int mHitFrames = 0;
  float mPlayheadPos = -1.f; ///< 0..1 fraction, negative = hidden
  float mRiserGain = 1.f;    ///< Visual volume scaling for riser
  float mHitGain = 1.f;      ///< Visual volume scaling for hit
  float mFadeInFrac = 0.f;   ///< Fade-in as fraction of riser length
  std::function<void(const char*)> mDropCallback; ///< Called on file drop (GUI thread only)
};

/// Lightweight hit sample waveform preview for the hit panel.
class HitPreviewControl : public IControl
{
public:
  HitPreviewControl(const IRECT& bounds)
  : IControl(bounds)
  {
    SetWantsMidi(false);
  }

  void SetData(const float* data, int numFrames, double sampleRate)
  {
    mNumFrames = numFrames;
    mSampleRate = sampleRate;
    const int numBins = static_cast<int>(mRECT.W());
    if (numBins <= 0 || numFrames <= 0 || !data)
    {
      mPeaks.Clear();
      mNumFrames = 0;
      mSampleRate = 0.0;
      mHoveredHandle = EHandle::None;
      mDraggingHandle = EHandle::None;
      SetDirty(false);
      return;
    }

    mPeaks.mMin.resize(numBins);
    mPeaks.mMax.resize(numBins);
    mPeaks.mNumBins = numBins;

    const float framesPerBin = static_cast<float>(numFrames) / static_cast<float>(numBins);
    for (int bin = 0; bin < numBins; ++bin)
    {
      const int s = static_cast<int>(bin * framesPerBin);
      const int e = std::min(std::max(s + 1, static_cast<int>((bin + 1) * framesPerBin)), numFrames);
      float lo = 0.f, hi = 0.f;
      for (int f = s; f < e; ++f)
      {
        lo = std::min(lo, data[f]);
        hi = std::max(hi, data[f]);
      }
      mPeaks.mMin[bin] = lo;
      mPeaks.mMax[bin] = hi;
    }
    SetDirty(false);
  }

  void SetCommittedTrimMs(double trimStartMs, double trimEndMs)
  {
    if (std::abs(trimStartMs - mCommittedTrimStartMs) < 0.5 &&
        std::abs(trimEndMs - mCommittedTrimEndMs) < 0.5)
    {
      return;
    }

    mCommittedTrimStartMs = trimStartMs;
    mCommittedTrimEndMs = trimEndMs;
    if (mDraggingHandle == EHandle::None)
    {
      mDraftTrimStartMs = trimStartMs;
      mDraftTrimEndMs = trimEndMs;
    }
    SetDirty(false);
  }

  void SetEditable(bool editable)
  {
    if (mEditable == editable)
      return;

    mEditable = editable;
    if (!mEditable)
    {
      mHoveredHandle = EHandle::None;
      mDraggingHandle = EHandle::None;
    }
    SetDirty(false);
  }

  void SetTrimCommitCallback(std::function<void(double, double)> fn)
  {
    mTrimCommitCallback = std::move(fn);
  }

  void Draw(IGraphics& g) override
  {
    const IRECT r = mRECT;
    const auto trimRange = CurrentTrimRange();
    const float startX = FrameToX(trimRange.mStartFrame);
    const float endX = FrameToX(trimRange.mEndFrameExclusive);

    // Dark background fill
    g.FillRect(IColor(255, 0x0A, 0x10, 0x1A), r);
    // Subtle border
    g.DrawRect(gui::kColorSeparator, r, nullptr, 1.f);

    if (mPeaks.IsEmpty())
    {
      const IText emptyText(11, gui::kColorTextMuted, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
      g.DrawText(emptyText, "No sample", r);
      return;
    }

    const float w = r.W();
    const float midY = r.MH();
    const float halfH = r.H() * 0.40f;
    const int numPx = static_cast<int>(w);

    float peak = 0.001f;
    for (int i = 0; i < mPeaks.mNumBins; ++i)
    {
      peak = std::max(peak, std::abs(mPeaks.mMin[i]));
      peak = std::max(peak, std::abs(mPeaks.mMax[i]));
    }

    const float bpp = static_cast<float>(mPeaks.mNumBins) / static_cast<float>(numPx);

    for (int px = 0; px < numPx; ++px)
    {
      const int binStart = static_cast<int>(px * bpp);
      int binEnd = static_cast<int>((px + 1) * bpp);
      if (binEnd <= binStart) binEnd = binStart + 1;
      if (binEnd > mPeaks.mNumBins) binEnd = mPeaks.mNumBins;

      float lo = 0.f, hi = 0.f;
      for (int b = binStart; b < binEnd; ++b)
      {
        lo = std::min(lo, mPeaks.mMin[b]);
        hi = std::max(hi, mPeaks.mMax[b]);
      }

      const float minVal = std::clamp(lo / peak, -1.f, 1.f);
      const float maxVal = std::clamp(hi / peak, -1.f, 1.f);

      const float x = r.L + static_cast<float>(px);
      const float y1 = midY - maxVal * halfH;
      const float y2 = midY - minVal * halfH;
      const bool active = (x >= startX && x <= endX);
      const IColor fillColor = active ? gui::kColorBlue : gui::kColorTextMuted.WithOpacity(0.65f);
      const IColor bgColor = active ? gui::kColorBlue.WithOpacity(0.06f) : gui::kColorTextMuted.WithOpacity(0.04f);

      g.DrawLine(bgColor, x, midY - halfH, x, midY + halfH, nullptr, 1.f);
      g.DrawLine(fillColor, x, y1, x, y2, nullptr, 1.f);
    }

    // Zero-axis line
    g.DrawLine(gui::kColorSeparator, r.L, midY, r.R, midY, nullptr, 0.5f);

    DrawTrimHandle(g, startX, EHandle::Start);
    DrawTrimHandle(g, endX, EHandle::End);

    if (mDraggingHandle != EHandle::None)
      DrawReadout(g, mDraggingHandle == EHandle::Start ? startX : endX);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    if (!mEditable)
      return;

    mDraggingHandle = HitTestHandle(x, y);
    if (mDraggingHandle == EHandle::None)
      return;

    mHoveredHandle = mDraggingHandle;
    mDraftTrimStartMs = mCommittedTrimStartMs;
    mDraftTrimEndMs = mCommittedTrimEndMs;
    UpdateDraftFromPosition(x);
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    (void) y;
    (void) dX;
    (void) dY;
    (void) mod;
    if (mDraggingHandle == EHandle::None || !mEditable)
      return;

    UpdateDraftFromPosition(x);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    (void) x;
    (void) y;
    (void) mod;
    if (mDraggingHandle == EHandle::None)
      return;

    const double committedStartMs = mDraftTrimStartMs;
    const double committedEndMs = mDraftTrimEndMs;
    mDraggingHandle = EHandle::None;
    mCommittedTrimStartMs = committedStartMs;
    mCommittedTrimEndMs = committedEndMs;
    if (mTrimCommitCallback)
      mTrimCommitCallback(committedStartMs, committedEndMs);
    SetDirty(false);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    if (!mEditable)
      return;

    const EHandle handle = HitTestHandle(x, y);
    if (handle == EHandle::None)
      return;

    if (handle == EHandle::Start)
      mCommittedTrimStartMs = 0.0;
    else
      mCommittedTrimEndMs = 0.0;

    mDraftTrimStartMs = mCommittedTrimStartMs;
    mDraftTrimEndMs = mCommittedTrimEndMs;
    if (mTrimCommitCallback)
      mTrimCommitCallback(mCommittedTrimStartMs, mCommittedTrimEndMs);
    SetDirty(false);
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    if (!mEditable || mDraggingHandle != EHandle::None)
      return;

    const EHandle hovered = HitTestHandle(x, y);
    if (hovered != mHoveredHandle)
    {
      mHoveredHandle = hovered;
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    if (mDraggingHandle == EHandle::None && mHoveredHandle != EHandle::None)
    {
      mHoveredHandle = EHandle::None;
      SetDirty(false);
    }
  }

private:
  enum class EHandle
  {
    None = 0,
    Start,
    End
  };

  static constexpr float kHandleHitRadius = 10.0f;
  static constexpr float kHandleCapWidth = 10.0f;
  static constexpr float kHandleCapHeight = 8.0f;

  TrimRangeFrames CurrentTrimRange() const
  {
    return ResolveTrimRangeFrames(
      mNumFrames, mSampleRate,
      mDraggingHandle == EHandle::None ? mCommittedTrimStartMs : mDraftTrimStartMs,
      mDraggingHandle == EHandle::None ? mCommittedTrimEndMs : mDraftTrimEndMs,
      kTrimMinRegionMs
    );
  }

  float FrameToX(int frame) const
  {
    if (mNumFrames <= 0)
      return mRECT.L;

    const float frac = static_cast<float>(std::clamp(frame, 0, mNumFrames)) / static_cast<float>(mNumFrames);
    return mRECT.L + frac * mRECT.W();
  }

  double TotalDurationMs() const
  {
    return FramesToTrimMs(mNumFrames, mSampleRate);
  }

  EHandle HitTestHandle(float x, float y) const
  {
    if (!mEditable || mNumFrames <= 0 || mSampleRate <= 0.0)
      return EHandle::None;

    const auto trimRange = CurrentTrimRange();
    const float startX = FrameToX(trimRange.mStartFrame);
    const float endX = FrameToX(trimRange.mEndFrameExclusive);
    const float top = mRECT.T;
    const float bottom = mRECT.B;

    if (y >= top && y <= bottom)
    {
      if (std::abs(x - startX) <= kHandleHitRadius)
        return EHandle::Start;
      if (std::abs(x - endX) <= kHandleHitRadius)
        return EHandle::End;
    }

    return EHandle::None;
  }

  void UpdateDraftFromPosition(float x)
  {
    const double totalMs = TotalDurationMs();
    if (totalMs <= 0.0)
      return;

    const double clampedX = std::clamp<double>(x, mRECT.L, mRECT.R);
    const double posMs = ((clampedX - mRECT.L) / mRECT.W()) * totalMs;

    if (mDraggingHandle == EHandle::Start)
    {
      const double maxStartMs = std::max(0.0, totalMs - mDraftTrimEndMs - kTrimMinRegionMs);
      mDraftTrimStartMs = std::round(std::clamp(posMs, 0.0, maxStartMs));
    }
    else if (mDraggingHandle == EHandle::End)
    {
      const double trimEndMs = totalMs - posMs;
      const double maxEndMs = std::max(0.0, totalMs - mDraftTrimStartMs - kTrimMinRegionMs);
      mDraftTrimEndMs = std::round(std::clamp(trimEndMs, 0.0, maxEndMs));
    }

    SetDirty(false);
  }

  void DrawTrimHandle(IGraphics& g, float x, EHandle handle) const
  {
    const bool highlighted = (mDraggingHandle == handle) || (mHoveredHandle == handle && mEditable);
    const IColor color = highlighted ? gui::kColorHighlight : gui::kColorBlue.WithOpacity(0.9f);
    const float clampedX = std::clamp(x, mRECT.L, mRECT.R);
    g.DrawLine(color, clampedX, mRECT.T + 2.f, clampedX, mRECT.B - 2.f, nullptr, highlighted ? 2.f : 1.25f);
    const IRECT cap(clampedX - kHandleCapWidth * 0.5f, mRECT.T + 2.f,
                    clampedX + kHandleCapWidth * 0.5f, mRECT.T + 2.f + kHandleCapHeight);
    g.FillRoundRect(color, cap, 2.f);
  }

  void DrawReadout(IGraphics& g, float handleX) const
  {
    const double valueMs = (mDraggingHandle == EHandle::Start) ? mDraftTrimStartMs : mDraftTrimEndMs;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.0f ms", valueMs);

    const float bubbleW = 52.f;
    const float bubbleH = 18.f;
    const float x = std::clamp(handleX + 8.f, mRECT.L + 4.f, mRECT.R - bubbleW - 4.f);
    const float y = mRECT.T + 12.f;
    const IRECT bubble(x, y, x + bubbleW, y + bubbleH);
    g.FillRoundRect(gui::kColorDarkGrey.WithOpacity(0.95f), bubble, 4.f);
    g.DrawRoundRect(gui::kColorBlue.WithOpacity(0.8f), bubble, 4.f, nullptr, 1.f);
    g.DrawText(IText(11, gui::kColorTextPrimary, "Roboto-Regular", EAlign::Center, EVAlign::Middle), buffer, bubble);
  }

  WaveformPeaks mPeaks;
  int mNumFrames = 0;
  double mSampleRate = 0.0;
  double mCommittedTrimStartMs = 0.0;
  double mCommittedTrimEndMs = 0.0;
  double mDraftTrimStartMs = 0.0;
  double mDraftTrimEndMs = 0.0;
  bool mEditable = false;
  EHandle mHoveredHandle = EHandle::None;
  EHandle mDraggingHandle = EHandle::None;
  std::function<void(double, double)> mTrimCommitCallback;
};

} // namespace rvrse
