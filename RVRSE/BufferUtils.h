#pragma once

/// @file BufferUtils.h
/// @brief Stateless buffer manipulation utilities for the offline pipeline.
///        Never called from the audio thread.

#include <algorithm>
#include <vector>

namespace rvrse {

/// Reverse a mono buffer in-place.
/// @param buf  Buffer to reverse
inline void reverseBuffer(std::vector<float>& buf)
{
  std::reverse(buf.begin(), buf.end());
}

/// Reverse stereo buffers in-place (deinterleaved L/R).
/// Both channels are reversed independently so stereo imaging is preserved.
/// @param left   Left channel buffer
/// @param right  Right channel buffer
inline void reverseBufferStereo(std::vector<float>& left, std::vector<float>& right)
{
  std::reverse(left.begin(), left.end());
  std::reverse(right.begin(), right.end());
}

} // namespace rvrse
