#pragma once

// When the battery is too low to keep the always-on face (CLAUDE.md, power management): at or
// below 10 % the watch powers off, so that the cell still runs the RTC for days. Plain C++ for
// the host tests (tests/BatteryLimitTest.cpp).
class BatteryLimit
{
public:
    // The gauge reads too low for a while after a power cycle (docs/hardware.md, run D), so VBAT
    // must agree; 3.65 V is about 10 % on a Li-ion cell.
    static const int PowerOffPercent = 10;
    static const int PowerOffMillivolts = 3650;
    // VBAT alone, for a gauge that stays high on a flat cell: about 3-5 % left (an estimate).
    static const int BackstopMillivolts = 3450;
    // The floor while an alarm is pending (the owner, 2026-11-14: about 2 %, so that the RTC
    // keeps its time): 2 % with VBAT at most 3.50 V, or VBAT alone at most 3.40 V.
    static const int FloorPercent = 2;
    static const int FloorPairedMillivolts = 3500;
    static const int FloorMillivolts = 3400;

    static bool empty( int millivolts, int percent );
    static bool atFloor( int millivolts, int percent );
};
