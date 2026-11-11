#pragma once
#ifndef NO_LED
#define NO_LED 0xFFFFFFFFu
#endif
struct KeychronLayout {
    unsigned short pid;
    const char*    name;         // e.g. "Keychron V6 Ultra 8K"
    const char*    description;  // "<Board> Ultra (custom ZMK firmware, OpenRGB direct control)"
    unsigned int   led_count;
    unsigned int   map_height;
    unsigned int   map_width;
    unsigned int*  matrix_map;   // map_height*map_width, row-major, NO_LED = gap
    const char**   led_names;    // led_count entries
};
extern const KeychronLayout KEYCHRON_LAYOUTS[];
extern const unsigned int   KEYCHRON_LAYOUT_COUNT;
