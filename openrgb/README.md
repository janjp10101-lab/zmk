# OpenRGB Mixed RGB test for the Keychron V1 Ultra 8K

This branch adds an OpenRGB configuration protocol and a three-layer firmware
compositor for the Keychron V1 Ultra 8K. OpenRGB configures the effects; the
keyboard renders them locally, so it does not need a PC-driven frame stream.

> **Personal project, shared as-is.** I built this for my own keyboard and put
> it on GitHub in case it's useful to someone else. No support, warranty, or
> maintenance is promised - use it at your own risk, and understand that
> flashing custom firmware is inherently do-it-yourself.

It rides the existing raw-HID channel with `id_openrgb` (0x16). The legacy
Direct RGB commands remain available, while protocol v2 adds Mixed RGB layer
configuration. The first preset is fixed orange keys, a cyan-blue spiral, and
a yellow reactive splash. Three overlapping layers are composited in firmware:
cyan base, yellow reactive blend, then orange fixed-key priority. The 23 orange
LED indices are selected in the test harness/plugin rather than guessed.

The original Direct-mode plugin is archived and read-only. The new Mixed RGB
plugin UI will be developed separately against this firmware protocol after
the first V1 build and on-keyboard test pass.

## What changed (vs upstream `rtl8762g`)
- `app/src/launcher/openrgb.c` - version 2 Mixed RGB configuration commands.
- `app/src/rgb/keychron/mixed_rgb.c` - three effect layers.
- `app/src/rgb/rgb_matrix_drivers.c` - per-layer frame buffers and compositor.
- `app/src/rgb/keychron/keychron_rgb.c` - orange/cyan/yellow test preset and
  versioned settings keys; legacy Direct mode remains as a fallback.

## Build
Needs Docker. From the repo root:

```bash
./openrgb/build.sh                       # keychron_v6_ultra_ansi
./openrgb/build.sh keychron_q6_ultra_ansi   # another Ultra shield
```

First run initializes the west workspace (clones zephyr/ + modules/ into the
repo) inside the toolchain container. Output: `openrgb/output/<shield>.bin` - an
OTA-flashable image with the same header format as Keychron's CDN firmware
(SHA-256 checksum, plaintext/unsigned - self-built images are accepted).

## Flash
The Keychron Launcher only flashes *official* images, so use the included host
flasher (Realtek SC-FWU over `/dev/hidraw`). It finds your keyboard on its own
and works on any Ultra board. Plug the keyboard in via USB:

```bash
sudo ./openrgb/flash.py handshake                       # read-only: identify only
sudo ./openrgb/flash.py flash openrgb/output/keychron_v6_ultra_ansi.bin
```

If you have two Keychron boards plugged in, it will stop and ask which one, so
it cannot flash the wrong keyboard by accident. Pass `--device=/dev/hidrawN` to
choose.

Flashing is **brick-safe**: the image is staged to a separate "OTA Tmp" bank and
only activated after the device verifies its CRC - an interrupted or bad flash
leaves the running firmware untouched. Recovery: the official 1.0.2 image is
still on Keychron's CDN and can be reflashed via the Launcher.

## Test
`openrgb_test.py` talks to the keyboard directly, so you can check the firmware
without installing OpenRGB. It works on any Ultra board, because it asks the
keyboard how many LEDs it has instead of assuming.

```bash
sudo ./openrgb/openrgb_test.py count    # just asks the LED count, nothing lights up
sudo ./openrgb/openrgb_test.py demo     # cycles red/green/blue/white across all keys
sudo ./openrgb/openrgb_test.py hold     # holds one color for a while
```

If `count` gets an answer, the OpenRGB firmware is running. Stock firmware does
not know the command and replies `0xFF`.

## Running without sudo (optional)
Both scripts talk to `/dev/hidraw`, which normally only root can open. If you
would rather not use `sudo` every time, install the udev rule included here:

```bash
sudo cp openrgb/61-keychron-ultra-openrgb.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Then unplug and replug the keyboard. This is optional - `sudo` works fine
without it. The same rule also lets OpenRGB reach the keyboard while running as
your normal user.

## Keeping up with upstream
```bash
git fetch origin && git merge origin/rtl8762g
```
