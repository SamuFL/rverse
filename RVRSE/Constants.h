#pragma once

/// @file Constants.h
/// @brief Central location for all RVRSE constants. No magic numbers in DSP code.

namespace rvrse {

// --- MIDI CC Defaults (MVP: hardcoded, future: user-assignable) ---
constexpr int CC_STUTTER_RATE  = 1;   ///< Mod wheel → Stutter Rate
constexpr int CC_STUTTER_DEPTH = 11;  ///< Expression pedal → Stutter Depth
constexpr int CC_RISER_TUNE    = 2;   ///< Breath controller → Riser Tune

// --- Riser Length musical values (in beats) ---
constexpr double kRiserLengthMin     = 0.25;   ///< 1/4 beat
constexpr double kRiserLengthMax     = 16.0;   ///< 16 beats
constexpr double kRiserLengthDefault = 4.0;    ///< 4 beats

// --- Parameter defaults ---
constexpr double kLushDefault        = 40.0;   ///< Reverb wet amount (0–100%)
constexpr double kFadeInDefault      = 60.0;   ///< Fade-in shape (0–100%)
constexpr double kStutterDepthDefault = 50.0;  ///< Stutter wet/dry (0–100%)
constexpr double kStutterRateMinHz   = 0.0;    ///< Stutter rate minimum (0 = off)
constexpr double kStutterRateMaxHz   = 30.0;   ///< Stutter rate maximum in Hz
constexpr double kStutterRateDefaultHz = 0.0;  ///< Stutter rate default (off)

// --- Tuning ---
constexpr double kTuneMinSemitones   = -24.0;
constexpr double kTuneMaxSemitones   =  24.0;

// --- Reverb (Schroeder / Moorer) ---
constexpr int   kNumCombs            = 8;       ///< Number of parallel comb filters
constexpr int   kNumAllpasses        = 4;       ///< Number of series allpass filters
constexpr float kReverbMinRoomFactor = 0.5f;    ///< Room size multiplier at Lush = 0
constexpr float kReverbMaxRoomFactor = 1.5f;    ///< Room size multiplier at Lush = 1
constexpr float kReverbMinFeedback   = 0.70f;   ///< Comb feedback at Lush = 0
constexpr float kReverbMaxFeedback   = 0.90f;   ///< Comb feedback at Lush = 1
constexpr float kReverbMinDamping    = 0.2f;    ///< Comb LP damping at Lush = 0
constexpr float kReverbMaxDamping    = 0.5f;    ///< Comb LP damping at Lush = 1
constexpr float kReverbAllpassGain   = 0.5f;    ///< Allpass feedback coefficient

// --- Time-Stretching (OLA) ---
constexpr int   kOlaWindowSize       = 2048;    ///< OLA analysis/synthesis window size in samples

// --- Riser Tail Fade-Out ---
/// Fraction of a beat used for the riser tail fade-out (BPM-adaptive).
/// Smooths the riser→hit boundary to prevent a click from the reversed transient.
/// Set to 0.0 to disable the fade-out entirely.
/// Examples: 1/16 = 0.0625 (subtle), 1/4 = 0.25 (gentle), 1.0 = full beat.
constexpr double kRiserTailFadeBeats = 0.0625;  ///< 1/16 of a beat

// --- Audio ---
constexpr double kDefaultBPM         = 120.0;
constexpr double kNoteOffFadeMs      = 5.0;    ///< Note-off fade-out duration in milliseconds (anti-click)
constexpr double kStutterFadeMs      = 2.0;    ///< Stutter gate ramp duration in milliseconds (anti-click)

// --- Sample Loading ---
constexpr int    kMaxSampleLengthSeconds = 30;             ///< Max sample length in seconds
constexpr int    kMaxSampleFrames    = 192000 * kMaxSampleLengthSeconds; ///< Max frames at 192 kHz (supports all standard rates)
constexpr int    kMaxSampleChannels  = 2;                  ///< Stereo max

/// Supported file extensions for the open-file dialog
constexpr const char* kSupportedAudioExts = "wav aif aiff";

} // namespace rvrse
