# RVRSE — User Feedback Collection (v1.1.0 Roadmap Input)

> **Status:** Collecting & Clustering
> **Last updated:** 2026-04-11

---

## 🐛 Bugs

### BUG-001: macOS Gatekeeper xattr command fails — path mismatch in install instructions
- **Source:** User feedback (macOS, MacBook Air M4)
- **Summary:** User copied RVRSE.vst3 to the correct VST3 folder and can see it in Finder, but `xattr -cr ~/Library/Audio/Plug-Ins/VST3/RVRSE.vst3` returns "No such file or directory."
- **Root cause (likely):** The plugin is installed in the system-level `/Library/Audio/Plug-Ins/VST3/` but the install instructions (or user assumption) point to the user-level `~/Library/Audio/Plug-Ins/VST3/`. The `~` expands to `/Users/<username>/Library/...` which is a different path.
- **Evidence:** Screenshot 1 shows the file present in Finder under `Library > Audio > Plug-Ins > VST3`. Screenshot 2 shows the terminal error with the `~/` prefix.
- **Action:** Review install instructions — clarify system vs. user Library path, or provide both variants. Consider whether the README/install guide needs a macOS-specific section.
- **Notes:** User called the plugin "really useful and actually quite epic."

### BUG-002: Plugin not recognized in Studio One / Fender Studio Pro
- **Source:** Multiple users (×2) — one on Fender Studio Pro, one on Studio One 8
- **Summary:** RVRSE is not detected by Studio One (or its Fender rebrand). One user confirmed standalone works fine. Both Studio One and Fender Studio Pro support VST3, so this is not a format issue.
- **Possible causes:** Wrong install directory, VST3 scan path not configured in Studio One's settings, code-signing/quarantine issue, or a Studio One-specific plugin validation problem.
- **Action:** Investigate whether this is a Studio One-specific compatibility issue rather than just user error. Two reports from the same DAW family is a pattern. May also warrant a troubleshooting FAQ entry covering plugin scan paths for common DAWs.
- **Notes:** Second user pointedly calls it "Studio One 8" rather than Fender Studio Pro ("I am one of the never Fender people").

### BUG-003: Plugin not recognized in Cubase (macOS)
- **Source:** User feedback (Cubase, Mac)
- **Summary:** Cubase doesn't recognize the RVRSE VST3. User asks if others have the same issue.
- **Likely cause:** User probably skipped the `xattr -cr` quarantine removal step. Without it, macOS Gatekeeper blocks the plugin and DAWs silently fail to load it during scan.
- **Related to:** BUG-001 — both point to the macOS install instructions not being prominent or clear enough. The quarantine removal step is critical but easy to miss.
- **Action:** Make the xattr step more prominent in docs — consider bolding it, adding a warning callout, or even providing a one-click install script for macOS.

### BUG-004: "Failed to read PCM data from file" error in FL Studio
- **Source:** User feedback (FL Studio)
- **Summary:** User tried to load a sample (128Kbps, 44.1kHz, 16-bit) and got the error "failed to read PCM data from file." The file is a stock FL Studio sample.
- **Possible causes:** The file might be MP3/OGG/compressed rather than WAV despite the specs listed. RVRSE may only support uncompressed WAV/AIFF. The "128Kbps" detail suggests this is a compressed format — WAV at 44.1kHz/16-bit would be ~1411 Kbps, not 128.
- **Action:** If RVRSE only supports uncompressed PCM, this should be documented clearly. Consider adding support for common compressed formats, or at minimum a clearer error message indicating which formats are supported.

### BUG-005: Empty resource/examples folder
- **Source:** User feedback (same as BUG-004, FL Studio)
- **Summary:** User couldn't load the bundled example samples because the resource folder was empty after installation.
- **Possible causes:** Installer didn't copy example files, or path mapping issue.
- **Action:** Verify that example samples are included and correctly placed during installation on all platforms.

### FR-001: Dry/bypass sample on separate MIDI note
- **Source:** User feedback (Windows 11, Cakewalk, VST3)
- **Summary:** User wants to trigger the original (dry) sample on a different MIDI note (e.g. D1) alongside the RVRSE-processed version (e.g. C1), so they can alternate between processed and unprocessed hits without leaving the plugin.
- **Use case:** Drum design — using RVRSE on a kick or snare but needing quick access to the clean sample within the same instrument instance.
- **Notes:** User explicitly stated "no bugs" and called it "a great tool" and "quite a time saver."

