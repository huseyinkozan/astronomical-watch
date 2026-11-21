// Host test of main/Tally.cpp and main/Morse.cpp (the Tools page's counter and the torch's SOS).
// Build and run: sh tests/run_host_tests.sh

#include <cstdio>

#include "Morse.h"
#include "Tally.h"

static int failures = 0;

static void check( const char *name, long long value, long long expected )
{
    if ( value != expected ) {
        printf( "FAIL %s: %lld, expected %lld\n", name, value, expected );
        ++failures;
    }
}

int main()
{
    const int64_t noon = 1782043200;  // 2026-06-21 12:00 UTC
    Tally tally;
    check( "empty: no rate", tally.perHour( noon ), -1 );
    check( "empty: no time", tally.elapsedMinutes( noon ), -1 );
    tally.add( noon + 30 );
    check( "first count", tally.count, 1 );
    check( "first count: its minute", tally.startMinute, noon / 60 );
    check( "first minute: no rate", tally.perHour( noon + 59 ), -1 );
    for ( int i = 1; i < 47; ++i ) {
        tally.add( noon + 60 + i );
    }
    // 47 counts in 1 h 31 min (the mock-up): 31 an hour.
    check( "47 counts", tally.count, 47 );
    check( "rate", tally.perHour( noon + 91 * 60 ), 31 );
    check( "elapsed", tally.elapsedMinutes( noon + 91 * 60 + 59 ), 91 );
    check( "clock set back: no time", tally.elapsedMinutes( noon - 120 ), -1 );
    check( "no target: one step", tally.stepCount(), 1 );
    check( "no target: goal", tally.goal(), 0 );
    check( "no target: not reached", tally.reached(), 0 );
    check( "no target: shown", tally.inStep(), 47 );

    tally.undo();
    check( "undo", tally.count, 46 );
    tally.reset();
    check( "reset", tally.count, 0 );
    check( "reset: no start", tally.startMinute, 0 );
    tally.undo();
    check( "undo at zero", tally.count, 0 );
    tally.add( noon );
    tally.undo();
    check( "undo to zero: no start", tally.startMinute, 0 );

    tally.target = Tally::Target33;
    for ( int i = 0; i < 33; ++i ) {
        tally.add( noon );
    }
    check( "33: reached", tally.reached(), 1 );
    check( "33: shown", tally.inStep(), 33 );
    tally.add( noon );
    check( "33: beyond the goal", tally.inStep(), 34 );

    tally.reset();
    tally.target = Tally::Steps33;
    check( "steps: three", tally.stepCount(), 3 );
    check( "steps: empty in step 1", tally.step(), 0 );
    for ( int i = 0; i < 33; ++i ) {
        tally.add( noon );
    }
    check( "steps: 33 still step 1", tally.step(), 0 );
    check( "steps: 33 of 33", tally.inStep(), 33 );
    check( "steps: step 1 reached", tally.reached(), 1 );
    tally.add( noon );
    check( "steps: 34 is step 2", tally.step(), 1 );
    check( "steps: 1 of 33", tally.inStep(), 1 );
    check( "steps: step 2 not reached", tally.reached(), 0 );
    for ( int i = 0; i < 65; ++i ) {
        tally.add( noon );
    }
    check( "steps: 99 is step 3", tally.step(), 2 );
    check( "steps: 33 of 33 in step 3", tally.inStep(), 33 );
    tally.add( noon );
    check( "steps: 100 stays in step 3", tally.step(), 2 );
    check( "steps: 34 in step 3", tally.inStep(), 34 );
    tally.undo();
    tally.undo();
    check( "steps: undo back over a step", tally.inStep(), 32 );

    tally.target = Tally::Target100;
    tally.nextTarget();
    check( "target after 100: the steps", tally.target, Tally::Steps33 );
    tally.nextTarget();
    check( "target after the steps: none", tally.target, Tally::NoTarget );

    tally.count = Tally::MaxCount;
    tally.add( noon );
    check( "no count beyond the maximum", tally.count, Tally::MaxCount );

    // SOS: S (three 200 ms dots), O (three 600 ms dashes), S, then 1.4 s off; 6.8 s a cycle.
    int remaining = 0;
    check( "SOS starts lit", Morse::sosLit( 0, &remaining ), 1 );
    check( "SOS first dot", remaining, 200 );
    check( "SOS gap after the dot", Morse::sosLit( 250, &remaining ), 0 );
    check( "SOS gap left", remaining, 150 );
    check( "SOS letter gap", Morse::sosLit( 1000 + 100 ), 0 );
    check( "SOS first dash", Morse::sosLit( 1600 + 500, &remaining ), 1 );
    check( "SOS first dash left", remaining, 100 );
    check( "SOS last dot", Morse::sosLit( 5200 + 100 ), 1 );
    check( "SOS word gap", Morse::sosLit( 5400 + 1300, &remaining ), 0 );
    check( "SOS word gap left", remaining, 100 );
    check( "SOS repeats", Morse::sosLit( 6800 + 50 ), 1 );
    check( "SOS cycle", Morse::CycleUnits * Morse::UnitMs, 6800 );

    if ( failures == 0 ) {
        printf( "Tally and Morse: all tests passed\n" );
    }
    return failures == 0 ? 0 : 1;
}
