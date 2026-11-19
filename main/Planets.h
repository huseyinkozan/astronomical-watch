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
};
