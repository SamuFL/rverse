#pragma once

/// @file ExportRender.h
/// @brief Stateless helper for rendering the normal riser+hit export path offline.

#include "RvrseProcessor.h"
#include "SampleData.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace rvrse {

struct ExportRenderConfig
{
  float mFadeInPct = 0.0f;
  float mRiserGain = 1.0f;
  float mHitGain = 1.0f;
  float mVelocityGain = 1.0f;
};

struct ExportRenderData
{
  std::vector<float> mInterleaved;
  uint32_t mSampleRate = 0;
  int mNumFrames = 0;

  bool IsReady() const
  {
    return mSampleRate > 0 && mNumFrames > 0 && mInterleaved.size() == static_cast<size_t>(mNumFrames * 2);
  }
};

inline bool RenderNormalExport(const RiserData& riser,
                               const SampleData& hit,
                               const ExportRenderConfig& config,
                               ExportRenderData& output)
{
  output = {};

  if (!riser.IsReady() || !hit.IsLoaded())
    return false;

  const int riserFrames = riser.NumFrames();
  const int hitFrames = hit.NumFrames();
  if (riserFrames <= 0 || hitFrames <= 0)
    return false;

  const int hitOffset = riser.HitStartFrame();
  const int totalFrames = std::max(riserFrames, hitOffset + hitFrames);
  if (totalFrames <= 0)
    return false;

  const int fadeInLen = static_cast<int>(static_cast<float>(riserFrames) * config.mFadeInPct);
  const float velocityGain = std::clamp(config.mVelocityGain, 0.0f, 1.0f);
  output.mSampleRate = static_cast<uint32_t>(std::lround(riser.mSampleRate > 0.0 ? riser.mSampleRate : hit.mSampleRate));
  output.mNumFrames = totalFrames;
  output.mInterleaved.assign(static_cast<size_t>(totalFrames * 2), 0.0f);

  if (output.mSampleRate == 0)
    return false;

  for (int frame = 0; frame < totalFrames; ++frame)
  {
    float left = 0.0f;
    float right = 0.0f;

    if (frame < riserFrames)
    {
      float riserLeft = riser.mLeft[frame];
      float riserRight = riser.mRight[frame];

      if (config.mFadeInPct > 0.0f && fadeInLen > 1 && frame < fadeInLen)
      {
        const float fadeGain = static_cast<float>(frame) / static_cast<float>(fadeInLen - 1);
        riserLeft *= fadeGain;
        riserRight *= fadeGain;
      }

      left += riserLeft * velocityGain * config.mRiserGain;
      right += riserRight * velocityGain * config.mRiserGain;
    }

    const int hitFrame = frame - hitOffset;
    if (hitFrame >= 0 && hitFrame < hitFrames)
    {
      left += hit.mLeft[hitFrame] * velocityGain * config.mHitGain;
      right += hit.mRight[hitFrame] * velocityGain * config.mHitGain;
    }

    output.mInterleaved[static_cast<size_t>(frame * 2)] = left;
    output.mInterleaved[static_cast<size_t>(frame * 2 + 1)] = right;
  }

  return true;
}

} // namespace rvrse
