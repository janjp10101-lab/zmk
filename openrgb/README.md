# OpenRGB Mixed RGB for the Keychron V1 Ultra 8K

This branch adds protocol-v2 Mixed RGB controls to the Keychron V1 Ultra 8K
firmware while retaining the original Direct RGB mode as a fallback.

The first preset is the agreed three-layer design:

- 23 fixed orange-lit keys (the physical keycaps remain grey);
- a cyan-blue spiral across the other 59 LEDs;
- a yellow reactive splash blended over the cyan layer when keys are pressed.

OpenRGB configures the layers. The keyboard renders the animation locally, so
closing OpenRGB does not stop the effects. The new architecture is documented
in [MIXED_RGB_OVERLAY_ARCHITECTURE.md](MIXED_RGB_OVERLAY_ARCHITECTURE.md).

The OpenRGB plugin source and its Windows build workflow live in
[`openrgb-plugin/`](../openrgb-plugin/). The plugin adds a three-layer editor
with on/off and speed controls, plus a Mixed RGB device mode.

> **Personal project, shared as-is.** Custom firmware is do-it-yourself. Keep
> the stock firmware/recovery route available and do not flash an image for a
> different keyboard model.

## Build
Needs Docker. From the repo root:

```bash
./openrgb/build.sh                       # keychron_v1_ultra_ansi
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
sudo ./openrgb/openrgb_test.py count       # asks LED count
sudo ./openrgb/openrgb_test.py mixed-info  # verifies three-layer protocol
sudo ./openrgb/openrgb_test.py mixed-demo  # applies orange/cyan/yellow preset
sudo ./openrgb/openrgb_test.py demo        # legacy Direct RGB test
sudo ./openrgb/openrgb_test.py hold        # legacy Direct RGB hold test
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
