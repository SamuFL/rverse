# RVRSE — GUI Design Brief for Specialist Agent

## 1. What Is RVRSE?

RVRSE is a free, open-source audio plugin (VST3 / AU / CLAP) that generates a **reverse-reverb riser** from any loaded hit sample and fires the original hit at a tempo-synced beat boundary. Think of the classic "whoooosh → BOOM" reverse crash effect, fully automated.

**Target user:** Music producers working in EDM, cinematic scoring, and sound design.

---

## 2. Design Constraints (Non-Negotiable)

- **Framework:** iPlug2 IGraphics (C++ native). NO web UI, NO HTML/CSS/React.
- **Rendering:** NanoVG (vector) on macOS (Metal backend) and Windows (OpenGL2).
- **Resolution:** Default 1024×768 px, resizable with corner resizer.
- **Theme:** Dark. This is a production audio tool — dark backgrounds reduce eye fatigue in studio environments.
- **Controls must map to iPlug2 native widgets** (see Section 5 below).

---

## 3. Plugin Parameters — Complete Specification

Every parameter listed here MUST have a corresponding control in the GUI. The "Code Name" column is the C++ enum identifier — use it in implementation comments for traceability.

### 3.1 Riser Section — Offline Parameters
> Changes trigger background DSP recalculation (reverb → reverse → stretch pipeline).
> Turning these knobs has an inherent delay before the user hears the result (typically 0.5–2s).
> Consider a subtle "processing" indicator near the waveform display when a rebuild is in progress.

| # | Parameter | Code Name | Control Type | Range | Step | Default | Unit | Tooltip / UX Notes |
|---|---|---|---|---|---|---|---|---|
| 1 | **Lush** | `kParamLush` | Continuous knob | 0 – 100 | 0.1 | 40 | % | Controls reverb room size + wet gain. 0% = dry, 100% = cathedral wash. The name is intentionally musical, not technical. |
| 2 | **Riser Length** | `kParamRiserLength` | Discrete / stepped knob | 1/4, 1/2, 1, 2, 4, 8, 16 | — | 4 | beats | Duration of the riser before the hit. **Must latch to discrete musical values only** — no in-between positions. A stepped knob (7 positions) or a rotary selector works well. Display should show musical notation: "1/4", "1/2", "1", "2", "4", "8", "16" beats. iPlug2: use `InitEnum()` with 7 values, not `InitDouble()`. |
| 3 | **Fade In** | `kParamFadeIn` | Continuous knob | 0 – 100 | 0.1 | 60 | % | Shape of the volume ramp from silence to full over the riser duration. 0% = no fade (full volume from start), 100% = linear ramp from silence. |
| 4 | **Riser Volume** | `kParamRiserVolume` | Continuous knob | -60 – +6 | 0.1 | 0 | dB | Level of the riser output. -60 dB ≈ silence. Unity = 0 dB. |
| 5 | **Stretch Quality** | `kParamStretchQuality` | Toggle / Slide switch | High / Low | — | High | — | **Low priority control** — can be smaller or tucked away. "High" = best quality (presetDefault, larger FFT), recommended for rendering/mixdown. "Low" = faster (~2× CPU saving), for real-time tweaking or resource-limited systems. Tooltip: "High quality recommended for final renders. Use Low for faster previews." |

### 3.2 Riser Section — Real-Time Parameters
> These respond instantly (per-sample in audio thread). Full MIDI CC support.
> Visually group these separately from the offline knobs above — they feel different to the user.
> Each should have a **MIDI CC indicator** (◉) that lights up when receiving external CC data.

| # | Parameter | Code Name | Control Type | Range | Step | Default | Unit | MIDI CC | Tooltip / UX Notes |
|---|---|---|---|---|---|---|---|---|---|
| 6 | **Stutter Rate** | `kParamStutterRate` | Continuous knob | 0 – 30 | 0.1 | 0 (OFF) | Hz | CC 1 (mod wheel) | Rhythmic chop rate applied to the riser. 0 = stutter disabled. Higher values = faster chopping. |
| 7 | **Stutter Depth** | `kParamStutterDepth` | Continuous knob | 0 – 1.0 | 0.01 | 0.5 | (normalized) | CC 11 (expression) | Wet/dry mix of the stutter gate. 0 = no stutter audible even if Rate > 0. Display as percentage (0–100%) in the GUI. |

