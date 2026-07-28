# Model the transition as Riser Release

RVRSE models its user-controlled transition as a one-sided **Riser Release**, not a two-sided crossfade: the trimmed dry hit keeps its fixed technical onset ramp centered on the Beat Anchor, while the riser continues past that anchor and fades linearly to silence. The release is rendered by stretching the cached reversed buffer to the pre-anchor riser duration plus the effective release, avoiding reads outside the canonical trimmed source and avoiding an unnecessary reverb rebuild.

The requested release is stored in milliseconds, while the effective release is limited to the pre-anchor riser duration without overwriting the request. Project states created by v1 retain the old adaptive riser-tail behavior until the user deliberately commits a Release gesture; this compatibility mode does not restore older reverb or dry-hit processing.

## Consequences

- Riser Release changes require a stretch-stage rebuild, not a full pipeline rebuild.
- The dry hit is never faded by Riser Release, so overlap remains additive and receives no hidden gain compensation.
- Playback, export, and waveform preview must consume one shared transition-timing model.
- A literal numeric value of `0 ms` means no post-anchor extension or release fade.
