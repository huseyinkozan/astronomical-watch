#pragma once

#include "CycleLog.h"

// Diyanet's (Hanafi) rulings on the logged bleeding, for the cycle page's hayız part
// (docs/superpowers/specs/2026-11-22-cycle-page-design.md): hayız lasts 3 to 10 days, clean days
// between its bleeding days within the ten count as hayız, at least 15 days of purity lie
// between two; shorter bleeding or bleeding before the 15 days is istihaze; past ten days the
// habit (the last hayız's length, 10 without one) is hayız and the rest istihaze. A period still
// going on counts as hayız so far. Plain C++, host-tested (tests/HayizTest.cpp).
class Hayiz
{
public:
    enum Kind
    {
        Purity,
        Hayz,
        Istihaze
    };
    struct Today
    {
        Kind kind;
        int day;    // the hayız's day, or the days of purity since the last one ended; 0 otherwise
        int habit;  // the last complete hayız's length, 0: none yet
    };

    static Kind kindOn( const CycleLog &log, int day );
    static Today today( const CycleLog &log, int day );

    // The hayız days among [first, first + length), as in a Ramadan.
    static int hayzDays( const CycleLog &log, int first, int length );

    // The make-up fasts left: the earlier balance and the ended Ramadans' hayız days, less the
    // fasts made up; never below 0.
    static int balance( const CycleLog &log );
};
