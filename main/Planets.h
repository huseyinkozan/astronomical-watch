#pragma once

#include <cstdint>

#include "Astronomy.h"

// The planets and the Earth, from JPL's "Keplerian elements for
// approximate positions of the major planets" (E. M. Standish, Table 1, valid 1800-2050) and
// Kepler's equation. Plain C++ like Astronomy, checked against Skyfield with the DE421
// ephemeris (tests/PlanetsTest.cpp).
class Planets
{
public:
    enum Body
    {
        Mercury,
        Venus,
        Earth,  // the Earth-Moon barycentre, within 7" of the Earth seen from the Sun
        Mars,
        Jupiter,
        Saturn,
        Uranus,
        Neptune,
        BodyCount
    };

    // In the J2000 ecliptic: degrees, AU.
    struct Heliocentric
    {
        double longitude;
        double latitude;
        double distance;
    };

    // J2000 right ascension and declination, as the light left the planet; AU.
    struct Geocentric
    {
        double raHours;
        double decDegrees;
        double distance;
    };

    static Heliocentric heliocentric( Body body, int64_t time );

    // Seen from the Earth's centre; not for the Earth itself.
    static Geocentric geocentric( Body body, int64_t time );

    // Topocentric altitude and azimuth (from north through east), without refraction.
    static Astronomy::Position position( Body body, int64_t time, const Astronomy::Observer &observer );

    // The first rise and set of the planet's centre through -0.5667 degrees (refraction, no
    // semi-diameter) in the local day from `localMidnight`; not for the Earth.
    static Astronomy::Crossings riseSet( Body body, int64_t localMidnight, const Astronomy::Observer &observer );

    // The five planets' rise and set of a local day (the Earth's entry stays empty).
    struct Day
    {
        Astronomy::Crossings riseSet[BodyCount];
    };
    static Day day( int64_t localMidnight, const Astronomy::Observer &observer );

    // A planet's place on the arc faces, by the faces' rule for the Sun and the Moon
    // (Corners::arcPlace) taken from the hour angle and the half arc above the horizon: no
    // search, a few milliseconds. Angles as LVGL's: clockwise from 3 o'clock. `orbit`: the Full
    // arcs' circle (the hour angle from the top, below the horizon too), else the Half arcs'
    // path, left to right in the north, right to left in the south (only while up). Not known
    // when the planet stays up or down all day.
    struct ArcPlace
    {
        bool known;
        bool up;
        float angle;
    };
    static ArcPlace arcPlace( Body body, int64_t time, const Astronomy::Observer &observer, bool orbit );

    // Which local day's rise and set the planets page needs: asked again until answered (an
    // answer may be lost while the display is busy), and again for a new local day or place.
    class DayRequest
    {
    public:
        // True, with that day's local midnight (Unix s), while the day of `now` at
        // `utcOffsetMinutes` and `observer` has no answer.
        bool needed( int64_t now, int utcOffsetMinutes, const Astronomy::Observer &observer, int64_t &localMidnight ) const;
        void answered( int64_t localMidnight, const Astronomy::Observer &observer );

    private:
        bool m_answered = false;
        int64_t m_midnight = 0;
        Astronomy::Observer m_observer = {};
    };
};
