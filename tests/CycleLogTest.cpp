// Host test of main/CycleLog.cpp (the cycle page's one-year log).
// Build and run: sh tests/run_host_tests.sh

#include <cstdio>

#include "CycleLog.h"

static int failures = 0;
static int checks = 0;

static void check( const char *name, long long value, long long expected )
{
    ++checks;
    if ( value != expected ) {
        printf( "FAIL %s: %lld, expected %lld\n", name, value, expected );
        ++failures;
    }
}

int main()
{
    static CycleLog log;
    log.clear();
    const int day = 20777;  // 2026-11-20
    check( "empty: nothing", log.get( day ), 0 );
    log.advance( day );
    log.set( day, 2 );
    check( "set today", log.get( day ), 2 );
    check( "flow", log.flow( day ), 2 );
    log.set( day + 1, 3 );
    check( "a future day is ignored", log.get( day + 1 ), 0 );
    log.set( day - 365, 1 );
    check( "the oldest day kept", log.get( day - 365 ), 1 );
    log.set( day - 366, 1 );
    check( "older than a year is ignored", log.get( day - 366 ), 0 );
    check( "older than a year does not alias today", log.get( day ), 2 );

    // Days that pass without an entry must not show what the ring held a year earlier.
    log.advance( day + 1 );
    check( "the next day is empty", log.get( day + 1 ), 0 );
    check( "yesterday kept", log.get( day ), 2 );
    check( "the day a year back dropped out", log.get( day - 365 ), 0 );
    log.advance( day + 365 );
    check( "a year on: the first day still there", log.get( day ), 2 );
    log.advance( day + 366 );
    check( "a year and a day on: gone", log.get( day ), 0 );
    log.set( day + 100, 3 );
    log.advance( day + 800 );
    check( "a long gap clears everything", log.get( day + 100 ), 0 );
    log.advance( day + 700 );
    check( "the clock moved back: nothing moves", log.newestDay, day + 800 );

    // Made-up fasts: bit 3 counted once per day, set or cleared.
    log.clear();
    log.advance( day );
    log.set( day, CycleLog::MadeUpFast );
    check( "a made-up fast counted", log.madeUp, 1 );
    log.set( day, CycleLog::MadeUpFast | 1 );
    check( "set again: counted once", log.madeUp, 1 );
    log.set( day, 1 );
    check( "cleared: uncounted", log.madeUp, 0 );
    log.set( day - 2, CycleLog::MadeUpFast );
    log.advance( day + 400 );
    check( "a kept fast stays counted when its day leaves the log", log.madeUp, 1 );

    printf( "Cycle log: %d checks, %d failures\n", checks, failures );
    return failures == 0 ? 0 : 1;
}
