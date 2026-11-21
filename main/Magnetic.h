#pragma once

#include <cstdint>

// The magnetic declination at a place from the World Magnetic Model WMM2025 (NOAA NCEI, public
// domain; degree and order 12, valid 2025.0 to 2030.0, extrapolated outside), for the observing
// page's aiming part: a compass reads the true bearing minus the declination. Plain C++ for the
// host tests (tests/NavigationTest.cpp, against NOAA's test values). The coefficients take 1.4 KB
// of flash (tools/make_wmm_table.py).
class Magnetic
{
public:
    // The field's north, east and down components (nT) at the geodetic `latitude`,
    // `longitude` (degrees) and `heightKm` above the WGS 84 ellipsoid in `decimalYear`.
    struct Field
    {
        double x;
        double y;
        double z;
    };
    static Field field( double latitude, double longitude, double heightKm, double decimalYear );

    // The declination, degrees east of true north (west negative), at the ellipsoid's surface.
    static double declination( double latitude, double longitude, double decimalYear );

    // A Unix time as a decimal year (2025.0 at the start of 2025).
    static double decimalYear( int64_t time );
};
