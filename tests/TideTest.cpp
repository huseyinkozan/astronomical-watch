// Host test of main/Tide.cpp (docs/superpowers/specs/2026-11-23-tide-design.md, "The model").
// Build and run: sh tests/run_host_tests.sh

#include <cstdio>
#include <cmath>
#include <cstdlib>

#include "Dates.h"
#include "Sky.h"
#include "Tide.h"

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

static void near( const char *name, long long value, long long expected, long long tolerance )
{
    ++checks;
    if ( llabs( value - expected ) > tolerance ) {
        printf( "FAIL %s: %lld, expected %lld (+-%lld)\n", name, value, expected, tolerance );
        ++failures;
    }
}

int main()
{
    // The Moon's upper transits a lunar day (24 h 50 min) apart; an interval of 5 h 20 min puts
    // high water 19200 s after each transit and half a lunar day (44700 s) later.
    const int64_t T0 = 1782000000;
    const int64_t T1 = T0 + 89400;
    const int64_t T2 = T1 + 89400;
    const int64_t T3 = T2 + 89400;
    const int64_t transits[Tide::TransitCount] = { T0, T1, T2, T3 };
    const int I = 19200;

    Tide::State s = Tide::at( transits, 320, 7.4, T1 + I );
    check( "atHighWater: valid", s.valid, 1 );
    check( "atHighWater: level", s.level, 1000 );
    s = Tide::at( transits, 320, 7.4, T1 + I + 22350 );
    near( "atLowWater: level", s.level, 0, 2 );
    check( "risingBeforeHigh", Tide::at( transits, 320, 7.4, T1 + I - 3600 ).rising, 1 );
    check( "fallingAfterHigh", Tide::at( transits, 320, 7.4, T1 + I + 3600 ).rising, 0 );

    // At a transit the next high water is the interval away; the low before it has passed.
    s = Tide::at( transits, 320, 7.4, T1 );
    check( "nextTimes: high", s.nextHigh, T1 + I );
    check( "nextTimes: low", s.nextLow, T1 + I + 22350 );

    check( "off", Tide::at( transits, Tide::Off, 7.4, T1 ).valid, 0 );

    // A day without an upper transit (about once a month): the neighbours bridge it.
    const int64_t gap[Tide::TransitCount] = { T0, 0, T2, T3 };
    s = Tide::at( gap, 320, 7.4, T0 + I + 89400 );
    check( "noTransitDay: valid", s.valid, 1 );
    near( "noTransitDay: level", s.level, 1000, 5 );

    // The longest interval just after today's transit: high water from yesterday's.
    s = Tide::at( transits, 745, 7.4, T1 + 60 );
    check( "longIntervalEarly: valid", s.valid, 1 );
    check( "longIntervalEarly: just after high", s.level >= 990, 1 );

    // Spring at new and full Moon, neap at the quarters.
    check( "spring: new Moon", Tide::at( transits, 320, 0, T1 ).spring, 1000 );
    check( "spring: first quarter", Tide::at( transits, 320, 7.38, T1 ).spring < 50, 1 );
    check( "spring: full Moon", Tide::at( transits, 320, 14.77, T1 ).spring > 950, 1 );

    // A day from T1: high, low, high, low.
    Tide::Water waters[8];
    const int n = Tide::waters( transits, 320, T1, T1 + 86400, waters, 8 );
    check( "waters: four", n, 4 );
    check( "waters: first high", waters[0].high, 1 );
    check( "waters: first time", waters[0].time, T1 + I );
    check( "waters: second low", waters[1].high, 0 );
    check( "waters: second time", waters[1].time, T1 + I + 22350 );
    check( "waters: third high", waters[2].time, T1 + I + 44700 );

    // The wave icon (B3, the owner's pick): a calm line at 20 % of the height at low water, the
    // crest at the top at high water, the crest at 68 % of the width.
    near( "wave: calm at low water, left", std::lround( Tide::waveHeight( 0, 0.1f ) * 1000 ), 200, 0 );
    near( "wave: calm at low water, crest", std::lround( Tide::waveHeight( 0, 0.68f ) * 1000 ), 200, 0 );
    near( "wave: high water's crest at the top", std::lround( Tide::waveHeight( 1000, 0.68f ) * 1000 ), 1000, 1 );
    near( "wave: half way", std::lround( Tide::waveHeight( 500, 0.68f ) * 1000 ), 600, 1 );
    near( "wave: the left edge stays calm", std::lround( Tide::waveHeight( 1000, 0.0f ) * 1000 ), 200, 1 );
    check( "wave: the front falls steeply", Tide::waveHeight( 1000, 0.80f ) < Tide::waveHeight( 1000, 0.56f ), 1 );

    // A year of real transits (Istanbul, 2026): every 10 minutes of every local day has a tide
    // for any interval, also before the first known transit's high water (the final review:
    // mornings were blank on about half the days) and across days without a transit.
    const Astronomy::Observer istanbul = { 41.0082, 28.9784 };
    const int intervals[4] = { 0, 300, 600, 745 };
    int blank = 0;
    int fewWaters = 0;
    for ( int day = Dates::dayNumber( 2026, 1, 1 ); day < Dates::dayNumber( 2027, 1, 1 ); ++day ) {
        const Sky::Day sky = Sky::computeDay( day, 180, istanbul, Hijri::Diyanet, 17 );
        for ( int k = 0; k < 4; ++k ) {
            for ( int minute = 0; minute < 1440; minute += 10 ) {
                if ( !Tide::at( sky.moonTransits, intervals[k], sky.phase.ageDays, sky.localMidnight + minute * 60 ).valid ) {
                    ++blank;
                }
            }
            Tide::Water list[8];
            if ( Tide::waters( sky.moonTransits, intervals[k], sky.localMidnight, sky.localMidnight + 86400, list, 8 ) < 3 ) {
                ++fewWaters;
            }
        }
    }
    check( "a year: no blank moment", blank, 0 );
    check( "a year: three or four waters every day", fewWaters, 0 );

    printf( "Tide: %d checks, %d failures\n", checks, failures );
    return failures == 0 ? 0 : 1;
}
