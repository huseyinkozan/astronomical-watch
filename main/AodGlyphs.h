#pragma once

// The always-on face's bitmaps, made by tools/make_aod_glyphs.py (main/AodGlyphs.c, C so that
// the LVGL font structures can use designated initializers). One bit per pixel: the panel's idle
// mode shows each colour channel only on or off, so no grey levels are kept.

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Rows one after another without padding, most significant bit first (LVGL's 1-bpp glyphs).
typedef struct
{
    int16_t width;
    int16_t height;
    const uint8_t *bits;
} AodGlyph;

typedef struct
{
    // The same digits and colon as an LVGL font (codes '0' to ':'), for the always-on page of
    // active mode; each glyph hangs from the top of the line.
    const lv_font_t *font;
    AodGlyph digits[10];
    AodGlyph colon;  // width 0: no colon (HH over MM)
    // Top-left corners of the two hour digits, the colon and the two minute digits in the time
    // block (-1: not drawn).
    int16_t x[5];
    int16_t y[5];
    int16_t width;
    int16_t height;
    // Seven-segment styles: the moon, date and battery are dotted too.
    bool dottedItems;
} AodStyle;

enum
{
    AOD_STYLE_COUNT = 14,
    AOD_SMALL_COUNT = 33
};

// The low-battery mark, the last of the small glyphs.
#define AOD_BATTERY_CODE 0xE000

extern const AodStyle aodStyles[AOD_STYLE_COUNT];

// The date's characters (a space, digits and the letters of the day names) and the battery mark,
// solid and dotted (every second pixel of every second row). The characters advance by
// aodSmallAdvance; aodSmallCodes holds their code points.
extern const uint16_t aodSmallCodes[AOD_SMALL_COUNT];
extern const AodGlyph aodSmall[AOD_SMALL_COUNT];
extern const AodGlyph aodSmallDotted[AOD_SMALL_COUNT];
extern const int aodSmallAdvance;
extern const lv_font_t aodSmallFont;
extern const lv_font_t aodSmallDottedFont;

#ifdef __cplusplus
}
#endif
