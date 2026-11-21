#include "NightRed.h"

static const int s_blackBelow = 0x20;
static const int s_dimmestRed = 0x30;

uint16_t NightRed::map( uint16_t colour )
{
    const int r5 = colour >> 11;
    const int g6 = ( colour >> 5 ) & 0x3F;
    const int b5 = colour & 0x1F;
    const int r = ( r5 << 3 ) | ( r5 >> 2 );
    const int g = ( g6 << 2 ) | ( g6 >> 4 );
    const int b = ( b5 << 3 ) | ( b5 >> 2 );
    if ( r < s_blackBelow && g < s_blackBelow && b < s_blackBelow ) {
        return 0;
    }
    // Rec. 601 brightness, the weights summing to 256.
    int brightness = ( 77 * r + 150 * g + 29 * b ) >> 8;
    if ( brightness < s_dimmestRed ) {
        brightness = s_dimmestRed;
    }
    return static_cast<uint16_t>( ( brightness >> 3 ) << 11 );
}

void NightRed::convert( uint16_t *pixels, int count )
{
    for ( int i = 0; i < count; ++i ) {
        if ( pixels[i] != 0 ) {
            pixels[i] = map( pixels[i] );
        }
    }
}
