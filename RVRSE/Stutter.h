#pragma once

/// @file Stutter.h
/// @brief Real-time rhythmic gate for the riser voice.
///
/// AUDIO THREAD ONLY — never call from the offline pipeline.
/// All state is explicit (StutterState struct) so the gate is unit-testable
/// and stateless from the caller's perspective.

#include "Constants.h"
#include <cmath>

namespace rvrse {

/// Stutter rate expressed as a musical subdivision.
/// Values correspond to note durations relative to one beat.
enum class EStutterRate : int
{
  Off = 0,   ///< Stutter disabled
  N1_32,     ///< 1/32 note  (0.125 beats)
  N1_16,     ///< 1/16 note  (0.25 beats)
  N1_8,      ///< 1/8 note   (0.5 beats)
  N1_4,      ///< 1/4 note   (1 beat)
  N1_2,      ///< 1/2 note   (2 beats)
  kNumRates
};

/// Number of beats per stutter cycle for each rate value.
/// Index matches EStutterRate. Off maps to 0.0 (unused).
constexpr double kStutterRateBeats[] = {
  0.0,     // Off
  0.125,   // 1/32
  0.25,    // 1/16
  0.5,     // 1/8
  1.0,     // 1/4
  2.0      // 1/2
};

/// Mutable per-voice state for the stutter gate.
/// Reset this when a new note starts.
struct StutterState
{
  int phase = 0;  ///< Current sample position within the stutter period
};

/// Compute the stutter gate gain for a single sample.
///
/// @param state       Mutable stutter state (phase advances by 1 each call)
/// @param rate        Current stutter rate subdivision
/// @param depth       Wet/dry mix of the gate (0.0 = no effect, 1.0 = full chop)
/// @param sampleRate  DAW sample rate in Hz
/// @param bpm         Host tempo in BPM
/// @return            Gain multiplier in [0.0, 1.0] to apply to the riser sample
inline float stutterProcess(StutterState& state,
                            EStutterRate rate,
                            float depth,
                            double sampleRate,
                            double bpm)
{
  // Stutter off or depth at zero → pass through
  if (rate == EStutterRate::Off || depth <= 0.0f)
    return 1.0f;

  const int rateIdx = static_cast<int>(rate);
  const double beatsPerCycle = kStutterRateBeats[rateIdx];

  // Compute period in samples: beatsPerCycle × (60 / BPM) × sampleRate
  const double secondsPerCycle = beatsPerCycle * (60.0 / bpm);
  const int period = std::max(1, static_cast<int>(secondsPerCycle * sampleRate));

  // Square wave gate: first half open (1.0), second half closed (0.0)
  const float gate = (state.phase % period) < (period / 2) ? 1.0f : 0.0f;

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

/// Convert a normalised 0–1 value (e.g. from MIDI CC / param) to an EStutterRate.
/// Maps evenly across the range: 0.0 = Off, ~0.2 = 1/32, ... 1.0 = 1/2.
inline EStutterRate stutterRateFromNormalised(double normalised)
{
  if (normalised <= 0.0)
    return EStutterRate::Off;

  const int numRates = static_cast<int>(EStutterRate::kNumRates);
  const int idx = std::min(numRates - 1,
                           static_cast<int>(normalised * numRates));
  return static_cast<EStutterRate>(idx);
}

} // namespace rvrse
