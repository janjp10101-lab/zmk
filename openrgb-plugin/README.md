# Keychron Ultra OpenRGB plugin

This copy adds a **Mixed RGB** device mode to the original Keychron Ultra OpenRGB plugin.

- **Direct**: OpenRGB streams individual LED colors to the keyboard.
- **Mixed RGB**: OpenRGB hands control back to the firmware and selects the native Mixed RGB effect. The companion firmware configures 23 orange keys, a cyan-blue spiral on the remaining 59 keys, and a yellow reactive splash overlay.

The plugin targets OpenRGB 1.0rc3 / plugin API v4, matching the original plugin. Build artifacts are produced by the `build-openrgb-plugin` GitHub Actions workflow.

Install the resulting `OpenRGBKeychronUltraPlugin.dll` through OpenRGB → Settings → Plugins. The keyboard must be running firmware from this repository's `feature/cyan-blue-splash-overlay` branch (or a later compatible build).