### FR-002: Sample trimming — remove dead air at front (and back)
- **Source:** User feedback (+4 total reports)
- **Summary:** Ability to trim/cut the sample to remove silence or select a portion. One user also asks whether this would trim the back end of the reversed sample so processed and dry meet in the middle with no gap. One user specifically references "sample cut" functionality mentioned at 7:30 in the announcement video.
- **Use case:** Tighter timing alignment between the reverse tail and the original hit.

### FR-003: Crossfade tool
- **Source:** User feedback
- **Summary:** A crossfade control for blending the transition between the reversed tail and the original sample.
- **Related to:** FR-002 (trimming) — both address the seam between reverse and dry.

### FR-004: Additional tempo sync subdivisions for odd time signatures
- **Source:** User feedback
- **Summary:** Add tempo sync values based on multiples of 3, 5, 6, and 7 to support music not in 4/4.
- **Use case:** Prog rock and other odd-meter genres.

### FR-005: Stutter depth — MIDI CC assignment & reassignment
- **Source:** User feedback
- **Summary:** Stutter rate is mapped to CC1 by default. User asks which CC controls stutter depth, and whether both can be reassigned.
- **Related to:** QST-001

### FR-007: External/sidechain reverb — replace built-in reverb with any plugin
- **Source:** User feedback (experienced user, ~40 years of reverse-reverb techniques)
- **Summary:** Instead of using RVRSE's built-in reverb, allow the user to route an external reverb plugin into RVRSE's processing chain. Essentially: let the user supply their own reverb and have RVRSE handle the reverse/rise mechanics around it.
- **Rationale:** "Why bother building your own reverb when you could open it up to all that already exist?"
- **References:** User points to Eventide Physion MkII as a model for this kind of architecture. Also mentions AudioThing plugins as effects they'd want to feed in.
- **Technical consideration:** Could be achieved via sidechain input, or a send/return loop within the plugin. User hopes it wouldn't require a "wrapper" approach. This would be a significant architectural change.
- **Notes:** User has deep experience doing reverse-reverb manually (reel-to-reel, cassette, DAWs over decades) — strong validation of the concept, and the request comes from someone who knows the workflow intimately.

### FR-009: iPadOS / AUv3 build
- **Source:** User feedback (iOS-based musician)
- **Summary:** User requests an iPadOS version of RVRSE. States all their music production is now iOS-based and highlights a "great musician community" there.
- **Technical consideration:** Would require an AUv3 (Audio Unit v3) build. iPlug2 does have iOS/AUv3 support in its target list, but the UI layer (NanoVG/OpenGL) would need touch input adaptation, and distribution goes through the App Store. Significant effort.

### FR-006: Linux build
- **Source:** Multiple users (YouTube comments, ×2)
- **Summary:** Users asking for a Linux version of RVRSE.
- **Notes:** Two separate requests so far — signals genuine demand from the Linux audio community (Bitwig, REAPER on Linux, etc.).

### FR-014: Fractional LFO rates (instead of frequency)
- **Source:** User feedback (plugin developer working on similar project)
- **Summary:** Allow LFO rates to be expressed as fractional values rather than (or in addition to) frequency. Likely referring to stutter LFO — e.g. expressing rate as note divisions (1/3, 3/8, etc.) rather than Hz.
- **Related to:** FR-004 (odd time signature support) — both address rhythmic flexibility.

