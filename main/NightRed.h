#pragma once

#include <cstdint>

// The red night mode: every colour becomes the red of the same brightness, so that the eye stays
// used to the dark (after the night palette of mkvesala/ESP32-Waveshare-SkippersWatch, MIT).
// Plain C++ for the host tests (tests/NightRedTest.cpp).
class NightRed
{
public:
    // One pixel in LVGL's RGB565 (red in the top bits). Near black, every channel below 0x20 of
    // 0xFF, stays black, so that the ground never glows; any other colour is at least the dimmest
    // red, so that a dark colour (the night gauge's blue) does not vanish.
    static uint16_t map( uint16_t colour );

    // A band of pixels in place.
    static void convert( uint16_t *pixels, int count );
};
