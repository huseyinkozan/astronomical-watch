// Host test of main/Hayiz.cpp (Diyanet's Hanafi rules for the cycle page's hayız part).
// Build and run: sh tests/run_host_tests.sh

#include <cstdio>

#include "CycleLog.h"
#include "Hayiz.h"

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

static const int Today = 20788;
static CycleLog s_log;

static void reset()
{
    s_log.clear();
    s_log.advance( Today );
}

static void bleed( int first, int days )
{
    for ( int i = 0; i < days; ++i ) {
        s_log.set( first + i, 2 );
    }
}

int main()
{
    // Fewer than three days: istihaze.
    reset();
    const int d = Today - 60;
    bleed( d, 2 );
    check( "two days: istihaze", Hayiz::kindOn( s_log, d ), Hayiz::Istihaze );
    check( "two days: then purity", Hayiz::kindOn( s_log, d + 3 ), Hayiz::Purity );

    // Five days: hayız, then purity counted from its end.
    reset();
    bleed( d, 5 );
    check( "five days: first", Hayiz::kindOn( s_log, d ), Hayiz::Hayz );
    check( "five days: fifth", Hayiz::kindOn( s_log, d + 4 ), Hayiz::Hayz );
    check( "five days: sixth is purity", Hayiz::kindOn( s_log, d + 5 ), Hayiz::Purity );
    Hayiz::Today t = Hayiz::today( s_log, Today );
    check( "purity day", t.day, Today - ( d + 4 ) );
    check( "habit", t.habit, 5 );

    // Clean days inside the ten days count as hayız.
    reset();
    bleed( d, 3 );
    bleed( d + 5, 2 );
    check( "inner clean day", Hayiz::kindOn( s_log, d + 3 ), Hayiz::Hayz );
    check( "last bleeding day", Hayiz::kindOn( s_log, d + 6 ), Hayiz::Hayz );
    check( "after it", Hayiz::kindOn( s_log, d + 7 ), Hayiz::Purity );

    // Past ten days: the habit is hayız, the rest istihaze.
    reset();
    bleed( d - 40, 6 );
    bleed( d, 12 );
    check( "with a habit of 6: day 6", Hayiz::kindOn( s_log, d + 5 ), Hayiz::Hayz );
    check( "with a habit of 6: day 7", Hayiz::kindOn( s_log, d + 6 ), Hayiz::Istihaze );
    check( "with a habit of 6: day 12", Hayiz::kindOn( s_log, d + 11 ), Hayiz::Istihaze );
    reset();
    bleed( d, 12 );
    check( "no habit: day 10", Hayiz::kindOn( s_log, d + 9 ), Hayiz::Hayz );
    check( "no habit: day 11", Hayiz::kindOn( s_log, d + 10 ), Hayiz::Istihaze );

    // Bleeding before fifteen days of purity: istihaze.
    reset();
    bleed( d, 5 );
    bleed( d + 15, 4 );
    check( "ten clean days: istihaze", Hayiz::kindOn( s_log, d + 15 ), Hayiz::Istihaze );
    reset();
    bleed( d, 5 );
    bleed( d + 20, 4 );
    check( "fifteen clean days: hayız", Hayiz::kindOn( s_log, d + 20 ), Hayiz::Hayz );

    // Today in an ongoing period.
    reset();
    bleed( Today - 2, 3 );
    t = Hayiz::today( s_log, Today );
    check( "ongoing: hayız", t.kind, Hayiz::Hayz );
    check( "ongoing: day 3", t.day, 3 );
    reset();
    bleed( Today - 1, 2 );
    t = Hayiz::today( s_log, Today );
    check( "ongoing, day 2: hayız so far", t.kind, Hayiz::Hayz );

    // A Ramadan with five hayız days.
    reset();
    bleed( d + 3, 5 );
    check( "Ramadan: hayız days", Hayiz::hayzDays( s_log, d, 30 ), 5 );
    s_log.earlierBalance = 3;
    s_log.ramadanAdded = 5;
    s_log.madeUp = 2;
    check( "balance", Hayiz::balance( s_log ), 6 );
    s_log.madeUp = 20;
    check( "balance never below 0", Hayiz::balance( s_log ), 0 );

    printf( "Hayiz: %d checks, %d failures\n", checks, failures );
    return failures == 0 ? 0 : 1;
}
