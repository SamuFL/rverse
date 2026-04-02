#pragma once

/// @file Stutter.h
/// @brief Real-time rhythmic gate for the riser voice.
///
/// AUDIO THREAD ONLY — never call from the offline pipeline.
/// All state is explicit (StutterState struct) so the gate is unit-testable
/// and stateless from the caller's perspective.
///
/// The rate is a continuous frequency in Hz (e.g. 1–30 Hz), giving smooth
/// creative control. A tempo-synced mode is planned for later (rverse-0gq).

#include "Constants.h"
#include <algorithm>
#include <cmath>

namespace rvrse {

/// Mutable per-voice state for the stutter gate.
/// Reset this when a new note starts.
struct StutterState
{
  int phase = 0;  ///< Current sample position within the stutter period
};

/// Compute the stutter gate gain for a single sample.
///
/// Uses a trapezoidal wave (square wave with short ramps) to avoid clicks.
/// Rate is a continuous frequency in Hz — 0 Hz means stutter is off.
///
/// @param state       Mutable stutter state (phase advances by 1 each call)
/// @param rateHz      Stutter frequency in Hz (0 = off, range: kStutterRateMinHz–kStutterRateMaxHz)
/// @param depth       Wet/dry mix of the gate (0.0 = no effect, 1.0 = full chop)
/// @param sampleRate  DAW sample rate in Hz
/// @return            Gain multiplier in [0.0, 1.0] to apply to the riser sample
inline float stutterProcess(StutterState& state,
                            float rateHz,
                            float depth,
                            double sampleRate)
{
  // Stutter off or depth at zero → pass through
  if (rateHz <= 0.0f || depth <= 0.0f)
    return 1.0f;

  // Compute period in samples from Hz
  const int period = std::max(1, static_cast<int>(sampleRate / static_cast<double>(rateHz)));
  const int half = period / 2;

  // Ramp length for anti-click fade (trapezoidal wave instead of square)
  const int ramp = std::max(1, std::min(half / 4,
    static_cast<int>(sampleRate * kStutterFadeMs / 1000.0)));

  const int pos = state.phase % period;

  // Trapezoidal gate: ramp up → sustain at 1.0 → ramp down → sustain at 0.0
  float gate;
  if (pos < ramp)
    gate = static_cast<float>(pos) / static_cast<float>(ramp);           // ramp up
  else if (pos < half - ramp)
    gate = 1.0f;                                                          // open
  else if (pos < half)
    gate = static_cast<float>(half - pos) / static_cast<float>(ramp);    // ramp down
  else
    gate = 0.0f;                                                          // closed

  // Advance phase (wrap to avoid eventual overflow)
  state.phase++;
  if (state.phase >= period)
    state.phase -= period;

  // Blend between dry (1.0) and gated signal based on depth
  return 1.0f - depth * (1.0f - gate);
}

/// Reset the stutter state (call on note-on).
inline void stutterReset(StutterState& state)
{
  state.phase = 0;
}

} // namespace rvrse
