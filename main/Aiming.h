#pragma once

#include <cstdint>

#include "Astronomy.h"

// The observing page's aiming part (docs/superpowers/plans/2026-11-21-tools.md, stage 4): where
// a dish points to a geostationary satellite, when the Sun stands in a direction, and a fixed
// solar panel's tilt. Plain C++ for the host tests (tests/NavigationTest.cpp, the dish against
// Skyfield: tools/make_navigation_reference.py). Angles in degrees, azimuths from true north
// through east, times Unix seconds.
class Aiming
{
public:
    // The dish's elevation (without refraction) and true azimuth from a place on the WGS 84
    // ellipsoid to a geostationary satellite above `satelliteLongitude` (east positive).
    static Astronomy::Position dish( const Astronomy::Observer &observer, double satelliteLongitude );

    // The first time in the local day from `localMidnight` when the Sun's centre stands above the
    // horizon at `azimuth`; false when it does not (the direction is in the night, or never
    // reached at this latitude). Samples every 10 min, then bisection to a second.
    static bool sunAtAzimuth( int64_t localMidnight, const Astronomy::Observer &observer, double azimuth,
                              int64_t &time );

    // A fixed panel facing the equator: tilted by about the latitude all year; the best tilt today
    // faces the Sun at noon (90 minus its noon altitude). Its azimuth: south north of the equator.
    static double yearTilt( double latitude );
    static double dayTilt( double noonAltitude );
    static double panelAzimuth( double latitude );
};
