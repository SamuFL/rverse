#pragma once
/// @file WaveformControl.h
/// Custom waveform display for RVRSE — shows riser (gold) + hit (blue) with playhead.

#include "IControl.h"
#include "GUIColors.h"
#include <vector>
#include <algorithm>
#include <cmath>

using namespace iplug;
using namespace igraphics;

namespace rvrse {

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

  void Draw(IGraphics& g) override
  {
    const IRECT r = mRECT;
    const float w = r.W();
    const float h = r.H();
    const float midY = r.MH();

    const int totalFrames = mRiserFrames + mHitFrames;
    if (totalFrames == 0)
    {
      // Empty state — draw label
      const IText emptyText(14, gui::kColorTextMuted, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
      g.DrawText(emptyText, "Load a sample to see the waveform", r);
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
      const int endFrame = std::min(static_cast<int>((bin + 1) * framesPerBin), numFrames);

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

  void SetData(const float* data, int numFrames)
  {
    const int numBins = static_cast<int>(mRECT.W());
    if (numBins <= 0 || numFrames <= 0 || !data)
    {
      mPeaks.Clear();
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
      const int e = std::min(static_cast<int>((bin + 1) * framesPerBin), numFrames);
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

  void Draw(IGraphics& g) override
  {
    const IRECT r = mRECT;

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

      g.DrawLine(gui::kColorBlue.WithOpacity(0.06f), x, midY - halfH, x, midY + halfH, nullptr, 1.f);
      g.DrawLine(gui::kColorBlue, x, y1, x, y2, nullptr, 1.f);
    }

    // Zero-axis line
    g.DrawLine(gui::kColorSeparator, r.L, midY, r.R, midY, nullptr, 0.5f);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override {}

private:
  WaveformPeaks mPeaks;
};

} // namespace rvrse
