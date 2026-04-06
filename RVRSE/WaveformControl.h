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

    // Draw riser waveform (gold)
    if (!mRiserPeaks.IsEmpty())
    {
      DrawWaveform(g, mRiserPeaks, IRECT(r.L, r.T, splitX, r.B),
                   gui::kColorGold.WithOpacity(0.7f), gui::kColorGold.WithOpacity(0.3f));
    }

    // Draw hit waveform (blue)
    if (!mHitPeaks.IsEmpty())
    {
      DrawWaveform(g, mHitPeaks, IRECT(splitX, r.T, r.R, r.B),
                   gui::kColorBlue.WithOpacity(0.7f), gui::kColorBlue.WithOpacity(0.3f));
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

    // Section labels
    if (mRiserFrames > 0)
    {
      const IText riserLabel(11, gui::kColorGold.WithOpacity(0.5f), "Roboto-Regular", EAlign::Near, EVAlign::Top);
      IRECT labelR(r.L + 6.f, r.T + 4.f, splitX - 4.f, r.T + 18.f);
      g.DrawText(riserLabel, "RISER", labelR);
    }
    if (mHitFrames > 0)
    {
      const IText hitLabel(11, gui::kColorBlue.WithOpacity(0.5f), "Roboto-Regular", EAlign::Near, EVAlign::Top);
      IRECT labelR(splitX + 6.f, r.T + 4.f, r.R - 4.f, r.T + 18.f);
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
  void DrawWaveform(IGraphics& g, const WaveformPeaks& peaks, const IRECT& area,
                    const IColor& fillColor, const IColor& bgColor) const
  {
    const float w = area.W();
    const float h = area.H();
    const float midY = area.MH();
    const float halfH = h * 0.45f; // leave a little headroom

    // Find global peak for normalization
    float globalPeak = 0.001f; // avoid division by zero
    for (int i = 0; i < peaks.mNumBins; ++i)
    {
      globalPeak = std::max(globalPeak, std::abs(peaks.mMin[i]));
      globalPeak = std::max(globalPeak, std::abs(peaks.mMax[i]));
    }

    const float binsPerPixel = static_cast<float>(peaks.mNumBins) / w;

    for (int px = 0; px < static_cast<int>(w); ++px)
    {
      const int bin = static_cast<int>(px * binsPerPixel);
      if (bin >= peaks.mNumBins) break;

      const float minVal = peaks.mMin[bin] / globalPeak;
      const float maxVal = peaks.mMax[bin] / globalPeak;

      const float x = area.L + static_cast<float>(px);
      const float y1 = midY - maxVal * halfH;
      const float y2 = midY - minVal * halfH;

      // Background fill (subtle)
      g.DrawLine(bgColor, x, midY - halfH, x, midY + halfH, nullptr, 1.f);
      // Waveform fill
      g.DrawLine(fillColor, x, y1, x, y2, nullptr, 1.f);
    }
  }

  WaveformPeaks mRiserPeaks;
  WaveformPeaks mHitPeaks;
  int mRiserFrames = 0;
  int mHitFrames = 0;
  float mPlayheadPos = -1.f; ///< 0..1 fraction, negative = hidden
};

} // namespace rvrse
