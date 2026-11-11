# Keychron Ultra OpenRGB plugin

This copy adds a **three-layer Mixed RGB editor** and a Mixed RGB device mode to the Keychron Ultra OpenRGB plugin.

- **Direct**: OpenRGB streams individual LED colors to the keyboard.
- **Mixed RGB**: OpenRGB hands rendering back to the firmware and exposes controls for all three layers: 23 fixed orange keys, a cyan-blue spiral across the other 59 keys, and a yellow reactive splash overlay.

The plugin targets OpenRGB 1.0rc3 / plugin API v4, matching the original plugin. Build artifacts are produced by the `build-openrgb-plugin` GitHub Actions workflow.

Install the resulting `OpenRGBKeychronUltraPlugin.dll` through OpenRGB → Settings → Plugins. The keyboard must be running firmware from this repository's `mixed-rgb-overlay-test-v2` branch (or a later compatible build).


In OpenRGB, select the Keychron device and use its mode selector:

- **Direct** for software-driven per-key output.
- **Mixed RGB** to hand control back to the keyboard and activate the native three-layer effect stack.

The plugin's Mixed RGB panel lets you enable/disable each agreed layer and adjust the cyan/yellow animation speeds. **Apply layers** sends the configuration and switches the keyboard to Mixed RGB; **Save to keyboard** persists it. The host sends settings only when you apply them and does not stream LED frames while the animation is running. The firmware's default key-region map preserves the 23 orange keys.
