// Host test of main/PartOrder.cpp. Build and run: sh tests/run_host_tests.sh

#include <cstdio>

#include "PartOrder.h"

static int failures = 0;

static void check( const char *name, int value, int expected )
{
    if ( value != expected ) {
        printf( "FAIL %s: %d, expected %d\n", name, value, expected );
        ++failures;
    }
}

static int encode( const int *parts, int count )
{
    int value = 0;
    for ( int i = 0; i < count; ++i ) {
        value |= parts[i] << ( 3 * i );
    }
    return value;
}

int main()
{
    int parts[PartOrder::MaxParts] = {};
    check( "default: natural order", PartOrder::decode( 0, 0, 4, -1, parts ), 4 );
    check( "default: first", parts[0], 0 );
    check( "default: last", parts[3], 3 );
    const int reversed[4] = { 3, 2, 1, 0 };
    check( "reversed count", PartOrder::decode( encode( reversed, 4 ), 0, 4, -1, parts ), 4 );
    check( "reversed first", parts[0], 3 );
    check( "hidden part 2", PartOrder::decode( encode( reversed, 4 ), 1 << 2, 4, -1, parts ), 3 );
    check( "hidden: order kept", parts[1], 1 );
    const int duplicate[4] = { 0, 0, 1, 2 };
    check( "not a permutation: natural", PartOrder::decode( encode( duplicate, 4 ), 0, 4, -1, parts ), 4 );
    check( "not a permutation: first", parts[1], 1 );
    check( "all hidden: one left", PartOrder::decode( 0, 15, 4, -1, parts ), 1 );
    check( "always shown stays", PartOrder::decode( 0, 15, 4, 3, parts ), 1 );
    check( "always shown is it", parts[0], 3 );
    const int eight[8] = { 7, 6, 5, 4, 3, 2, 1, 0 };
    check( "eight parts", PartOrder::decode( encode( eight, 8 ), 0, 8, -1, parts ), 8 );
    check( "eight parts first", parts[0], 7 );
    const int shown[3] = { 2, 0, 3 };
    check( "step forward", PartOrder::step( shown, 3, 0, 1 ), 3 );
    check( "step wraps", PartOrder::step( shown, 3, 3, 1 ), 2 );
    check( "step back wraps", PartOrder::step( shown, 3, 2, -1 ), 3 );
    check( "step from a hidden part", PartOrder::step( shown, 3, 1, 1 ), 2 );
    // A page built again comes back on its part only while that part is still shown.
    check( "keep a shown part", PartOrder::keep( shown, 3, 3 ), 3 );
    check( "a hidden part gives the first shown", PartOrder::keep( shown, 3, 1 ), 2 );
    // An order saved before a part was added (six fields, the seventh zero) keeps its places,
    // the new part at the end.
    const int six[6] = { 5, 3, 1, 0, 2, 4 };
    check( "older order count", PartOrder::decode( encode( six, 6 ), 0, 7, -1, parts ), 7 );
    check( "older order kept", parts[0] == 5 && parts[1] == 3 && parts[5] == 4, 1 );
    check( "older order new part last", parts[6], 6 );
    // The tide's regrouping (docs/superpowers/plans/2026-11-23-tide.md, Task 2): the defaults of
    // Settings.cpp, written out here as they are there.
    // Info: Sun 0, Twilight 1, Prayer 2, Moon 3, Compass 4, Time 5, Tide 6; the tide after the Moon.
    const int infoDefault = 0 | 1 << 3 | 2 << 6 | 3 << 9 | 6 << 12 | 4 << 15 | 5 << 18;
    check( "info default: seven", PartOrder::decode( infoDefault, 1 << 6, 7, -1, parts ), 6 );
    const int infoShown[6] = { 0, 1, 2, 3, 4, 5 };
    for ( int i = 0; i < 6; ++i ) {
        check( "info default: the tide hidden, the rest in order", parts[i], infoShown[i] );
    }
    check( "info default: the tide after the Moon", PartOrder::decode( infoDefault, 0, 7, -1, parts ), 7 );
    check( "info default: Moon then Tide", parts[3] * 10 + parts[4], 36 );
    // Settings (portal build): Rows 0, More 1, SetupInfo 2, Setup 3, Code 4, Reset 5, Battery 6,
    // About 7; the reset stays last.
    const int settingsDefault = 0 | 1 << 3 | 2 << 6 | 3 << 9 | 4 << 12 | 6 << 15 | 7 << 18 | 5 << 21;
    check( "settings default: eight", PartOrder::decode( settingsDefault, 0, 8, 3, parts ), 8 );
    check( "settings default: battery, about, reset", parts[5] * 100 + parts[6] * 10 + parts[7], 675 );
    // Without the portal: Rows 0, More 1, SetupInfo 2, Reset 3, Battery 4, About 5.
    const int settingsNoPortal = 0 | 1 << 3 | 2 << 6 | 4 << 9 | 5 << 12 | 3 << 15;
    check( "settings without the portal: six", PartOrder::decode( settingsNoPortal, 0, 6, -1, parts ), 6 );
    check( "settings without the portal: reset last", parts[5], 3 );
    // A saved order of eight parts on a page that now has seven decodes to seven parts, no more.
    const int oldEight[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    check( "old info order: seven", PartOrder::decode( encode( oldEight, 8 ), 0, 7, -1, parts ), 7 );

    printf( "Part order: 23 checks, %d failures\n", failures );
    return failures == 0 ? 0 : 1;
}
