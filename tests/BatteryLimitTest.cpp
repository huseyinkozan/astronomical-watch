// Host test of main/BatteryLimit.cpp. Build and run: sh tests/run_host_tests.sh

#include <cstdio>

#include "BatteryLimit.h"

static int failures = 0;
static int checks = 0;

static void check( const char *name, bool value, bool expected )
{
    ++checks;
    if ( value != expected ) {
        printf( "FAIL %s: %d, expected %d\n", name, value ? 1 : 0, expected ? 1 : 0 );
        ++failures;
    }
}

int main()
{
    // The 10 % limit needs the gauge and VBAT together: the gauge reads too low for a while after
    // a power cycle (docs/hardware.md, run D).
    check( "half full", BatteryLimit::empty( 3800, 50 ), false );
    check( "at the limit", BatteryLimit::empty( 3650, 10 ), true );
    check( "gauge low, VBAT high", BatteryLimit::empty( 3700, 10 ), false );
    check( "VBAT low, gauge above the limit", BatteryLimit::empty( 3600, 11 ), false );
    // A gauge stuck high (seen on a flat cell by another project, and after power cycles here)
    // must not let the cell run down: VBAT alone ends it.
    check( "gauge stuck high, VBAT at the backstop", BatteryLimit::empty( 3450, 60 ), true );
    check( "gauge stuck high, VBAT below the backstop", BatteryLimit::empty( 3300, 80 ), true );
    check( "gauge high, VBAT just above the backstop", BatteryLimit::empty( 3460, 60 ), false );

    // With an alarm pending the watch sleeps on below the limit down to the floor.
    check( "floor: 2 % and 3.50 V", BatteryLimit::atFloor( 3500, 2 ), true );
    check( "floor: 2 %, VBAT higher", BatteryLimit::atFloor( 3510, 2 ), false );
    check( "floor: VBAT 3.40 V alone", BatteryLimit::atFloor( 3400, 50 ), true );
    check( "floor: VBAT above 3.40 V, gauge high", BatteryLimit::atFloor( 3410, 50 ), false );
    // The backstop lies above the floor, so a pending alarm still has room to ring.
    check( "backstop above the floor", BatteryLimit::BackstopMillivolts > BatteryLimit::FloorMillivolts, true );

    printf( "Battery limit: %d checks, %d failures\n", checks, failures );
    return failures == 0 ? 0 : 1;
}
