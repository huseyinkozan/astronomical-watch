#pragma once

// The Milky Way band of the half arcs (docs/superpowers/specs/2026-11-18-face-templates-design.md):
// a circle of fixed size through the observer (the horizon's middle) that leaves it like a clock
// hand towards the galactic centre's azimuth and bends upwards. It does not follow the galactic
// plane's real tilt. Plain C++ for the host tests.
class MilkyWay
{
public:
    // The galactic centre (Sagittarius A*, J2000).
    static constexpr double CentreRaHours = 17 + 45 / 60.0 + 40.04 / 3600.0;
    static constexpr double CentreDecDegrees = -( 29 + 0 / 60.0 + 28.1 / 3600.0 );

    static const int Radius = 260;

    // The circle's centre on the screen (y downwards) and radius; `shown` while the galactic
    // centre is above the horizon, `bright` when the sky is dark too (the Sun 18 degrees down).
    struct Band
    {
        float cx;
        float cy;
        float radius;
        bool shown;
        bool bright;
    };

    // Directions as the arcs run: north of the equator east at the left end, south up, west at
    // the right end; south of it (`south`) mirrored, north up.
    static Band band( double centreAltitude, double centreAzimuth, double sunAltitude, bool south, float observerX,
                      float observerY );
};
