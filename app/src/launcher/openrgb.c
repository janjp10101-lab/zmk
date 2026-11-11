/*
 * OpenRGB direct-control command handler (ZMK issue #893).
 *
 * Rides the existing raw-HID (VIA) channel: raw_hid_receive() dispatches
 * data[0] == id_openrgb here. The response is written in place into `data`;
 * raw_hid_receive() sends it back. Packets are RAW_EPSIZE (32) bytes.
 *
 * Wire format:  data[0]=id_openrgb, data[1]=subcommand, data[2..]=args.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stddef.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "launcher.h"
#include "../rgb/rgb_matrix.h"

#define OPENRGB_PROTOCOL_VERSION 1 /* additive Mixed RGB commands preserve v1 direct compatibility */

// LED position/flag table (for OpenRGB's key-to-LED map).
extern led_config_t g_led_config;

enum openrgb_subcommand {
    openrgb_get_protocol_version = 0x00, // -> args[0] = protocol version
    openrgb_get_led_count = 0x01,        // -> args[0..1] = LED count (LE)
    openrgb_set_direct_mode = 0x02,      // args[0] = 1 enter / 0 exit
    openrgb_set_leds = 0x03,             // args[0]=start, args[1]=count, args[2..]=RGB triples
    openrgb_get_led_info = 0x04,         // args[0]=index -> args[1..3] = x, y, flags
    openrgb_mixed_get_info = 0x10,       // -> layer count, effects per layer
    openrgb_mixed_get_regions = 0x11,    // args[0]=start, args[1]=count -> membership masks
    openrgb_mixed_set_regions = 0x12,    // args[0]=start, args[1]=count, args[2..]=masks
    openrgb_mixed_get_effects = 0x13,    // args[0]=layer, args[1]=start, args[2]=count
    openrgb_mixed_set_effects = 0x14,    // args[0]=layer, args[1]=start, args[2]=count, then 8-byte records
    openrgb_set_mixed_mode = 0x15,       // args[0]=1 enter Mixed RGB, 0 restore previous mode
    openrgb_mixed_save = 0x16,           // persist current layer masks and effects
};

static uint8_t openrgb_previous_mode = RGB_MATRIX_DEFAULT_MODE;

static void openrgb_forward_mixed_command(uint8_t *data, uint8_t length) {
    extern void kc_rgb_matrix_rx(uint8_t *data, uint8_t length);
    uint8_t original = data[1];
    data[1] = (uint8_t)(11 + original - openrgb_mixed_get_info);
    kc_rgb_matrix_rx(data, length);
    data[1] = original;
}

void openrgb_handle_command(uint8_t *data, uint8_t length) {
    uint8_t *sub = &data[1];
    uint8_t *args = &data[2];

    switch (*sub) {
    case openrgb_get_protocol_version:
        args[0] = OPENRGB_PROTOCOL_VERSION;
        break;

    case openrgb_get_led_count:
        args[0] = (uint8_t)(RGB_MATRIX_LED_COUNT & 0xFF);
        args[1] = (uint8_t)((RGB_MATRIX_LED_COUNT >> 8) & 0xFF);
        break;

    case openrgb_set_direct_mode:
        if (args[0]) {
            zmk_rgb_matrix_openrgb_enter();
        } else {
            zmk_rgb_matrix_openrgb_exit();
        }
        break;

    case openrgb_set_leds: {
        uint8_t start = args[0];
        uint8_t count = args[1];
        uint8_t *rgb = &args[2]; // == &data[4]

        if (!zmk_rgb_matrix_openrgb_active()) {
            zmk_rgb_matrix_openrgb_enter();
        }

        for (uint8_t i = 0; i < count; i++) {
            size_t last = 6 + (size_t)i * 3; // absolute index of this triple's blue byte
            if (last >= length) {
                break; // would overrun the 32-byte packet
            }
            uint16_t led = (uint16_t)start + i;
            if (led >= RGB_MATRIX_LED_COUNT) {
                break;
            }
            if (os_indicator_owns_led(led)) {
                continue; // caps/num lock overlay owns this key; don't fight it (flicker)
            }
            uint8_t off = i * 3;
            zmk_rgb_matrix_set_color(led, rgb[off], rgb[off + 1], rgb[off + 2]);
        }
        zmk_rgb_matrix_openrgb_feed(); // pet the auto-hand-back watchdog
        break;
    }

    case openrgb_get_led_info: {
        uint8_t idx = args[0];
        if (idx < RGB_MATRIX_LED_COUNT) {
            args[1] = g_led_config.point[idx].x;
            args[2] = g_led_config.point[idx].y;
            args[3] = g_led_config.flags[idx];
        }
        break;
    }

    case openrgb_mixed_get_info:
    case openrgb_mixed_get_regions:
    case openrgb_mixed_set_regions:
    case openrgb_mixed_get_effects:
    case openrgb_mixed_set_effects:
        openrgb_forward_mixed_command(data, length);
        break;

    case openrgb_mixed_save: {
        extern void kc_rgb_matrix_rx(uint8_t *data, uint8_t length);
        uint8_t original = data[1];
        data[1] = 2; // Keychron RGB_SAVE command
        kc_rgb_matrix_rx(data, length);
        data[1] = original;
        break;
    }

    case openrgb_set_mixed_mode:
#ifdef CONFIG_KEYCHRON_RGB_ENABLE
        if (args[0]) {
            if (zmk_rgb_matrix_get_mode() != RGB_EFFECT_MIXED_RGB) {
                openrgb_previous_mode = zmk_rgb_matrix_get_mode();
            }
            zmk_rgb_matrix_openrgb_exit();
            zmk_rgb_matrix_mode(RGB_EFFECT_MIXED_RGB);
        } else {
            zmk_rgb_matrix_openrgb_exit();
            zmk_rgb_matrix_mode(openrgb_previous_mode == RGB_EFFECT_MIXED_RGB
                                    ? RGB_MATRIX_DEFAULT_MODE
                                    : openrgb_previous_mode);
        }
#else
        data[0] = id_unhandled;
#endif
        break;

    default:
        data[0] = id_unhandled;
        break;
    }
}
