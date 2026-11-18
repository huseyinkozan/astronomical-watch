#pragma once

#include <cstdint>

#include "Astronomy.h"

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
        float hand;  // the direction it leaves the observer in, screen degrees from 180 to 360
        bool shown;
        bool bright;
    };

    // Directions as the arcs run: north of the equator east at the left end, south up, west at
    // the right end; south of it (`south`) mirrored, north up.
    // A centre in the half of the sky behind the screen (south of the equator the screen faces
    // north) is folded over the horizon, east and west kept on their sides.
    static Band band( double centreAltitude, double centreAzimuth, double sunAltitude, bool south, float observerX,
                      float observerY );

    // Whether point (x, y) of the band's circle is drawn: over the horizon, inside the half disc
    // of `clip` around the observer, and ahead of it along the hand (a nearly flat hand's circle
    // comes back over the horizon behind the observer).
    static bool drawn( const Band &band, float x, float y, float observerX, float observerY, float clip );

    // The Full arcs' band: a circle of DialRadius through the dial's centre, its tangent there
    // along the galactic centre's place on the 24 h dial, faded towards the clock, ending inside
    // the Sun's orbit, only at night; the end towards the centre bright while the centre is up.
    static const int DialRadius = 200;
    static const int DialWidth = 26;
    struct DialBand
    {
        float cx;  // the circle's centre
        float cy;
        float radius;
        float angle;     // the galactic centre's place on the dial, screen degrees
        float strength;  // by the Sun: 0 above -12 degrees, 1 from -18 down
        bool centreUp;
        bool shown;
    };

    // The galactic centre's hour angle, degrees (0 at the transit, growing with the time).
    static double hourAngle( int64_t time, double longitude );
    // Its place on the Full arcs' 24 h dial: the transit at the top, clockwise.
    static float dialAngle( double hourAngle );
    static DialBand dialBand( double centreAltitude, double sunAltitude, float dialAngle, float dialX, float dialY );
    // How strongly point (x, y) of the screen is lit by the band, 0 to 1.
    static float dialFade( const DialBand &band, float x, float y, float dialX, float dialY );
};
