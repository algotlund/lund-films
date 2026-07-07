# Film Grain — a Fuse for DaVinci Resolve (Free)

A procedural film grain tool built as a **Fuse** (a Lua-based Fusion plugin).
Fuses run on the Fusion page, which ships with the free version of Resolve —
unlike third-party OFX plugins, which Resolve gates behind a Studio license.
So this works without buying Studio.

## How it works

1. Generates independent per-pixel noise from a coordinate hash (no shared
   random state, so it's safe to run multithreaded).
2. Blurs that noise with Fusion's native Gaussian blur to turn 1px noise
   into soft "clumped" grain — this is the **Size** control.
3. Composites the grain back onto the image with a gain that depends on
   the source pixel's luminance, via **Shadows / Midtones / Highlights**
   sliders — real film grain is denser in some tonal ranges than others,
   this reproduces that instead of flat noise everywhere.
4. **Color** blends between fully correlated grain (same noise value in
   R, G, B — looks monochrome, like classic B&W emulsion) and fully
   independent per-channel noise (colorful, chromatic grain).

## Install

Copy `FilmGrain.fuse` into your Fusion Fuses folder:

- **macOS**: `~/Library/Application Support/Blackmagic Design/DaVinci Resolve/Fusion/Fuses/`
- **Windows**: `C:\ProgramData\Blackmagic Design\DaVinci Resolve\Fusion\Fuses\`
- **Linux**: `~/.local/share/DaVinciResolve/Fusion/Fuses/`

(The Blackmagic SDK docs list a `Support/Fusion/Fuses` path, but on this
machine's install the actual folder is `Fusion/Fuses` with no `Support`
in between — confirmed by checking the installed app's folder tree.
Already copied there for you on this Mac.)

Restart Resolve. In the **Fusion page**, open the Effects Library and look
under `Fuses > Film` for **Film Grain**, or right-click in the node view →
`Add Tool > Fuses > FilmGrain`. Drop it after your source/color-corrected
node and wire it inline.

## Using it while grading (Color page)

A Fuse can't be dropped into the Color page's own node graph — that's an
OpenFX-only mechanism, and third-party OFX needs a Studio license anyway.
But you don't need that: in Resolve, a clip's pipeline order is
`Fusion → Color`, so anything you build in that clip's Fusion composition
is already baked in by the time you grade it on the Color page.

1. Select the clip in the Edit page timeline.
2. Click the **Fusion** page tab — it opens that clip's composition
   (creating MediaIn/MediaOut automatically if it's the first time).
3. Add `FilmGrain` between MediaIn and MediaOut.
4. Switch to the **Color** page for the same clip — the grain is already
   part of what you're seeing and grading. No extra step needed there.

## Controls

| Control | Range | What it does |
|---|---|---|
| Amount | 0–3 | Master grain intensity |
| Size | 0.1–5 | Apparent grain size (blur radius) |
| Complexity | 1–6 | Number of fractal noise octaves layered together - higher gives richer, more organic-looking texture at the cost of render speed |
| Roughness | 0–1 | Per-octave amplitude falloff (persistence). Low = smoother, dominated by the base octave; high = busier/rougher, finer octaves contribute more |
| Color | 0–1 | 0 = monochrome grain, 1 = fully chromatic |
| Red/Green/Blue Amount | 0–2 each | Per-channel grain weighting. Defaults (0.8 / 1.0 / 1.3) mimic real color negative stock, where the blue-sensitive emulsion layer typically shows the coarsest/most visible grain and red the smoothest |
| Shadows | 0–2 | Grain amount in dark tones |
| Midtones | 0–2 | Grain amount in mid tones |
| Highlights | 0–2 | Grain amount in bright tones |
| Seed | 0–1000 | Changes the noise pattern (useful to de-sync two stacked instances) |
| Animate Per Frame | on/off | Off = static grain (useful when comparing/tuning), on = new grain every frame like real film |
| Show Grain Only | on/off | Isolates the grain layer against mid-gray, for dialing in Size/Amount |

## Editing / iterating

Fuses recompile on the fly — edit `FilmGrain.fuse` in any text editor,
save, and the tool reloads in Resolve without a restart (only adding a
*new* Fuse for the first time requires a restart).

## Performance note

This is Lua, not compiled OFX/DCTL, so it's slower per frame — the noise
generation and compositing passes are multithreaded (`MultiProcessPixels`)
but still won't match a native GPU shader. Use proxy/quick-preview quality
while grading, then let it render at full quality. If it's too slow on 4K
timelines, lower Size (smaller blur kernel) or duplicate the node less.

---

# Bleach Bypass — a Fuse for DaVinci Resolve (Free)

Digital emulation of the bleach bypass (silver retention / skip bleach)
film processing technique — the desaturated, high-contrast, gritty
"silvery" look from films like *Saving Private Ryan* and *Se7en*. Real
bleach bypass skips the bleaching step of color development, leaving
metallic silver in the emulsion alongside the dye layers: increased
contrast, muted color, deep blacks, and often a slight shift toward cyan.

Unlike the grain Fuse, this one is built entirely from Fusion's native
fast Image operations (`Saturate`, `Gamma`, `ChannelOpOf`, `MergeOf` with
`MO_ApplyMode = "Overlay"`) — no per-pixel scripting, so it's faster and
sidesteps the multithreading issues that came up while building grain.

Install the same way as Film Grain (see above) — `BleachBypass.fuse`
goes in the same Fuses folder. Restart Resolve to pick up the new tool
for the first time; after that it hot-reloads like any Fuse.

## Controls

| Control | Range | What it does |
|---|---|---|
| Amount | 0–1 | Overall effect strength - mixes between untouched original and full effect |
| Desaturation | 0–1 | How monochrome the blended "silver" layer is |
| Silver Density | 0–1 | Extra contrast pushed into that layer before blending, standing in for the density retained silver adds on real film |
| Cyan Shift | 0–1 | Subtle color balance shift toward cyan, leaning into shadows (a characteristic of the real process) |
| Blend Mode | Overlay / Soft Light / Hard Light | Overlay is the standard/classic choice; Soft Light is gentler, Hard Light more aggressive |