### 3.3 Hit Section

| # | Parameter | Code Name | Control Type | Range | Step | Default | Unit | Tooltip / UX Notes |
|---|---|---|---|---|---|---|---|---|
| 8 | **Hit Volume** | `kParamHitVolume` | Continuous knob | -60 – +6 | 0.1 | 0 | dB | Level of the original dry hit when it fires at the beat boundary. |

### 3.4 Global / Bottom Bar

| # | Parameter | Code Name | Control Type | Range | Step | Default | Unit | Tooltip / UX Notes |
|---|---|---|---|---|---|---|---|---|
| 9 | **Master Volume** | `kParamMasterVol` | Continuous knob | 0 – 100 | 0.01 | 100 | % | Overall output level. NOTE: This is a percentage (0–100%), NOT dB. 100% = unity. |

### 3.5 Debug / Developer Parameters
> These should be **hidden by default** in the release GUI. Exposed only via a secret gesture
> (e.g., Ctrl+click on the version label) or compiled out via `#ifdef RVRSE_DEBUG`.

| # | Parameter | Code Name | Control Type | Values | Default | Tooltip / UX Notes |
|---|---|---|---|---|---|---|
| 10 | **Debug Stage** | `kParamDebugStage` | Dropdown / Menu | Normal, Reverbed, Reversed, Riser Only | Normal | Selects which pipeline stage buffer to audition. "Normal" = full riser+hit. Other modes play intermediate buffers for diagnostics. Developer tool — not for end users. |

### 3.6 Future Parameters (NOT YET IMPLEMENTED — design space only)
> These are defined in the product brief but not yet wired in code. The GUI design should
> **reserve visual space** for them but they will NOT be functional in the first version.
> Mark them with a "coming soon" disabled appearance, or simply leave blank space.

| Parameter | Section | Type | Range | Notes |
|---|---|---|---|---|
| Riser Tune | Riser | Knob | -24 – +24 st | Pitch-shifts the riser. Beads issue `rverse-bzs`. |
| Hit Tune | Hit | Knob | -24 – +24 st | Pitch-shifts the hit. Beads issue `rverse-bzs`. |

### 3.7 Non-Parameter UI Elements

| Element | Control Type | Description | Priority |
|---|---|---|---|
| **Load Sample** | IVButtonControl | Opens native OS file dialog for WAV/AIFF. Prominent — this is the first thing a new user does. | Critical |
| **Sample Name** | ITextControl | Shows loaded file: name, sample rate, channels, duration. E.g., "crash_01.wav (48kHz, stereo, 1.2s)". Shows "No sample loaded" when empty, "Loading..." during load, "Missing: filename" on error. | Critical |
| **BPM Display** | ITextControl | Shows host tempo (read-only). Format: "BPM: 120.0". Updates from DAW. | High |
| **Waveform Display** | Custom IControl | Shows the complete riser+hit waveform. Reversed riser on the left transitioning into the sharp hit transient on the right. Animated playhead during playback. This is the visual centrepiece. | High |
| **Hit Waveform Preview** | Custom IControl | Smaller waveform of just the original loaded sample (pre-processing). Gives visual feedback that a sample is loaded. | Medium |
| **MIDI CC Indicator** | Icon / glyph (◉) | Small indicator near Stutter Rate and Stutter Depth knobs. Lights up (accent color) when the knob is receiving external MIDI CC. Dim/hidden when not active. | Medium |
| **Processing Indicator** | Subtle animation or text | Shows when the offline pipeline is recalculating (e.g., after Lush or Length change). Could be a spinner near the waveform, or the waveform itself fading to indicate "stale". | Low |
| **Version** | ITextControl | Bottom-right corner, small, muted. Format: "RVRSE v0.1.0 (build date)". | Low |

---

## 4. Layout Specification (from product brief)

Three zones, top to bottom:

