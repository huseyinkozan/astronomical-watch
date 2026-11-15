#pragma once

// The pixels of the corner gauges' ring, made by tools/make_corner_table.cpp (main/CornerTable.c):
// those of the top-left corner's box, the other corners being its mirror images
// (Corners::geometry). In flash, not RAM: CornerView computes the active gauges' colours from
// them while LVGL draws, AlwaysOn the always-on gauges, so both looks light the same pixels.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The box at each corner that holds the ring and the active gauge's round ends.
#define CORNER_TABLE_BOX 125
// Along a gauge: thousandths of its length times this.
#define CORNER_ALONG_SCALE 32

#define CORNER_AOD_RING 1   // lit by the always-on gauge
#define CORNER_AOD_NOTCH 2  // one of the always-on gauge's notches

typedef struct
{
    int16_t along;    // from the top-left gauge's lower end; below 0 or above 32000 past its ends
    int8_t radial;    // from the active gauge's centre line, outwards, in 1/16 px
    uint8_t channel;  // active look: the share of the pixel the channel covers, 0..255
    uint8_t notch;    // active look: the share the middle notch covers, 0..255
    uint8_t flags;    // CORNER_AOD_RING, CORNER_AOD_NOTCH
} CornerPixel;

// Per row of the top-left box, its pixels from column `first` on, `count` of them, starting at
// cornerPixels[offset].
extern const uint8_t cornerRowFirst[CORNER_TABLE_BOX];
extern const uint8_t cornerRowCount[CORNER_TABLE_BOX];
extern const uint16_t cornerRowOffset[CORNER_TABLE_BOX];
extern const CornerPixel cornerPixels[];

// The ring pixel at (x, y) of a width x height screen, or NULL. `corner` gets its corner
// (Corners::Corner) and `along` its place along that corner's gauge from the gauge's lower end.
static inline const CornerPixel *cornerTablePixel( int width, int height, int x, int y, int *corner, int *along )
{
    const int right = x >= width - CORNER_TABLE_BOX;
    const int bottom = y >= height - CORNER_TABLE_BOX;
    if ( ( x >= CORNER_TABLE_BOX && !right ) || ( y >= CORNER_TABLE_BOX && !bottom ) ) {
        return NULL;
    }
    const int column = right ? width - 1 - x : x;
    const int row = bottom ? height - 1 - y : y;
    if ( column < cornerRowFirst[row] || column >= cornerRowFirst[row] + cornerRowCount[row] ) {
        return NULL;
    }
    const CornerPixel *pixel = &cornerPixels[cornerRowOffset[row] + column - cornerRowFirst[row]];
    *corner = bottom ? ( right ? 3 : 2 ) : ( right ? 1 : 0 );
    // The lower end lies on the side edge for the top corners and on the bottom edge for the
    // bottom ones (Corners::geometry): mirrored top to bottom, a gauge runs the other way.
    *along = bottom ? 1000 * CORNER_ALONG_SCALE - pixel->along : pixel->along;
    return pixel;
}

#ifdef __cplusplus
}
#endif
