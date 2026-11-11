# Mixed RGB overlay test architecture

Status: experimental firmware and OpenRGB plugin both compile successfully from commit `43bbea7191e23a6d06d95b150c8c9042a39929e9`. Windows and Linux plugin artifacts are available. The keyboard behavior is not yet hardware-verified; treat this as a test build, not a finished release.

## Target

Replace the OpenRGB plugin's current continuously streamed Direct RGB workflow with a firmware-rendered Mixed RGB workflow. OpenRGB becomes the configuration UI; the keyboard renders the animation locally.

The first test preset is intentionally limited to three effects:

1. Layer 0 — fixed orange: a 23-LED mask, static orange, highest priority. Confirmed LED indices: `0, 5, 6, 7, 8, 13, 28, 29, 30, 43, 44, 45, 56, 57, 58, 59, 71, 72, 73, 74, 76, 77, 78`.
2. Layer 1 — cyan-blue spiral: covers every LED not in the orange mask.
3. Layer 2 — yellow reactive splash: reacts to keypresses on the non-orange LEDs and blends above the cyan base.

The physical keycaps remain grey. The orange/cyan/yellow describe illumination only.

## Existing firmware pieces

- `app/src/rgb/keychron/mixed_rgb.c` already runs a two-layer effect list.
- `app/src/rgb/keychron/keychron_rgb.c` already exposes layer/effect configuration and region data through Keychron's existing RGB command path.
- `app/src/rgb/keychron/per_key_rgb.c` already contains a reactive splash implementation driven by `g_last_hit_tracker`.
- `app/src/launcher/openrgb.c` currently exposes the separate `id_openrgb` (0x16) Direct RGB protocol.
- `app/src/rgb/rgb_matrix_drivers.c` now captures per-layer frames and composites cyan base, yellow splash, then orange foreground. Region bytes are membership masks (bit 0/1/2).

## Rendering change

Do not implement this as three exclusive regions. Convert the Mixed RGB region map to layer-membership masks and introduce a small per-frame layer buffer:

- clear three layer buffers at the beginning of a Mixed RGB frame;
- render each enabled layer into its own buffer;
- composite in a deterministic order;
- preserve the orange mask as a final, non-overwritable foreground;
- flush one final RGB value per LED to the existing strip driver.

The first compositor policy is:
- cyan is the base;
- yellow splash adds/blends over cyan while its reactive value is nonzero;
- orange fixed keys overwrite all other layers.

This avoids the current issue where each region writes straight to the strip and the last rendered layer simply wins.

## Host/firmware protocol

Keep the existing `id_openrgb` raw-HID transport and add a versioned Mixed RGB configuration command family. Do not stream 82 LED colors every frame from the PC.

The protocol needs commands to:
- query protocol version, LED count, layer count and supported test effects;
- switch between firmware Mixed RGB rendering and legacy Direct RGB;
- read/write a layer's effect, hue, saturation, speed and enable state;
- read/write each layer's LED membership mask in bounded chunks;
- request a save to persistent settings.

Use 32-byte reports, validate every layer/index/count before copying, and keep the legacy Direct commands available as a recovery path. The firmware reports protocol version 2. Legacy Direct commands remain available as a recovery path, but the new plugin requires protocol v2. The first OpenRGB UI should expose only the three agreed effects and their basic settings, not the full Keychron effect catalogue.

## OpenRGB plugin

The existing `naaraxi/keychron_ultra_openrgb` repository is archived and read-only, so development must happen in a writable fork/new plugin repository or in this repository's `openrgb/plugin` subtree.

The plugin should:
- detect the keyboard and verify the new protocol version;
- present a Mixed RGB mode instead of selecting Direct mode by default;
- show three layer panels with effect selection and basic controls;
- provide a key-layout mask editor for the fixed orange 23-key selection;
- send configuration only when the user changes a setting;
- leave the keyboard rendering independently if OpenRGB closes.

## Test order

1. Verify the new protocol can report 82 LEDs and three layers without changing lighting.
2. Set Mixed RGB and verify the keyboard keeps rendering after OpenRGB closes.
3. Test orange fixed mask alone.
4. Test cyan spiral alone.
5. Test yellow reactive splash alone.
6. Enable all three and verify priority/overlay behavior.
7. Build the OpenRGB controls around the proven protocol.

## Open item

The 23 orange-lit LED indices are encoded in `app/boards/shields/keychron_v1_ultra_ansi/rgb_matrix_config.h` and preselected in the OpenRGB panel. Firmware and plugin compilation pass, including Windows and Linux plugin builds. Physical keyboard behavior and the final OpenRGB/HID round trip still require hardware testing; see `MIXED_RGB_HARDWARE_TEST.md`. The original upstream plugin is preserved; a writable three-layer plugin implementation now lives in `openrgb/plugin/`. Its Linux CI build has passed. It exposes three effect layers, speed/enabled controls, the orange 23-key selector, and save-to-firmware.
