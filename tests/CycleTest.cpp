// Host test of main/Cycle.cpp (the cycle page's rules; docs/superpowers/specs/
// 2026-11-22-cycle-page-design.md, "Rules").
// Build and run: sh tests/run_host_tests.sh

#include <cstdio>

#include "Cycle.h"
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

static const int Today = 20788;  // 2026-12-01
static CycleLog s_log;

static void reset()
{
    s_log.clear();
    s_log.advance( Today );
}

static void bleed( int first, int days, int flow = 2 )
{
    for ( int i = 0; i < days; ++i ) {
        s_log.set( first + i, static_cast<uint8_t>( flow ) );
    }
}

// Cycles of `lengths` (oldest first), then the current one started `currentAgo` days ago; each
// period 5 days.
static void cycles( const int *lengths, int count, int currentAgo )
{
    int start = Today - currentAgo;
    for ( int i = count - 1; i >= 0; --i ) {
        start -= lengths[i];
    }
    for ( int i = 0; i < count; ++i ) {
        bleed( start, 5 );
        start += lengths[i];
    }
    bleed( start, 5 > currentAgo + 1 ? currentAgo + 1 : 5 );
}

int main()
{
    reset();
    Cycle::State s = Cycle::analyse( s_log, Today );
    check( "empty: no period", s.count, 0 );
    check( "empty: no cycle day", s.cycleDay, 0 );
    check( "empty: no prediction", s.nextEarly, 0 );
    check( "empty: no fertile window", s.fertileFirst, 0 );

    reset();
    bleed( Today - 11, 5 );
    s = Cycle::analyse( s_log, Today );
    check( "one period: cycle day", s.cycleDay, 12 );
    check( "one period: no complete cycle", s.complete, 0 );
    check( "one period: irregular", s.irregular, 1 );
    check( "one period: no prediction", s.nextEarly, 0 );
    check( "one period: window 6", s.fertileFirst, 6 );
    check( "one period: window 21", s.fertileLast, 21 );
    check( "one period: no ovulation", s.ovulationDay, 0 );

    // One clean day inside a period keeps it; two end it, and later bleeding is between periods.
    reset();
    bleed( Today - 20, 3 );
    bleed( Today - 16, 2 );
    s = Cycle::analyse( s_log, Today );
    check( "one clean day: one period", s.count, 1 );
    check( "one clean day: length", s.periods[0].length, 6 );
    check( "one clean day: bleeding days", s.periods[0].bleedingDays, 5 );
    reset();
    bleed( Today - 20, 3 );
    bleed( Today - 15, 1 );
    s = Cycle::analyse( s_log, Today );
    check( "two clean days: no new cycle", s.count, 1 );
    check( "two clean days: length", s.periods[0].length, 3 );
    check( "two clean days: between periods", ( s.periods[0].warnings & Cycle::BetweenPeriods ) != 0, 1 );
    reset();
    bleed( Today - 20, 3 );
    bleed( Today - 7, 2 );
    s = Cycle::analyse( s_log, Today );
    check( "ten clean days: a new cycle", s.count, 2 );
    check( "ten clean days: its length", s.periods[0].cycleLength, 13 );

    // Spotting (a single light day) neither starts a cycle nor warns; day 1 is the first day
    // of a heavier flow or of two bleeding days.
    reset();
    bleed( Today - 40, 5 );
    bleed( Today - 15, 1, 1 );   // spotting three days before the period
    bleed( Today - 12, 5 );
    s = Cycle::analyse( s_log, Today );
    check( "spotting before: two periods", s.count, 2 );
    check( "spotting before: the cycle's length", s.periods[0].cycleLength, 28 );
    check( "spotting before: no warning", s.periods[1].warnings & Cycle::BetweenPeriods, 0 );
    check( "spotting before: day 13", s.cycleDay, 13 );
    reset();
    bleed( Today - 30, 5 );
    bleed( Today - 16, 1, 1 );   // mid-cycle spotting
    s = Cycle::analyse( s_log, Today );
    check( "mid-cycle spotting: no new cycle", s.count, 1 );
    check( "mid-cycle spotting: no warning", s.periods[0].warnings & Cycle::BetweenPeriods, 0 );

    // The log is a ring of 366 days: day 20496 (Today - 292) is its index 0. A period across that
    // wrap is still one period, and the cycles after it keep their lengths.
    reset();
    for ( int k = 0; k <= 10; ++k ) {
        bleed( Today - 294 + 28 * k, 5 );
    }
    s = Cycle::analyse( s_log, Today );
    check( "ring wrap: eleven periods", s.count, 11 );
    check( "ring wrap: the first starts before the wrap", s.periods[0].start, Today - 294 );
    check( "ring wrap: and lasts across it", s.periods[0].length, 5 );
    check( "ring wrap: its cycle", s.periods[0].cycleLength, 28 );
    reset();
    bleed( Today - 30, 5 );
    bleed( Today - 1, 2, 1 );    // two light days: a light period
    s = Cycle::analyse( s_log, Today );
    check( "two light days: a new cycle", s.count, 2 );
    reset();
    bleed( Today - 30, 5 );
    bleed( Today, 1, 1 );        // a light day today: a start until tomorrow says otherwise
    s = Cycle::analyse( s_log, Today );
    check( "light today: a start", s.cycleDay, 1 );

    // Six cycles (the mock-ups' sample).
    const int six[6] = { 26, 29, 27, 28, 31, 26 };
    reset();
    cycles( six, 6, 11 );
    s = Cycle::analyse( s_log, Today );
    check( "six: complete", s.complete, 6 );
    check( "six: cycle day", s.cycleDay, 12 );
    check( "six: mean", s.meanLength, 28 );
    check( "six: spread x100", static_cast<int>( s.spread * 100 + 0.5 ), 194 );
    check( "six: regular", s.irregular, 0 );
    check( "six: next early", s.nextEarly, 24 );
    check( "six: next late", s.nextLate, 32 );
    check( "six: next mean", s.nextMean, 28 );
    check( "six: Ogino first", s.fertileFirst, 8 );
    check( "six: Ogino last", s.fertileLast, 20 );
    check( "six: ovulation", s.ovulationDay, 15 );

    // Six regular cycles: Ogino gives 10-17, never narrower than Standard Days' 8-19.
    const int sixRegular[6] = { 28, 28, 28, 28, 28, 28 };
    reset();
    cycles( sixRegular, 6, 11 );
    s = Cycle::analyse( s_log, Today );
    check( "six regular: not narrower first", s.fertileFirst, 8 );
    check( "six regular: not narrower last", s.fertileLast, 19 );

    // A period that began before the log: its cycle is cut and left out of the averages.
    reset();
    bleed( Today - 365, 3 );  // the window's first day: the period's end
    bleed( Today - 365 + 58, 5 );
    bleed( Today - 365 + 58 + 28, 5 );
    s = Cycle::analyse( s_log, Today );
    check( "cut first period: a cycle less", s.complete, 1 );
    check( "cut first period: the real one", s.shortest, 28 );
    check( "cut first period: no warning", s.periods[0].warnings & Cycle::Long, 0 );

    const int four[4] = { 28, 28, 28, 28 };
    reset();
    cycles( four, 4, 11 );
    s = Cycle::analyse( s_log, Today );
    check( "four: Standard Days first", s.fertileFirst, 8 );
    check( "four: Standard Days last", s.fertileLast, 19 );
    check( "four: no spread, at least 2 days: early", s.nextEarly, 26 );
    check( "four: late", s.nextLate, 30 );
    check( "four: ovulation", s.ovulationDay, 15 );

    const int uneven[6] = { 24, 36, 28, 30, 26, 33 };
    reset();
    cycles( uneven, 6, 11 );
    s = Cycle::analyse( s_log, Today );
    check( "uneven: irregular", s.irregular, 1 );
    check( "uneven: union first", s.fertileFirst, 6 );
    check( "uneven: union last", s.fertileLast, 25 );

    const int two[2] = { 28, 28 };
    reset();
    cycles( two, 2, 11 );
    s = Cycle::analyse( s_log, Today );
    check( "two: wide window first", s.fertileFirst, 6 );
    check( "two: wide window last", s.fertileLast, 21 );
    check( "two: no ovulation", s.ovulationDay, 0 );
    check( "two: early", s.nextEarly, 26 );
    check( "two: late", s.nextLate, 30 );

    const int one[1] = { 30 };
    reset();
    cycles( one, 1, 11 );
    s = Cycle::analyse( s_log, Today );
    check( "one: early", s.nextEarly, 26 );
    check( "one: late", s.nextLate, 34 );

    // Adolescents.
    reset();
    s_log.teenMode = CycleLog::TeenOn;
    cycles( two, 2, 11 );
    s = Cycle::analyse( s_log, Today );
    check( "teen: on", s.teen, 1 );
    check( "teen: no fertile window", s.fertileFirst, 0 );
    check( "teen: no ovulation", s.ovulationDay, 0 );
    check( "teen, few cycles: early", s.nextEarly, 21 );
    check( "teen, few cycles: late", s.nextLate, 45 );
    const int teenThree[3] = { 30, 40, 35 };
    reset();
    s_log.teenMode = CycleLog::TeenOn;
    cycles( teenThree, 3, 11 );
    s = Cycle::analyse( s_log, Today );
    check( "teen, three cycles: early", s.nextEarly, 28 );
    check( "teen, three cycles: late", s.nextLate, 42 );
    reset();
    s_log.teenMode = CycleLog::TeenAuto;
    check( "auto without a date: teen", Cycle::teenActive( s_log, Today ), 1 );
    s_log.firstPeriodDay = Today - 2 * 365;
    check( "auto, two years: teen", Cycle::teenActive( s_log, Today ), 1 );
    s_log.firstPeriodDay = Today - 4 * 365;
    check( "auto, four years: adult", Cycle::teenActive( s_log, Today ), 0 );

    // Warnings.
    const int shortCycle[3] = { 28, 23, 28 };
    reset();
    cycles( shortCycle, 3, 11 );
    s = Cycle::analyse( s_log, Today );
    check( "adult 23 days: short", ( s.periods[1].warnings & Cycle::Short ) != 0, 1 );
    check( "adult 28 days: no warning", s.periods[0].warnings, 0 );
    const int longCycle[2] = { 28, 40 };
    reset();
    cycles( longCycle, 2, 11 );
    s = Cycle::analyse( s_log, Today );
    check( "adult 40 days: long", ( s.periods[1].warnings & Cycle::Long ) != 0, 1 );
    reset();
    s_log.teenMode = CycleLog::TeenOn;
    const int teenCycles[2] = { 22, 46 };
    cycles( teenCycles, 2, 11 );
    s = Cycle::analyse( s_log, Today );
    check( "teen 22 days: no warning", s.periods[0].warnings, 0 );
    check( "teen 46 days: long", ( s.periods[1].warnings & Cycle::Long ) != 0, 1 );
    reset();
    bleed( Today - 30, 9 );
    s = Cycle::analyse( s_log, Today );
    check( "adult 9 bleeding days: long bleeding", ( s.periods[0].warnings & Cycle::LongBleeding ) != 0, 1 );
    reset();
    bleed( Today - 30, 8 );
    s = Cycle::analyse( s_log, Today );
    check( "adult 8 bleeding days: fine", ( s.periods[0].warnings & Cycle::LongBleeding ) != 0, 0 );
    reset();
    s_log.teenMode = CycleLog::TeenOn;
    bleed( Today - 30, 8 );
    s = Cycle::analyse( s_log, Today );
    check( "teen 8 bleeding days: long bleeding", ( s.periods[0].warnings & Cycle::LongBleeding ) != 0, 1 );
    reset();
    bleed( Today - 90, 5 );
    s = Cycle::analyse( s_log, Today );
    check( "90 days: no period", ( s.warnings & Cycle::NoPeriod90 ) != 0, 1 );
    reset();
    bleed( Today - 89, 5 );
    s = Cycle::analyse( s_log, Today );
    check( "89 days: not yet", ( s.warnings & Cycle::NoPeriod90 ) != 0, 0 );
    reset();
    bleed( Today - 10, 2, 2 );
    bleed( Today - 8, 3, 3 );
    s = Cycle::analyse( s_log, Today );
    check( "three heavy days", ( s.periods[0].warnings & Cycle::ManyHeavy ) != 0, 1 );
    check( "three heavy days: counted", s.periods[0].heavyDays, 3 );

    // Carry a pad from the predicted range's first day until a period is logged.
    reset();
    cycles( four, 4, 25 );
    s = Cycle::analyse( s_log, Today );
    check( "carry pad: not yet", s.carryPad, 0 );
    reset();
    cycles( four, 4, 26 );
    s = Cycle::analyse( s_log, Today );
    check( "carry pad: from the early day", s.carryPad, 1 );

    printf( "Cycle: %d checks, %d failures\n", checks, failures );
    return failures == 0 ? 0 : 1;
}