```
┌──────────────────────────────────────────────────────┐
│  RVRSE          [LOAD SAMPLE]    BPM: 120            │  ← Header bar
│  ────────────────────────────────────────────────     │
│  [ waveform: reversed riser ~~~~ | hit transient ]   │  ← Waveform display
├─────────────────────────┬────────────────────────────┤
│  RISER                  │  HIT                       │  ← Two-panel control area
│                         │                            │
│  Lush        [knob]     │  Volume      [knob]        │
│  Length      [knob]     │  (Tune)      [reserved]    │
│  Fade In     [knob]     │                            │
│  Riser Vol   [knob]     │  [ hit waveform preview ]  │
│  (Riser Tune)[reserved] │                            │
│  Quality     [toggle]   │                            │
│  ── real-time ────────  │                            │
│  Stutter Rate [knob] ◉  │                            │
│  Stutter Depth[knob] ◉  │                            │
├─────────────────────────┴────────────────────────────┤
│  Master [knob]                         RVRSE v0.1.0  │  ← Bottom bar
└──────────────────────────────────────────────────────┘
```

### Design Notes

- The **RISER panel** is the larger side (~60% width). It has more parameters.
- The **HIT panel** is simpler — just volume + a small waveform preview of the original sample.
- The **waveform display** spans the full width above the panels. It shows the complete playback sequence: reversed riser transitioning into the hit transient. A playhead indicator scrubs through during playback.
- **Stutter knobs** should visually stand apart (perhaps a subtle separator or grouping label "REAL-TIME") since they are MIDI CC controlled, unlike the offline knobs above them.
- **Reserved spaces** (shown in parentheses) are for future parameters (Riser Tune, Hit Tune). Design should accommodate them gracefully — either as visible disabled placeholders or as empty space that won't look awkward.
- Bottom bar is minimal — master volume on the left, version string on the right.
- The Debug Stage dropdown should be **invisible** in the default view.

---

## 5. Available iPlug2 IGraphics Controls

These are the **actual C++ widget classes** available. Your design must be implementable using ONLY these controls (or custom `IControl` subclasses drawn with NanoVG).

### Knobs (Rotary)
- **IVKnobControl** — Vector-drawn knob with label+value. Fully styleable (colors, roundness, arc style). **This is our primary control.**
- **ISVGKnobControl** — Knob rendered from an SVG file (rotated).
- **IBKnobControl** — Bitmap sprite-sheet knob (60-frame PNG filmstrip).

### Buttons & Toggles
- **IVButtonControl** — Vector button with text label. Supports action lambda.
- **IVToggleControl** — On/off toggle (checkbox style or custom).
- **IVSwitchControl** — Multi-state switch (cycle through states on click).
- **IVSlideSwitchControl** — Animated slide toggle (like iOS switch).
- **IVRadioButtonControl** — Radio button group.
- **IVTabSwitchControl** — Tab-style selector.

### Sliders
- **IVSliderControl** — Vertical or horizontal slider.
- **IVRangeSliderControl** — Dual-handle range slider.

### Text & Labels
- **ITextControl** — Static text label.
- **IEditableTextControl** — User-editable text field.
- **ICaptionControl** — Auto-updating param display (name + value).
- **IVLabelControl** — Styled vector label.

### Metering & Visualization
- **IVMeterControl** — Level meter (peak or RMS).
- **IVScopeControl** — Oscilloscope-style waveform.
- **IVSpectrumAnalyzerControl** — FFT spectrum display.
- **IVPlotControl** — Generic XY plot.
- **IVDisplayControl** — Waveform/LFO shape display.

### Layout & Grouping
- **IVPanelControl** — Background panel (rectangle with styling).
- **IVGroupControl** — Labeled group box around a region.

### Custom Drawing
- **ILambdaControl** — Draw anything with a lambda function + NanoVG API.
- **Custom IControl subclass** — Override `Draw(IGraphics& g)` for full NanoVG access: arcs, fills, gradients, rounded rects, text, images, shadows, bezier curves.

### Popup / File
- **PromptForFile()** — Native OS file dialog (already implemented in RVRSE).

---

## 6. Styling System (IVStyle)

All "IV" (iPlug Vector) controls share a common styling system:

```
IVStyle {
  showLabel: bool,        // Show parameter name above/below
  showValue: bool,        // Show current value
  colors: {
    kBG     — Background
    kFG     — Foreground (main color)
    kPR     — Pressed state
    kFR     — Frame/border
    kHL     — Highlight
    kSH     — Shadow
    kX1     — Extra 1 (accent)
    kX2     — Extra 2
    kX3     — Extra 3
  },
  labelText: IText,       // Font, size, color, alignment
  valueText: IText,       // Font, size, color, alignment
  roundness: float,       // 0.0 = sharp corners, 1.0 = fully rounded
  drawShadows: bool,
  drawFrame: bool,
  frameThickness: float
}
```

