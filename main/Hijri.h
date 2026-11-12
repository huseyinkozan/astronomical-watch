#pragma once

#include <cstdint>

#include "Astronomy.h"

// Hijri (lunar Islamic) date (M10). Each month starts on the day after the first evening when the
// new crescent counts as seen, by one of two methods (a setting):
// - LocalCrescent: Odeh's criterion (Experimental Astronomy 18, 2004) at the observer's location:
//   the crescent must be visible to the naked eye (zone A).
// - Diyanet: the 1978 Istanbul criterion used by Turkey's Diyanet: at sunset somewhere on land the
//   Moon is at least 8 degrees from the Sun and at least 5 degrees above the horizon.
// Months are named by the tabular calendar at their middle, which is never more than two days
// away from an observed month. Plain C++17 like Astronomy; days are local civil day numbers
// (days since 1970-01-01 in local time).
class Hijri
{
public:
    enum Method
    {
        LocalCrescent = 0,
        Diyanet = 1
    };

    struct Date
    {
        int year;
        int month;  // 1 Muharram to 12 Dhu al-Hijjah
        int day;
    };

    // Odeh's visibility zones: A naked eye, B optical aid (then maybe the naked eye), C optical
    // aid only, D not visible. None: no crescent this evening (the Moon sets first, or the
    // conjunction has not happened yet), or the conjunction was more than three days ago.
    enum Zone
    {
        ZoneNone,
        ZoneA,
        ZoneB,
        ZoneC,
        ZoneD
    };

    struct Crescent
    {
        Zone zone;
        double v;          // Odeh's V
        int64_t bestTime;  // sunset + 4/9 of the time until moonset
    };

    // The new crescent at the observer on the evening of local day `day`.
    static Crescent crescent( int day, int utcOffsetMinutes, const Astronomy::Observer &observer );

    static Date date( int day, int utcOffsetMinutes, const Astronomy::Observer &observer, Method method );

    // Diyanet's condition for the evening of date `day` (days since 1970-01-01), anywhere on land.
    static bool diyanetEvening( int day, int64_t conjunction );

    // Tabular Islamic calendar (30-year cycle) of a civil day number.
    static Date tabular( int day );
};
