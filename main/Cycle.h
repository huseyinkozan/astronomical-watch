#pragma once

#include <cstdint>

#include "CycleLog.h"

// What the cycle page shows, derived from the log (docs/superpowers/specs/
// 2026-11-22-cycle-page-design.md, "Rules"): the periods and cycles, the next start's likely
// range, the fertile window as an estimate, regularity and warning signs. Plain C++, host-tested
// (tests/CycleTest.cpp). Days are local day numbers; "cycle days" count from 1 at a start.
class Cycle
{
public:
    // A year of cycles of 21 days or more.
    static const int MaxCycles = 18;
    enum Warning : uint16_t
    {
        Short = 1,          // a cycle under 24 days (adolescents 21)
        Long = 2,           // over 38 days (adolescents 45), also the current one so far
        LongBleeding = 4,   // a period over 8 days (adolescents 7)
        NoPeriod90 = 8,     // 90 days or more since the last start
        ManyHeavy = 16,     // three or more heavy days in a period
        BetweenPeriods = 32 // bleeding logged after a period ended, not starting a new cycle
    };
    struct Period
    {
        int start;         // its first bleeding day
        int length;        // from the start to its last bleeding day (one clean day may lie inside)
        int bleedingDays;
        int heavyDays;
        int cycleLength;   // to the next start; 0: the current cycle
        uint16_t warnings;
        bool cut;          // it may have begun before the log: left out of the averages
    };
    struct State
    {
        Period periods[MaxCycles];  // oldest first; the last is the current cycle
        int count;
        int today;
        int cycleDay;     // today's day in the current cycle, 0: no period logged
        bool teen;
        bool irregular;   // fewer than 3 complete cycles, or a spread of more than 9 days
        int complete;     // complete cycles taken into the averages (at most 6)
        int meanLength;   // their mean, rounded
        double spread;    // their standard deviation
        int shortest;
        int longest;
        // The next start's likely range and its middle, in days after the current start
        // (the next start is the day current start + n); 0: no prediction.
        int nextEarly;
        int nextLate;
        int nextMean;
        int fertileFirst;  // cycle days of the fertile window (an estimate); 0: none
        int fertileLast;
        int ovulationDay;  // cycle day of the estimated ovulation; 0: none
        bool carryPad;     // inside the predicted range, no period logged yet
        uint16_t warnings; // the current cycle's
    };

    static State analyse( const CycleLog &log, int today );

    // The adolescent mode: on, or automatic for three years from the first period (on while no
    // date is entered).
    static bool teenActive( const CycleLog &log, int today );
};
