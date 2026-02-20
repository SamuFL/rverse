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

// --- Audio ---
constexpr double kDefaultBPM         = 120.0;
constexpr double kNoteOffFadeMs      = 5.0;    ///< Note-off fade-out duration in milliseconds (anti-click)

// --- Sample Loading ---
constexpr int    kMaxSampleLengthSeconds = 30;             ///< Max sample length in seconds
constexpr int    kMaxSampleFrames    = 48000 * kMaxSampleLengthSeconds; ///< Max frames at 48 kHz
constexpr int    kMaxSampleChannels  = 2;                  ///< Stereo max

/// Supported file extensions for the open-file dialog
constexpr const char* kSupportedAudioExts = "wav aif aiff";

} // namespace rvrse
