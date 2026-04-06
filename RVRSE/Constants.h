#pragma once

/// @file Constants.h
/// @brief Central location for all RVRSE constants. No magic numbers in DSP code.

namespace rvrse {

// --- Mathematical constants ---
/// Portable pi constant (M_PI is not standard C++ and missing on MSVC without _USE_MATH_DEFINES)
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// --- MIDI CC Defaults (MVP: hardcoded, future: user-assignable) ---
constexpr int CC_STUTTER_RATE  = 1;   ///< Mod wheel → Stutter Rate
constexpr int CC_STUTTER_DEPTH = 11;  ///< Expression pedal → Stutter Depth
constexpr int CC_RISER_TUNE    = 2;   ///< Breath controller → Riser Tune

// --- Riser Length discrete values (in beats) ---
/// Riser length is a discrete parameter with musically meaningful values.
/// The enum index is stored/serialized; use kRiserLengthValues[] to get beats.
enum ERiserLength
{
  kRiserLen_1_4  = 0, ///< 1/4 beat
  kRiserLen_1_2  = 1, ///< 1/2 beat
  kRiserLen_1    = 2, ///< 1 beat
  kRiserLen_2    = 3, ///< 2 beats
  kRiserLen_4    = 4, ///< 4 beats
  kRiserLen_8    = 5, ///< 8 beats
  kRiserLen_16   = 6  ///< 16 beats
};

constexpr int kNumRiserLengths = 7;
constexpr int kRiserLengthDefault = kRiserLen_4;  ///< Default: 4 beats

/// Lookup table: enum index → beat value
constexpr double kRiserLengthValues[kNumRiserLengths] = {
  0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0
};

/// Display labels for each riser length value
constexpr const char* kRiserLengthLabels[kNumRiserLengths] = {
  "1/4", "1/2", "1", "2", "4", "8", "16"
};

// --- Parameter defaults ---
constexpr double kLushDefault        = 40.0;   ///< Reverb wet amount (0–100%)
constexpr double kFadeInDefault      = 60.0;   ///< Fade-in shape (0–100%)
constexpr double kRiserVolumeDefault  = 0.0;    ///< Riser volume in dB (default: unity)
constexpr double kHitVolumeDefault   = 0.0;    ///< Hit volume in dB (default: unity)
constexpr double kVolumeMinDb        = -60.0;  ///< Voice volume minimum (-60 dB ≈ silence)
constexpr double kVolumeMaxDb        = 6.0;    ///< Voice volume maximum (+6 dB headroom)
constexpr double kStutterDepthDefault = 50.0;  ///< Stutter wet/dry (0–100%)
constexpr double kStutterRateMinHz   = 0.0;    ///< Stutter rate minimum (0 = off)
constexpr double kStutterRateMaxHz   = 30.0;   ///< Stutter rate maximum in Hz
constexpr double kStutterRateDefaultHz = 0.0;  ///< Stutter rate default (off)

// --- Debug Playback Mode ---
/// Selects which pipeline stage buffer is played back.
/// Normal = full riser + hit. Other modes play intermediate buffers for diagnostics.
enum EDebugStage
{
  kDebugNormal   = 0, ///< Normal operation: riser + hit
  kDebugReverbed = 1, ///< Play reverbed sample (before reverse/stretch), no hit
  kDebugReversed = 2, ///< Play reversed sample (before stretch), no hit
  kDebugRiserOnly = 3 ///< Play final riser (after stretch), suppress hit
};

constexpr int kNumDebugStages = 4;

// --- Tuning ---
constexpr double kTuneMinSemitones   = -24.0;
constexpr double kTuneMaxSemitones   =  24.0;

// --- Reverb (Schroeder / Moorer) ---
/// Maximum reverb tail duration appended to the source sample before processing.
/// This silence extension allows the reverb to ring out naturally, producing the
/// characteristic reverse-reverb "whoosh" that builds toward the hit.
/// A generous value ensures even high-lush settings decay fully.
constexpr double kReverbTailSeconds  = 5.0;     ///< Seconds of silence appended for reverb tail

/// Amplitude threshold below which trailing samples are considered silent
/// and trimmed after reverb processing. -30 dB ≈ 0.032.
/// The riser fade-in envelope masks any residual energy at the trim point.
constexpr float  kSilenceThreshold   = 0.032f;

constexpr int   kNumCombs            = 8;       ///< Number of parallel comb filters
constexpr int   kNumAllpasses        = 4;       ///< Number of series allpass filters
constexpr float kReverbMinRoomFactor = 0.5f;    ///< Room size multiplier at Lush = 0
constexpr float kReverbMaxRoomFactor = 1.5f;    ///< Room size multiplier at Lush = 1
constexpr float kReverbMinFeedback   = 0.70f;   ///< Comb feedback at Lush = 0
constexpr float kReverbMaxFeedback   = 0.90f;   ///< Comb feedback at Lush = 1
constexpr float kReverbMinDamping    = 0.2f;    ///< Comb LP damping at Lush = 0
constexpr float kReverbMaxDamping    = 0.5f;    ///< Comb LP damping at Lush = 1
constexpr float kReverbAllpassGain   = 0.5f;    ///< Allpass feedback coefficient

// --- Time-Stretching (signalsmith-stretch) ---
// Quality preset selection for the spectral stretcher.
// High = presetDefault (larger FFT, more overlap, better transients)
// Low  = presetCheaper (smaller FFT, wider hop, ~2x faster)
enum EStretchQuality
{
  kStretchQualityHigh = 0,  ///< Best quality — recommended for rendering/mixdown
  kStretchQualityLow  = 1   ///< Faster — for real-time tweaking or resource-limited systems
};

constexpr int kNumStretchQualities = 2;
constexpr int kStretchQualityDefault = kStretchQualityHigh;

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
