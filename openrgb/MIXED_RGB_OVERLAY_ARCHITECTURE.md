# Three-layer Mixed RGB architecture

This branch combines the V1 Ultra's existing Keychron Mixed RGB engine with a frame compositor and an OpenRGB layer editor.

## Preset and key layout

The V1 Ultra ANSI matrix has 82 LEDs. Its existing `default_region` map is retained:

- Region 0: 23 LEDs, fixed orange.
- Region 1: 59 LEDs, cyan-blue spiral base.
- The firmware derives membership in layer 2 for those same 59 region-1 LEDs, allowing yellow reactive splash to overlay the cyan base.

The physical keycaps are grey. These are lighting colors only.

## Frame pipeline

1. `mixed_rgb_update_region_masks()` converts the saved per-key region IDs into layer membership bits.
2. `mixed_rgb()` begins a fresh three-layer frame.
3. Layer 0 renders static orange.
4. Layer 1 renders the cyan-blue spiral brightness effect.
5. Layer 2 renders yellow only where the keypress wave is active.
6. `zmk_rgb_matrix_mixed_frame_flush()` composites the three layer buffers and writes one final color per LED.

Composition priority is cyan base, yellow alpha blend, then orange fixed-key foreground. The orange 23-key group cannot be overwritten by the animated layers.

## OpenRGB control path

OpenRGB uses the existing raw-HID `id_openrgb` channel (0x16), protocol version 2. The legacy Direct RGB commands remain available.

New commands expose:
- Mixed RGB layer count and effect slots;
- per-layer effect configuration (effect, hue, saturation, speed, duration);
- region read/write;
- persistence via the existing Keychron RGB save handler;
- firmware Mixed RGB mode selection.

The plugin's Settings panel exposes only the agreed initial effects:
- Layer 1: static orange;
- Layer 2: cyan-blue spiral;
- Layer 3: yellow reactive splash.

The panel provides on/off and speed controls. Applying settings switches the device to firmware Mixed RGB mode. Saving persists the layer configuration. The keyboard renders locally; OpenRGB does not stream animation frames.

## Build outputs

The test workflow targets `keychron_v1_ultra_ansi`. The plugin workflow builds the Windows x64 DLL against OpenRGB 1.0rc3 / plugin API v4.
