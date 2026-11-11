# Mixed RGB hardware test checklist

The firmware and Windows OpenRGB plugin both compile successfully from commit `43bbea7191e23a6d06d95b150c8c9042a39929e9`. This is an experimental test build, not yet hardware-verified.

## Before flashing

- Keep a copy of the currently working firmware and know the recovery/DFU procedure for the V1 Ultra.
- Do not use this build on another Keychron model; it is built specifically for the V1 Ultra ANSI (82 LEDs, PID `0x0c10`).
- Download the firmware artifact from the matching GitHub Actions run and extract the `zmk.bin` file.
- Download the Windows OpenRGB plugin artifact and extract `OpenRGBKeychronUltraPlugin.dll`.

## First test

1. Flash the experimental firmware using the same verified recovery/update method used for the current custom firmware.
2. Reconnect the keyboard over USB and open OpenRGB.
3. Install the plugin DLL in the OpenRGB plugin directory used by the installed OpenRGB build, then restart OpenRGB.
4. Confirm the device is detected as Keychron V1 Ultra 8K and the plugin reports protocol v2 with 3 Mixed RGB layers.
5. Open the Keychron Mixed RGB plugin panel. The orange selector should show 23 preselected keys.
6. Apply and save the three-layer preset.

## Verify the effects separately

- Layer 1: 23 selected LEDs are static orange.
- Layer 2: the remaining LEDs run the cyan-blue spiral.
- Layer 3: keypresses produce a yellow reactive splash over the cyan-blue base.
- All three enabled: orange remains protected while yellow reacts over cyan on the other LEDs.
- Close OpenRGB: the keyboard should continue rendering Mixed RGB locally.

## Recovery

If the keyboard does not behave normally, disconnect/reconnect and use the known V1 Ultra DFU/recovery procedure to restore the saved working firmware. Do not repeatedly flash experimental builds without confirming the recovery path.
