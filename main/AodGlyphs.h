#pragma once

// The always-on face's bitmaps, made by tools/make_aod_glyphs.py (main/AodGlyphs.c). One bit per
// pixel: the panel's idle mode shows each colour channel only on or off, so no grey levels are
// kept.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Rows one after another without padding, most significant bit first.
typedef struct
{
    int16_t width;
    int16_t height;
    const uint8_t *bits;
} AodGlyph;

// The digits and the colon of a digital clock at the Large template's size.
typedef struct
{
    AodGlyph digits[11];  // 0 to 9, then the dash of "--:--" (no valid time): AOD_DASH
    AodGlyph colon;
    // Top-left corners of the two hour digits, the colon and the two minute digits in the time
    // block.
    int16_t x[5];
    int16_t y[5];
    int16_t width;
    int16_t height;
} AodStyle;

// aodStyles[AOD_SIZE_* * 2 + FaceConfig::DigitalStyle]: JetBrains Mono Light, seven segments
// filled with dots; Large (122 px), Medium (64 px, the arc templates).
enum
{
    AOD_SIZE_LARGE = 0,
    AOD_SIZE_MEDIUM = 1,
    AOD_STYLE_COUNT = 4,
    AOD_SMALL_COUNT = 108,
    AOD_DASH = 10
};

extern const AodStyle aodStyles[AOD_STYLE_COUNT];

// The slot texts' characters: the watch font's set at 26 px, each aodSmallAdvance wide and the
// line aodSmallHeight high (the glyphs share its top); aodSmallCodes holds their code points.
extern const uint16_t aodSmallCodes[AOD_SMALL_COUNT];
extern const AodGlyph aodSmall[AOD_SMALL_COUNT];
extern const int aodSmallAdvance;
extern const int aodSmallHeight;

#ifdef __cplusplus
}
#endif
