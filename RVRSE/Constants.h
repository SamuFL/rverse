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

// --- Audio ---
constexpr double kDefaultBPM         = 120.0;

// --- Sample Loading ---
constexpr int    kMaxSampleLengthSeconds = 30;             ///< Max sample length in seconds
constexpr int    kMaxSampleFrames    = 48000 * kMaxSampleLengthSeconds; ///< Max frames at 48 kHz
constexpr int    kMaxSampleChannels  = 2;                  ///< Stereo max

/// Supported file extensions for the open-file dialog
constexpr const char* kSupportedAudioExts = "wav aif aiff";

} // namespace rvrse
