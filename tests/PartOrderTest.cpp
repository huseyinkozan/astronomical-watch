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
    printf( "Part order: 18 checks, %d failures\n", failures );
    return failures == 0 ? 0 : 1;
}
