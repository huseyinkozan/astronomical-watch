// Host test of main/Regatta.cpp: the 5-4-1-0 signals and the sync rounding.
// Build and run: sh tests/run_host_tests.sh

#include <cstdio>

#include "Regatta.h"

static int failures = 0;

static void check( const char *what, long long value, long long expected )
{
    if ( value != expected ) {
        printf( "FAIL %s: %lld, expected %lld\n", what, value, expected );
        ++failures;
    }
}

int main()
{
    // A whole sequence at the 20 ms steps of the main loop: three signals, in order.
    int signals = 0;
    int beeps[3] = {};
    int64_t at[3] = {};
    for ( int64_t left = Regatta::DurationS * 1000LL; left > -2000; left -= 20 ) {
        const Regatta::Signal signal = Regatta::signal( left, left - 20 );
        if ( signal.beeps > 0 && signals < 3 ) {
            beeps[signals] = signal.beeps;
            at[signals] = left - 20;
            ++signals;
        }
    }
    check( "signals in a sequence", signals, 3 );
    check( "4 min signal at", at[0], 240000 );
    check( "4 min signal beeps", beeps[0], 4 );
    check( "1 min signal at", at[1], 60000 );
    check( "1 min signal beeps", beeps[1], 1 );
    check( "start at", at[2], 0 );
    check( "start tone (ms)", Regatta::signal( 10, 0 ).toneMs, 1500 );
    // A step that jumps a mark (a busy main task) still gives its signal; a time left that grows
    // (sync rounding up) gives none.
    check( "jump over 1 min", Regatta::signal( 61000, 59000 ).beeps, 1 );
    check( "growing time left", Regatta::signal( 239000, 241000 ).beeps, 0 );
    check( "after the start", Regatta::signal( 0, -500 ).beeps, 0 );
    // Sync: to the nearest minute, a half rounding up.
    check( "sync 4:20", Regatta::sync( 260000 ), 240000 );
    check( "sync 4:40", Regatta::sync( 280000 ), 300000 );
    check( "sync 0:30", Regatta::sync( 30000 ), 60000 );
    check( "sync 0:29", Regatta::sync( 29000 ), 0 );
    check( "sync after the start", Regatta::sync( 0 ), 0 );
    // Sync down onto a mark is that mark's signal.
    check( "sync 4:20 to 4:00", Regatta::signal( 260000, Regatta::sync( 260000 ) ).beeps, 4 );
    printf( "Regatta: %d failures\n", failures );
    return failures == 0 ? 0 : 1;
}
