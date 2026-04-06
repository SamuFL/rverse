#pragma once
/// @file GUIColors.h
/// Centralised brand color palette and shared style definitions for the RVRSE GUI.
/// Source of truth: STITCH-DESIGN.md + SamuFL brand kit.

#include "IGraphicsStructs.h"

namespace rvrse {
namespace gui {

// ── Brand palette ──────────────────────────────────────────────────────
const IColor kColorGold          {255, 0xFF, 0xCC, 0x53}; // #FFCC53 — primary accent
const IColor kColorBlue          {255, 0x44, 0x90, 0xFC}; // #4490FC — alt-accent
const IColor kColorDark          {255, 0x0A, 0x10, 0x0A}; // #0A100A — deepest bg
const IColor kColorDarkGrey      {255, 0x22, 0x2D, 0x42}; // #222D42 — panel bg
const IColor kColorHighlight     {255, 0xD8, 0xFD, 0xFB}; // #D8FDFB — playhead
const IColor kColorWhite         {255, 0xFF, 0xFF, 0xFF}; // #FFFFFF

// ── Derived surface colors ─────────────────────────────────────────────
const IColor kColorHeaderBg      {255, 0x0E, 0x15, 0x0E}; // #0E150E
const IColor kColorWaveformBg    {255, 0x06, 0x0A, 0x06}; // #060A06
const IColor kColorSeparator     {255, 0x1A, 0x22, 0x35}; // #1A2235
const IColor kColorKnobTrack     {255, 0x1A, 0x22, 0x35}; // #1A2235

// ── Text colors ────────────────────────────────────────────────────────
const IColor kColorTextPrimary   {255, 0xDE, 0xE4, 0xDA}; // #DEE4DA
const IColor kColorTextSecondary {255, 0x8B, 0x95, 0xA5}; // #8B95A5
const IColor kColorTextMuted     {255, 0x4A, 0x56, 0x68}; // #4A5568

// ── Knob accent variants ──────────────────────────────────────────────
const IColor kColorSteel         {255, 0xA0, 0xAE, 0xC0}; // #A0AEC0 — offline knobs

// ── Layout proportions (in pixels at default 1024×768) ─────────────────
constexpr float kHeaderHeight  = 60.f;
constexpr float kFooterHeight  = 55.f;
constexpr float kWaveformHeight = 170.f;
constexpr float kZoneGap       = 2.f;
constexpr float kPanelPad      = 2.f;
constexpr float kRiserPanelPct = 0.6f; // riser panel = 60% of width

} // namespace gui
} // namespace rvrse
