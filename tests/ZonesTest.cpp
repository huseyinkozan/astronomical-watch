// Host test of main/Zones.cpp: offsets around two daylight saving changes (London 2027).
// Build and run: sh tests/run_host_tests.sh

#include <cstdio>

#include "Zones.h"

static int failures = 0;

static void check( const char *name, int value, int expected )
{
    if ( value != expected ) {
        printf( "FAIL %s: %d, expected %d\n", name, value, expected );
        ++failures;
    }
}

int main()
{
    // Europe/London: BST from 2027-03-28 01:00 UTC, GMT again from 2027-10-31 01:00 UTC.
    const int spring = 1806195600 / 60;
    const int autumn = 1824944400 / 60;
    Zone london = { 0, { spring, autumn }, { 60, 0 } };
    const int64_t s = 60;
    check( "before spring", Zones::offsetAt( london, spring * s - 1 ), 0 );
    check( "at spring", Zones::offsetAt( london, spring * s ), 60 );
    check( "summer", Zones::offsetAt( london, ( spring + 100000 ) * s ), 60 );
    check( "after autumn", Zones::offsetAt( london, autumn * s + 5 ), 0 );
    check( "no advance before", Zones::advance( london, spring * s - 60 ), 0 );
    check( "advance at spring", Zones::advance( london, spring * s ), 1 );
    check( "offset after advance", london.offset, 60 );
    check( "next change moved up", london.changeAt[0], autumn );
    check( "second change cleared", london.changeAt[1], 0 );
    check( "advance past both", Zones::advance( london, autumn * s ), 1 );
    check( "back to GMT", london.offset, 0 );
    check( "no more changes", london.changeAt[0], 0 );
    Zone fixed = { 180, { 0, 0 }, { 0, 0 } };
    check( "fixed zone", Zones::offsetAt( fixed, 1900000000 ), 180 );
    check( "fixed zone never advances", Zones::advance( fixed, 1900000000 ), 0 );
    printf( "Zones: 14 checks, %d failures\n", failures );
    return failures == 0 ? 0 : 1;
}
