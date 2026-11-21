#pragma once

#include <cstdint>

#include "Astronomy.h"

// The astrolabe of the observing page (docs/superpowers/plans/2026-11-21-tools.md): the plate's
// and the rete's geometry, the alidade's answers, the sundial's hour lines and a few bright
// stars. Plain C++ for the host tests (tests/AstrolabeTest.cpp, against Skyfield where a value
// is astronomical: tools/make_astrolabe_reference.py).
//
// The dial is a stereographic projection from the south celestial pole onto the plane of the
// equator, as on a northern astrolabe: the north celestial pole in the centre, the meridian
// upwards (the noon Sun at the top), east on the left, the sky turning clockwise. Units: the
// equator's radius is 1; x to the right, y up. South of the equator the declinations and the
// latitude change sign (the south pole in the centre), so the same drawing serves. The plate
// ends at the tropic of the far hemisphere (the limit circle), as an astrolabe's does.
class Astrolabe
{
public:
    struct Point
    {
        double x;
        double y;
    };

    struct Circle
    {
        double x;  // centre
        double y;
        double radius;
    };

    // J2000 positions (Hipparcos, rounded to 0.0001 h and 0.001 degree); the dial precesses
    // them to the date (Astronomy::precess), which moves them by up to 0.4 degree since 2000.
    struct Star
    {
        const char *name;
        double raHours;
        double decDegrees;
        double magnitude;
    };
    static const int StarCount = 20;
    static const Star Stars[StarCount];
    static const int Polaris = StarCount - 1;  // the last of the table

    // The tropics of the plate.
    static constexpr double PlateObliquity = 23.44;

    // The latitude the dial is drawn for: |latitude|, at least 1 degree (at the equator the
    // horizon would pass through the projection's pole).
    static double plateLatitude( double latitude );

    // A point of the sky: its hour angle (degrees, positive west of the meridian) and declination.
    static Point project( double hourAngle, double declination, double latitude );

    // The radius of the limit circle (the far tropic) and of a circle of declination (the
    // equator 1, the near tropic about 0.66), all centred on the pole.
    static double limitRadius();
    static double declinationRadius( double declination, double latitude );

    // The plate's circle of an altitude (0: the horizon; almucantars above it).
    static Circle almucantar( double altitude, double latitude );

    // The rete's ecliptic at local sidereal time `siderealTime` (degrees) with `obliquity`.
    static Circle ecliptic( double siderealTime, double obliquity, double latitude );

    // The part of `circle` inside the limit circle of radius `limit`: from `start` to `end`
    // degrees (counterclockwise from +x, end > start; the whole circle 0 to 360). False when
    // nothing of it is inside.
    static bool arcInside( const Circle &circle, double limit, double &start, double &end );

    // The hour angle (degrees, -180 to 180) of a right ascension (degrees) at `time` and longitude.
    static double hourAngle( double rightAscension, int64_t time, double longitude );

    // Alidade: the time when the Sun's centre stands `altitude` degrees high (geometric, without
    // refraction) at the observer, the morning or afternoon solution nearest `now`. False when
    // the Sun does not reach that altitude that day (above its noon altitude, say).
    static bool timeFromSunAltitude( double altitude, int64_t now, const Astronomy::Observer &observer, int64_t &time );

    // Alidade: the latitude from Polaris' altitude at `time` and longitude (Polaris is about
    // 0.65 degree from the pole: its hour angle corrects for it). False without an answer.
    static bool latitudeFromPolaris( double altitude, int64_t time, double longitude, double &latitude );

    // A horizontal sundial: the hour line of `hourAngle` (degrees; the shadow's angle from the
    // noon line, positive for the afternoon, -180 to 180) for `latitude` (its size only: south of
    // the equator the dial is the mirror image).
    static double sundialAngle( double latitude, double hourAngle );
};
