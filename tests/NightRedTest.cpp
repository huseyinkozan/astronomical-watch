// Host test of main/NightRed.cpp. Build and run: sh tests/run_host_tests.sh

#include <cstdint>
#include <cstdio>

#include "NightRed.h"

static int failures = 0;
static int checks = 0;

static void check( const char *name, uint16_t value, uint16_t expected )
{
    ++checks;
    if ( value != expected ) {
        printf( "FAIL %s: 0x%04X, expected 0x%04X\n", name, value, expected );
        ++failures;
    }
}

int main()
{
    // RGB565 as LVGL draws it: red in the top 5 bits.
    check( "black stays black", NightRed::map( 0x0000 ), 0x0000 );
    check( "white: full red", NightRed::map( 0xFFFF ), 0xF800 );
    check( "red stays red", NightRed::map( 0xF800 ), 0x4800 );
    check( "green: its brightness in red", NightRed::map( 0x07E0 ), 0x9000 );
    // Blue is dark by its brightness, but the night gauge must not vanish: lifted to the floor.
    check( "blue: lifted to the dimmest red", NightRed::map( 0x001F ), 0x3000 );
    check( "mid grey", NightRed::map( 0x8410 ), 0x8000 );
    // Near black (every channel below 0x20) stays black, so that the ground never glows red.
    check( "near black stays black", NightRed::map( 0x18E3 ), 0x0000 );
    check( "dark grey lifted", NightRed::map( 0x2104 ), 0x3000 );
    // Only the red channel is ever lit.
    bool onlyRed = true;
    for ( uint32_t c = 0; c <= 0xFFFF; ++c ) {
        if ( ( NightRed::map( static_cast<uint16_t>( c ) ) & 0x07FF ) != 0 ) {
            onlyRed = false;
        }
    }
    check( "only red is lit", onlyRed ? 1 : 0, 1 );
    uint16_t band[3] = { 0xFFFF, 0x0000, 0x07E0 };
    NightRed::convert( band, 3 );
    check( "band 0", band[0], 0xF800 );
    check( "band 1", band[1], 0x0000 );
    check( "band 2", band[2], 0x9000 );

    printf( "Night red: %d checks, %d failures\n", checks, failures );
    return failures == 0 ? 0 : 1;
}