### FR-015: ARA (Audio Random Access) support
- **Source:** User feedback (same as FR-014)
- **Summary:** ARA integration would allow RVRSE to work as an ARA plugin, enabling tighter DAW integration — direct access to audio on the timeline without bouncing/loading samples manually.
- **Technical consideration:** ARA is a significant integration effort (Celemony's ARA SDK). Would fundamentally change the workflow from "load a sample" to "process audio in-place on the timeline." Supported by Studio One, Logic, Cubase, REAPER, etc.
- **Notes:** User also suggested that advanced features like this "could be paid" — signals willingness to pay for a pro tier.

## 💬 General Remarks

### RMK-001: Fellow plugin dev — UI praise and tech curiosity
- **Source:** User feedback (plugin developer, uses JUCE)
- **Summary:** User is building their own plugins and asked how the RVRSE UI was created, guessing JUCE. Noted that UI design is the hardest part. Planning to release a free reduced version of their own plugin after beta.
- **Takeaway:** The UI is making an impression — validates the Stitch → NanoVG design workflow. Could be a content opportunity (behind-the-scenes / making-of).

### FR-010: Drag and drop sample loading (drag IN)
- **Source:** Multiple users (×5)
- **Summary:** Drag samples directly into the plugin instead of using a file browser.
- **Notes:** Five separate requests — by far the most requested feature. Non-negotiable for v1.1.

### FR-011: Drag sample OUT / render-in-place / export
- **Source:** Multiple users (×3)
- **Summary:** Export/render the processed result by dragging it out of the plugin directly into the DAW arrangement/audio track. One user specifically mentions "render-in-place and export" as separate capabilities.
- **Notes:** Common workflow in instruments like Serato Sample, XLN XO, Output Arcade. One user suggests these features "could be paid."

### FR-012: Play button (non-MIDI preview)
- **Source:** User feedback
- **Summary:** A built-in play/preview button so users can audition the result without needing to trigger via MIDI. Lowers the barrier for quick iteration.

### FR-013: Sample region selection (start/end points within a longer file)
- **Source:** User feedback
- **Summary:** Ability to define a specific region within a longer sample (e.g. isolate the beginning of a word in a vocal recording) rather than processing the entire file.
- **Use case:** Cinematic vocal rises — pick a specific syllable or word onset from a longer vocal take.
- **Related to:** FR-002 (trimming) — FR-002 is about removing silence; this is about selecting an arbitrary region of content.

### RMK-003: Simplicity praised
- **Source:** User feedback (same as FR-010)
- **Summary:** "It's simple, and I like it." Validates the minimal/focused design approach.

### RMK-005: Reason (Windows) tester incoming
- **Source:** User feedback
- **Summary:** User plans to test RVRSE in Reason on Windows. No issues reported yet — potential future feedback source.

### RMK-006: Fellow developer building similar project — collaboration interest
- **Source:** User feedback (same as FR-014, FR-015)
- **Summary:** User has been working on a similar plugin for months and wants to talk. Used RVRSE for an hour and provided detailed feedback. Also suggests advanced features "could be paid" — validates a freemium/pro tier model.

### RMK-008: Confirmed working — Windows 11 + REAPER 7.68
- **Source:** User feedback
- **Summary:** User tested RVRSE on Windows 11 with REAPER 7.68, reports no problems. Also praised the UI look.


- **Source:** User feedback
- **Summary:** User called RVRSE "a free version of Snapback." Snapback by Cableguys (collab with BT) is a $49 commercial drum layering effect that added a reverse feature in v1.1. The comparison positions RVRSE as a credible free alternative in the same space.
- **Takeaway:** Competitive validation — RVRSE is being compared to established commercial plugins. Also useful for SEO/marketing positioning.


- **Source:** User feedback
- **Summary:** User sees RVRSE as both a utility and a sound design tool for electronic dance music and electronica. Hasn't tested yet but plans to.

### RMK-002: Vocal use case validation
- **Source:** User feedback
- **Summary:** "Game changer for vocals." No specifics, but signals that RVRSE is being used for vocal production — worth noting as a use case beyond the typical cinematic/drum design scenarios.

### QST-001: Which MIDI CC controls stutter depth?
- **Source:** User feedback
- **Summary:** User knows stutter rate is on CC1, but it's unclear which CC controls depth — and whether CCs are reassignable. Suggests a documentation or UI clarity gap.
- **Related to:** FR-005

### QST-004: How to trigger/play sounds? (standalone and DAW)
- **Source:** User feedback (REAPER)
- **Summary:** User loaded a sample in both standalone and REAPER but can't figure out how to trigger playback. Doesn't realize MIDI input is required.
- **Implication:** The "you need to send MIDI to trigger it" concept isn't obvious, especially for users coming from effect-plugin workflows. Strongly reinforces FR-012 (play button) and suggests the UI or onboarding needs a clearer hint about MIDI triggering.
- **Related to:** FR-012 (play button as non-MIDI alternative)


- **Source:** User feedback (former Reason user)
- **Summary:** User recalls Reason's reverse reverb being uniquely powerful for turning sustained synth patches into playable tonal textures, especially with pitch bend. Wonders if RVRSE can serve a similar role.
- **Notes:** This stretches beyond RVRSE's current "rise and hit designer" scope into chromatic/playable instrument territory. See Appendix for related product idea (Padilizer).
- **Source:** User feedback
- **Summary:** User wants to use RVRSE in Pro Tools, assuming VST3 would work there. Pro Tools only supports AAX — RVRSE currently doesn't ship as AAX.
- **Implication:** Potential FR for AAX build (see FR-008).

### FR-008: AAX build (Pro Tools support)
- **Source:** Derived from QST-002
- **Summary:** Pro Tools requires AAX format. If there's demand from Pro Tools users, an AAX build would be needed.
- **Technical consideration:** AAX requires Avid's SDK and PACE/iLok wrapping for code signing. iPlug2 does support AAX as a target, but the signing/distribution pipeline adds complexity.

---

## 📎 Appendix: Open Sampler Research

### Rhapsody by Libre Wave — existing open-source sampler player
- **URL:** https://librewave.com/rhapsody/ | **Repo:** https://codeberg.org/LibreWave/Rhapsody
- **What it is:** An open-source sample library player built on HISE. GPLv3 licensed. Supports VST3, AU, standalone, and Linux. Currently at v2.5.2 (Oct 2024).
- **Key facts:**
  - Built on **HISE** (an open-source plugin development toolkit) — a full-featured sampler/synth/FX framework with scripting, GUI designer, and export to VST/AU/AAX/standalone.
  - HISE officially positions Rhapsody as its recommended distribution platform — devs can export HISE instruments as Rhapsody libraries *without needing a C++ compiler*.
  - Third-party libraries already exist (PianoBook has at least two).
  - Libre Wave also publishes its own instrument libraries (world/ethnic focus).
  - VI-Control community reception: positive on UI and features, but small library catalog limits adoption.
  - Audio samples use Creative Commons Plus license (not GPL), so compositions aren't forced to be GPL.
- **Developer:** Libre Wave / project maintainer (also runs Xtant Audio).

### Assessment: Should Open Sampler still be built?
- **The case for "no":** Rhapsody and Shortcircuit XT already exist, are open-source, and cover different parts of the sampler space. Building from scratch would duplicate significant effort.
- **The case for "yes, but differently":** Rhapsody inherits HISE's complexity and GPL constraints. Shortcircuit XT is a creative sampler, not a library player. If Open Sampler's thesis is specifically about *simplicity* (Decent Sampler's dead-simple XML format as an open-source alternative), that niche may still be underserved.
- **The case for "contribute instead":** Fork or contribute to Rhapsody or Shortcircuit XT rather than starting from zero.
- **Decision:** TBD

### Shortcircuit XT by Surge Synth Team — open-source creative sampler
- **URL:** https://surge-synth-team.org/shortcircuit-xt/ | **Repo:** https://github.com/surge-synthesizer/shortcircuit-xt
- **What it is:** A modern open-source creative sampler rebuilt by the Surge Synth Team from Vember Audio's Shortcircuit 2 codebase. Public beta as of Feb 2026. GPLv3.
- **Key facts:**
  - Built on **JUCE** (modern C++ rewrite, not a port of the original code).
  - Lineage: the original Vember Audio developer open-sourced Surge in 2018 and Shortcircuit 2 in 2021 → Surge Synth Team rebuilt both.
  - Supports VST3, AU, CLAP on macOS (Apple Silicon + Intel), Windows, and Linux.
  - Up to 16 parts, multi-sample support (WAV, SFZ, AIFF), drag-and-drop workflow.
  - Deep sound design: 5 envelopes, 4 LFOs, modulation matrix, built-in synth engines, filters, FX, mixing console.
  - MPE compatible.
  - Positioned as a *creative sampler* (sound design, sample mangling) rather than a library playback engine — different niche than Rhapsody/Decent Sampler.
- **Relevance to Open Sampler:** Shortcircuit XT is closer to Kontakt's creative/power-user side than to Decent Sampler's simplicity-first approach. Together with Rhapsody, the open-source sampler landscape now has both a library player (Rhapsody/HISE) and a creative sampler (SCXT). The "simple open-source Decent Sampler replacement" gap remains the most underserved.

### "Padilizer" — product idea seed (from RVRSE feedback)
- **Trigger:** User asked if RVRSE can be used as a tonal instrument, referencing Reason's reverse reverb on sustained synth patches + pitch bend for expressive playable textures.
- **Concept (working name: "Padilizer"):** A plugin that takes sustained/pad-like input and transforms it into a playable chromatic instrument using reverse-reverb, granular, or spectral processing — essentially the tonal/melodic cousin of RVRSE.
- **Differentiator from RVRSE:** RVRSE is a rise/hit designer (rhythmic, one-shot). Padilizer would be a *playable instrument* — chromatic mapping, pitch bend, sustain, potentially polyphonic.
- **Overlap with RVRSE:** Could share DSP components (reverb engine, reverse processing) but the interaction model is fundamentally different.
- **Status:** Idea phase. No commitment.

### "Logic EQ for Windows" — product idea seed (from user request)
- **Trigger:** User asked for an EQ plugin like Logic Pro's Channel EQ, but for Windows.
- **Concept:** A clean, visual parametric EQ with real-time spectrum analyzer — modeled after Logic Pro's beloved EQ UX, targeting Windows users who don't have access to it.
- **Notes:** Logic's Channel EQ is widely regarded as one of the best stock EQ interfaces. A free/open-source alternative with similar UX could fill a gap on Windows. Low priority — very different product category from RVRSE.
- **Status:** Idea phase. No commitment.
