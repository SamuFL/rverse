# RVRSE Domain

RVRSE turns a source hit into a tempo-aligned playable sequence consisting of a generated riser and the original hit.

## Language

**Beat Anchor**:
The exact musical instant at the midpoint of the committed trimmed hit's technical onset ramp, where its impact is perceived to land.
_Avoid_: Hit point, seam midpoint, raw sample start

**Technical Onset Ramp**:
A fixed, short gain ramp that conditions the trimmed hit boundary to prevent clicks. It is not a user-controlled transition effect.
_Avoid_: Crossfade, attack

**Riser Release**:
The user-requested time over which the riser should continue past the Beat Anchor while fading to silence.
_Avoid_: Crossfade

**Effective Riser Release**:
The Riser Release used by the playable sequence after limiting it to the riser's pre-anchor duration. It may be shorter than the requested Riser Release.
_Avoid_: Clamped parameter

**Playable Sequence**:
A matched riser, trimmed hit, and transition timing that are ready to preview, perform, or export as one coherent result.
_Avoid_: Riser buffer, preview buffer

**Pending Render**:
An offline change whose replacement Playable Sequence is not yet ready. The previous Playable Sequence remains performable but is not eligible for Export.
_Avoid_: Loading

**Legacy Transition**:
The adaptive riser-tail behavior preserved for playable sequences restored from v1 project state. It does not restore older hit conditioning and remains distinct from numeric Riser Release until the user chooses a Riser Release value.
_Avoid_: Zero release, default release
