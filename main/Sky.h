#pragma once

#include <cstdint>

#include "Astronomy.h"
#include "Hijri.h"

// What the astronomy pages show (M10): values computed once for a local day, and the ones that
// change by the second. Plain C++ like Astronomy, so the host tests can use it too.
class Sky
{
public:
    struct Day
    {
        int day;  // local day number (days since 1970-01-01 in local time)
        int64_t localMidnight;
        Astronomy::SunDay sun;
        int dayLengthChangeSeconds;  // against yesterday
        Astronomy::Crossings moon;
        Astronomy::MoonPhase phase;  // at local noon
        Hijri::Date hijri;
        Hijri::Crescent crescent;    // this evening
        bool darkWindow;             // tonight's astronomical night has time without the Moon
        int64_t darkStart;
        int64_t darkEnd;
        int64_t nextSeason;
        int season;                  // 0 March equinox, 1 June solstice, 2 September, 3 December
    };

    // Spring tides near new and full Moon, neap tides near the quarters (a general guide: the
    // local timing of the tides can lag by days).
    enum Tide
    {
        TideSpring,
        TideNeap,
        TideMiddle
    };

    struct Now
    {
        Astronomy::Position sun;
        Astronomy::Position moon;
        // Time to the next sunset while the Sun is up, else to the next sunrise; 0 without one.
        int secondsToSunEvent;
        bool sunUp;
    };

    static Day computeDay( int day, int utcOffsetMinutes, const Astronomy::Observer &observer, Hijri::Method method );

    static Now computeNow( int64_t time, const Day &day, const Astronomy::Observer &observer );

    // 0 new Moon, 1 waxing crescent, 2 first quarter, 3 waxing gibbous, 4 full Moon,
    // 5 waning gibbous, 6 last quarter, 7 waning crescent.
    static int phaseIndex( const Astronomy::MoonPhase &phase );

    static Tide tide( const Astronomy::MoonPhase &phase );
};