**Dark theme approach (see Section 7 for full palette):**
```
Main background:   #0A100A (brand Dark)
Panels:            #222D42 (brand Dark-Grey)
Primary text:      #FFFFFF (brand White)
Accent (knobs):    #FFCC53 (brand Primary — gold)
MIDI CC accent:    #4490FC (brand Alt-Accent — blue)
Knob track:        #1A2235
Separator:         #1A2235
```

---

## 7. Color Palette — SamuFL Brand + "Studio Dark" Theme

Based on the **SamuFL brand kit**, adapted for a dark studio environment.

### Brand Colors (Source of Truth)
| Name | Hex | Role in brand |
|---|---|---|
| Primary Accent | `#FFCC53` | **Gold** — signature brand color |
| Alt-Accent | `#4490FC` | **Blue** — secondary |
| Dark (Black) | `#0A100A` | Deepest background |
| Dark-Grey | `#222D42` | Panel backgrounds |
| Highlight | `#D8FDFB` | Light mint — sparingly |
| White | `#FFFFFF` | Text and contrast |

### Applied to Plugin GUI

#### Backgrounds
| Role | Hex | Source | Usage |
|---|---|---|---|
| Main background | `#0A100A` | Brand Dark | Plugin window fill — near-black |
| Panel background | `#222D42` | Brand Dark-Grey | Riser and Hit section panels |
| Header / footer | `#0E150E` | Slightly lighter than Dark | Visual layering |
| Waveform background | `#060A06` | Darker than Dark | High contrast for waveform |

#### Text
| Role | Hex | Source | Usage |
|---|---|---|---|
| Primary text | `#FFFFFF` | Brand White | Parameter values, section titles |
| Secondary text | `#8B95A5` | Derived | Labels, units, muted info |
| Subtle / disabled | `#4A5568` | Derived | Reserved param placeholders, version |

#### Accent Colors
| Role | Hex | Source | Usage |
|---|---|---|---|
| **Primary accent** | `#FFCC53` | **Brand Primary** | Knob arcs (active), Load Sample button, section headers, plugin title accent. This is the signature color — use it prominently. |
| Secondary accent | `#4490FC` | Brand Alt-Accent | MIDI CC indicator (◉), stutter section highlight, waveform playhead, links |
| Waveform riser | `#FFCC53` | Brand Primary | Riser portion of waveform (gold) |
| Waveform hit | `#4490FC` | Brand Alt-Accent | Hit transient portion (blue) |
| Playhead | `#D8FDFB` | Brand Highlight | Bright mint-white line with subtle glow |

#### Control Colors
| Role | Hex | Source | Usage |
|---|---|---|---|
| Knob track | `#1A2235` | Derived from Dark-Grey | Unswept arc background |
| Knob arc (active) | `#FFCC53` | Brand Primary | Swept arc = gold |
| Knob pointer | `#FFFFFF` | Brand White | Dot or line indicating position |
| Button idle | `#222D42` | Brand Dark-Grey | Load Sample button background |
| Button hover | `#2D3A52` | Lighter Dark-Grey | Hover state |
| Button pressed | `#0E150E` | Darker | Active press |
| Button text | `#FFCC53` | Brand Primary | "LOAD SAMPLE" text in gold |
| Separator line | `#1A2235` | Derived | Panel dividers, section separators |

