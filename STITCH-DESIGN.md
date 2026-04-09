# Design System Specification: Premium Audio Engineering Interface

## 1. Overview & Creative North Star
**Creative North Star: "The Obsidian Precision"**

This design system moves away from the cluttered, hardware-emulating aesthetics of legacy audio software. Instead, it adopts a "Digital-First Editorial" approach. We are not building a virtual rack; we are building a precision instrument for light and sound. 

The aesthetic is defined by **tonal depth** and **intentional void**. By utilizing a near-black green base and deep blue-grey panels, we create a "recessed" visual environment that reduces eye strain in dark studio settings. The UI breaks the "template" look by eschewing traditional borders in favor of overlapping surface tiers and high-contrast "Gold" focal points. Every element must feel like it was carved out of a single block of dark glass, with light only appearing where data or interaction exists.

---

## 2. Colors & Surface Logic
The palette is rooted in low-frequency greens and blues to maintain a "stealth" presence, allowing the Gold (`primary_container`) and Blue (`secondary`) accents to serve as the only sources of visual energy.

### The "No-Line" Rule
**Strict Mandate:** Designers are prohibited from using 1px solid strokes to define sections. 
*   **How to separate:** Use background shifts. Place a `surface_container` (#1B211B) module inside a `surface` (#0F150F) background. 
*   **The Goal:** Content should feel like it is floating in a structured void, not trapped in a cage.

### Surface Hierarchy & Nesting
Depth is achieved through a "Stacked Obsidian" metaphor:
*   **Base Layer (`surface_container_lowest` / #0A100A):** The master canvas.
*   **Module Layer (`surface_container` / #1B211B):** Primary panel areas.
*   **Active Interaction Layer (`surface_container_highest` / #30362F):** Hover states or focused parameter groups.
*   **Waveform Bed (`surface_dim` / #0F150F):** Recessed area for real-time data visualization.

### The "Glass & Gradient" Rule
To elevate the UI beyond a flat vector look, use **subtle linear gradients** on primary controls. 
*   *Example:* A Gold button should transition from `#FFEDCE` (top) to `#FFCC53` (bottom) at a 15% opacity overlay to give it a "backlit" feel without resorting to skeuomorphism.
*   *Floating Modals:* Use `surface_bright` (#343B33) with a 20px backdrop-blur to imply a physical glass layer above the interface.

---

## 3. Typography
We use a single typeface—Roboto—but we treat it with editorial intent. The hierarchy is driven by extreme scale variance rather than weight alone.

*   **Display / Headline:** Used for the logo mark and major mode titles (e.g., "OSCILLATOR"). Set with tight tracking (-2%) to feel modern and "engineered."
*   **Title (Gold / #FFCC53):** Used for parameter groups. This is the "Anchor" text that guides the user's eye.
*   **Body (Secondary Text / #8B95A5):** Low-contrast for non-essential information, ensuring the interface doesn't feel "noisy."
*   **Label (Muted / #4A5568):** Smallest scale (11px-12px) for secondary units like "ms" or "dB."

---

## 4. Elevation & Depth
We reject drop shadows in favor of **Tonal Layering**.

*   **The Layering Principle:** Instead of a shadow, a "raised" element is simply 2-3 shades lighter than the surface below it. 
*   **Ambient Shadows:** Use only for floating context menus. 
    *   *Spec:* Blur: 32px, Opacity: 8%, Color: `#000000`. This mimics the way light disappears behind an object in a dark room.
*   **The "Ghost Border" Fallback:** If a boundary is required for clarity (e.g., a dropdown list), use `outline_variant` (#4E4635) at **15% opacity**. It should be felt, not seen.

---

## 5. Components

### Knobs & Sliders (The Core Interface)
*   **The Track:** Use `surface_container_highest` (#30362F). No inner shadows.
*   **The Indicator:** Use `primary_fixed` (#FFDF9C). When a parameter is modulated, the secondary "Blue" (#4490FC) indicator should appear as a thin 2px arc inside the primary ring.
*   **Spacing:** Every control must have a minimum of 16px "breathing room" to ensure precision touch/mouse interaction.

### Buttons (The Action Set)
*   **Primary (Gold):** Flat fill using `#FFCC53`. Text is `#3F2E00` (High contrast).
*   **Secondary (Ghost):** No fill. `outline` color at 20% opacity. Text is white.
*   **Corner Radius:** Consistently **6px** (`md: 0.375rem`) for all containers and buttons to maintain a "technical-soft" feel.

### Waveforms & Visualizers
*   **Background:** `#060A06`.
*   **Primary Data:** Solid Blue (`#4490FC`) for real-time output.
*   **History/Ghosting:** Use `secondary_container` (#036ED8) at 30% opacity to show previous states.

### Cards & Modules
*   **Separation:** Forbid the use of divider lines. Separate modules using 24px vertical white space or a shift from `surface_container_low` to `surface_container`.

---

## 6. Do's and Don'ts

### Do
*   **Do** use asymmetrical layouts for the header/footer to break the "boxed-in" feel.
*   **Do** use the Blue accent (#4490FC) *only* for movement and data—never for static labels.
*   **Do** prioritize "Void" (empty space). If a panel feels crowded, increase the padding rather than adding a border.

### Don't
*   **Don't** use 100% white (#FFFFFF) for everything. Reserve it for active values; use `on_surface_variant` (#D2C5AF) for general labels to keep the "dark studio" vibe.
*   **Don't** use "Drop Shadows" on knobs. Use color gradients to imply the "top" of the control.
*   **Don't** use Roboto Bold for body text. Use Medium weight and rely on the Gold color for emphasis.

---

## 7. Interaction States
*   **Idle:** Low contrast, `on_surface_variant` text.
*   **Hover:** Surface glows slightly (shift to `surface_bright`).
*   **Active/Focused:** 1px `Ghost Border` (Gold at 40% opacity) appears around the component.
*   **Disabled:** Opacity reduced to 30%, saturation removed.