#include "SevenSegment.h"

// Segments a to g in bits 0 to 6.
static const unsigned char s_digits[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
static const float s_widthShare = 0.62f;
static const float s_pairGapShare = 0.16f;
static const float s_colonGapShare = 0.18f;

// A segment in its box, pointed at both ends of its long side.
static SevenSegment::Hexagon hexagon( float x0, float y0, float x1, float y1 )
{
    SevenSegment::Hexagon h = {};
    if ( x1 - x0 >= y1 - y0 ) {
        const float half = ( y1 - y0 ) / 2;
        const float middle = ( y0 + y1 ) / 2;
        h = { { { x0, middle }, { x0 + half, y0 }, { x1 - half, y0 }, { x1, middle }, { x1 - half, y1 }, { x0 + half, y1 } } };
    } else {
        const float half = ( x1 - x0 ) / 2;
        const float middle = ( x0 + x1 ) / 2;
        h = { { { middle, y0 }, { x1, y0 + half }, { x1, y1 - half }, { middle, y1 }, { x0, y1 - half }, { x0, y0 + half } } };
    }
    return h;
}

float SevenSegment::digitWidth( float height )
{
    return height * s_widthShare;
}

int SevenSegment::segments( int digit, float height, Hexagon out[7] )
{
    if ( digit < 0 || digit > 9 ) {
        return 0;
    }
    const float w = digitWidth( height );
    const float h = height;
    const float t = height / 11;
    const float g = height / 40;
    const float m = h / 2;
    const float boxes[7][4] = { { t / 2 + g, 0, w - t / 2 - g, t },         { w - t, t / 2 + g, w, m - g },
                                { w - t, m + g, w, h - t / 2 - g },         { t / 2 + g, h - t, w - t / 2 - g, h },
                                { 0, m + g, t, h - t / 2 - g },             { 0, t / 2 + g, t, m - g },
                                { t / 2 + g, m - t / 2, w - t / 2 - g, m + t / 2 } };
    int count = 0;
    for ( int i = 0; i < 7; ++i ) {
        if ( s_digits[digit] & ( 1 << i ) ) {
            out[count++] = hexagon( boxes[i][0], boxes[i][1], boxes[i][2], boxes[i][3] );
        }
    }
    return count;
}

float SevenSegment::colonSize( float height )
{
    return height / 11;
}

float SevenSegment::timeWidth( float height )
{
    const float w = digitWidth( height );
    return 4 * w + 2 * s_pairGapShare * w + 2 * s_colonGapShare * w + colonSize( height );
}

void SevenSegment::layout( float height, float x[4], float &colonX )
{
    const float w = digitWidth( height );
    const float pairGap = s_pairGapShare * w;
    const float colonGap = s_colonGapShare * w;
    const float colon = colonSize( height );
    x[0] = 0;
    x[1] = w + pairGap;
    x[2] = 2 * w + pairGap + 2 * colonGap + colon;
    x[3] = x[2] + w + pairGap;
    colonX = 2 * w + pairGap + colonGap + colon / 2;
}
