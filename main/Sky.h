#pragma once

#include <cstdint>

#include "Astronomy.h"
#include "Hijri.h"

// What the astronomy pages show (M10): values computed once for a local day, and the ones that
// change by the second. Plain C++ like Astronomy, so the host tests can use it too.
class Sky
{
public:
    // Prayer times by the method of Turkey's Diyanet, checked against its published 2026 tables
    // (tests/PrayerTest.cpp: 94 % to the minute, all within one): imsak at -18 degrees, the night
    // prayer at -17, asr with a shadow factor of 1, precautions of 0, -7, +5, +4, +7 and 0
    // minutes, rounded to the minute. Some countries use -16 for the night prayer (a setting).
    enum Prayer
    {
        Imsak,
        Sunrise,
        Dhuhr,
        Asr,
        Maghrib,
        Isha,
        PrayerCount
    };
    struct PrayerTimes
    {
        int64_t times[PrayerCount];
        bool valid[PrayerCount];
        // Above 44.5 degrees of latitude, or where twilight never gets dark enough, Diyanet
        // applies rules this code only approximates (a third of the night).
        bool approximate;
    };

    struct Day
    {
        int day;  // local day number (days since 1970-01-01 in local time)
        int64_t localMidnight;
        Astronomy::SunDay sun;
        int dayLengthChangeSeconds;  // against yesterday
        Astronomy::Crossings moon;
        // For the corner gauges (Corners): the Sun's and the Moon's events next to the day.
        Astronomy::Event sunsetBefore;     // yesterday's sunset
        Astronomy::Crossings moonBefore;   // yesterday's moonrise and moonset
        Astronomy::Crossings moonAfter;    // tomorrow's
        Astronomy::MoonPhase phase;  // at local noon
        Hijri::Date hijri;
        Hijri::Crescent crescent;    // this evening
        bool darkWindow;             // tonight's astronomical night has time without the Moon
        int64_t darkStart;
        int64_t darkEnd;
        int64_t nextSeason;
        int season;                  // 0 March equinox, 1 June solstice, 2 September, 3 December
        // The time page.
        int equationOfTime;          // apparent minus mean solar time, seconds
        double qiblaBearing;         // direction of the Kaaba, degrees from north (great circle)
        Astronomy::Event qibla;      // when the Sun stands in that direction (in daylight)
        int isoWeek;
        int dayOfYear;
        PrayerTimes prayers;
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
        int64_t time;  // the moment, Unix time
        bool sunUp;
        // Seconds of their day: apparent solar time, local mean sidereal time, and the ezani
        // (alaturka) time, which counts 12 hours from sunset (-1: no sunset to count from).
        int solarSeconds;
        int siderealSeconds;
        int ezaniSeconds;
        // The next prayer time (Prayer; tomorrow's imsak after the night prayer), -1: none.
        int nextPrayer;
        int secondsToPrayer;
        // The galactic centre (the half arcs' Milky Way), and whether the observer is south of
        // the equator (the arcs mirror).
        Astronomy::Position galacticCentre;
        double galacticHourAngle;  // degrees, 0 at its transit (the Full arcs' band)
        bool south;
    };

    // The prayer times of local day `day` (Diyanet's tables are in local civil time; the date is
    // what matters, the times are UTC).
    static PrayerTimes prayerTimes( int day, const Astronomy::Observer &observer, int ishaAngle = 17 );

    // `ishaAngle`: the Sun's depression for the night prayer, 17 (Turkey) or 16 degrees (the
    // countries where Diyanet uses it, research/prayer-times.md).
    static Day computeDay( int day, int utcOffsetMinutes, const Astronomy::Observer &observer, Hijri::Method method,
                           int ishaAngle = 17 );

    static Now computeNow( int64_t time, const Day &day, const Astronomy::Observer &observer );

    // 0 new Moon, 1 waxing crescent, 2 first quarter, 3 waxing gibbous, 4 full Moon,
    // 5 waning gibbous, 6 last quarter, 7 waning crescent.
    static int phaseIndex( const Astronomy::MoonPhase &phase );

    static Tide tide( const Astronomy::MoonPhase &phase );
};