### Rationale
- **Gold (#FFCC53)** as primary accent gives RVRSE a distinctive, premium identity — unusual in the sea of blue-accented plugins
- **Blue (#4490FC)** for MIDI CC indicators creates clear visual distinction between offline (gold) and real-time (blue) controls
- **Near-black (#0A100A)** background is very dark without being pure black — reduces harsh contrast
- **Mint highlight (#D8FDFB)** used sparingly for playhead and special emphasis — very high contrast on dark
- Gold riser waveform → blue hit waveform visually communicates the two phases

---

## 8. Typography Specification

Using **Roboto-Regular** (already loaded in the plugin) for everything.

| Element | Size | Color | Weight | Alignment |
|---|---|---|---|---|
| Plugin title "RVRSE" | 36–40 px | `#FFCC53` (gold accent) | Bold/Medium | Left |
| Section headers ("RISER", "HIT") | 14–16 px | `#FFCC53` at 60% opacity | Regular, uppercase, letter-spaced | Left |
| Knob labels | 11–12 px | `#8B95A5` | Regular | Center (below knob) |
| Knob values | 12–13 px | `#FFFFFF` | Regular | Center (inside/below knob) |
| Sample name | 13–14 px | `#8B95A5` | Regular | Left or center |
| BPM display | 13–14 px | `#FFFFFF` | Regular | Right |
| Version string | 11 px | `#4A5568` | Regular | Right |
| "REAL-TIME" separator label | 10 px | `#4490FC` at 60% opacity | Regular, uppercase | Left |

---

## 9. Fonts Available

- **Roboto-Regular** (already loaded in RVRSE) — Clean sans-serif for all text.
- **ForkAwesome** — Icon font (arrows, checkboxes, circles, etc.)
- **Fontaudio** — Audio-specific icons (filter types, waveforms, etc.)

---

## 10. Layout System

iPlug2 uses **IRECT** (rectangle) based positioning — NOT CSS/flexbox:

```
bounds.GetPadded(-10)           // Inset by 10px
bounds.GetGridCell(0, 2, 3)     // Grid cell (row 0, 2 rows, 3 cols)
bounds.FracRectVertical(0.3, true)   // Top 30%
bounds.GetCentredInside(200, 40)     // Centered 200×40 rect
bounds.GetFromTRHC(100, 20)          // Top-right corner, 100×20
bounds.SubRectVertical(4, 2)         // 3rd quarter vertically
bounds.GetVShifted(-50)              // Shift up 50px
```

All positioning is in absolute pixels relative to parent IRECT. The layout is calculated in a `LayoutUI()` function that runs on resize.

---

## 11. What We Need From You (The Design Specialist)

### Deliverable Format

Since this will be implemented in C++ (not HTML/CSS), please provide:

1. **A detailed layout specification** with:
   - Exact pixel dimensions and positions for each zone and control
   - Or proportional layout rules (e.g., "header = top 8%, waveform = next 25%, panels = next 58%, footer = bottom 9%")
   - Spacing, padding, margins between elements

2. **Color palette** — Exact hex values for:
   - Background colors (main, panels, header, footer)
   - Text colors (titles, labels, values, subtle/muted)
   - Accent color(s) for knobs, active states, highlights
   - Waveform colors (riser, hit, playhead, grid)

3. **Typography spec:**
   - Font sizes for: title, section headers, knob labels, knob values, sample info, version
   - Text alignment per element

4. **Control styling:**
   - Knob style: arc-only? filled arc? dot indicator? How big?
   - Button style: rounded? pill-shaped? outline or filled?
   - Toggle style for Stretch Quality
   - How stutter knobs differ visually (MIDI CC indicator)

5. **Waveform display spec:**
   - Colors for positive/negative amplitude
   - Playhead style (line? with glow?)
   - Grid lines or clean?
   - How riser and hit portions are visually distinguished

### Visual References

For inspiration, think:
- **FabFilter Pro-R** — clean dark reverb UI with clear parameter grouping
- **Valhalla VintageVerb** — minimal dark theme, clear knobs
- **Serum** by Xfer — modern dark with colored accents
- **Output Portal** — sleek dark UI with waveform displays

### Things to Avoid
- Skeuomorphic design (no photorealistic knobs, wood textures, etc.)
- Overly complex layouts — this plugin has ~10 knobs, it should feel clean
- Bright/light themes — studio standard is dark
- Tiny text — readability matters at 1024×768

---

## 12. Current State

The plugin currently has a minimal placeholder UI: title text, a Load Sample button, and a sample name display on a dark gray background. No knobs are exposed in the GUI yet (all parameters work via DAW automation / generic host UI only). The feature branch `feature/rverse-ebv-igraphics-gui` is ready for implementation.

### Logo
A **SamuFL brand logo** SVG is available in `RVRSE/resources/img/logo.svg`. It should be displayed in the header bar, either replacing the text "RVRSE" title or alongside it. SVG is ideal — resolution-independent, scales perfectly at any DPI. Use `pGraphics->LoadSVG(LOGO_FN)` to load it and `ISVGControl` to display it. Define `#define LOGO_FN "logo.svg"` in `config.h`.
