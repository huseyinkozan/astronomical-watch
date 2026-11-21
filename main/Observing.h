#pragma once

#include <cstdint>

#include "Astronomy.h"

// The observing page's sky computations (docs/superpowers/plans/2026-11-21-tools.md, stage 4):
// the pole star for a polar scope, tonight's astronomical darkness and the Milky Way window.
// Plain C++ for the host tests (tests/ObservingTest.cpp, against Skyfield:
// tools/make_observing_reference.py). Times are Unix seconds, angles degrees.
class Observing
{
public:
    // ---- Polar alignment ----

    // The pole star of the observer's hemisphere: Polaris north of the equator, sigma Octantis
    // south of it (J2000, Hipparcos).
    static const double PolarisRaHours;
    static const double PolarisDecDegrees;
    static const double SigmaOctantisRaHours;
    static const double SigmaOctantisDecDegrees;

    // The pole star's hour angle (0 to 360, precessed to the date; nutation and aberration left
    // out, which moves this star's hour angle by up to about 1 degree, a quarter of a degree on
    // the 12-hour reticle).
    static double poleStarHourAngle( bool south, int64_t time, double longitude );

    // The star's place on a polar scope's reticle read as a 12-hour clock: minutes from 0 to
    // 719, 0 at the top (12 o'clock). Looking at the north celestial pole the stars turn
    // counterclockwise, at the south pole clockwise; at the hour angle 0 the star stands above the
    // pole. `inverted`: as most polar scopes show it, turned by 180 degrees (not mirrored).
    static int reticleMinutes( double hourAngle, bool south, bool inverted );

    // ---- Tonight ----

    // Tonight's astronomical darkness (the Sun's centre 18 degrees below the horizon, without
    // refraction) after the local noon `localNoon`: from the Sun's hour angle at that altitude,
    // its declination taken at each event (within a minute or two of Skyfield). `dark` false: the
    // Sun does not go that low (summer at high latitudes). A polar night is dark from one noon to
    // the next.
    struct Night
    {
        bool dark;
        int64_t dusk;
        int64_t dawn;
    };
    static Night night( int64_t localNoon, const Astronomy::Observer &observer );

    // ---- Milky Way window ----

    // The galactic centre (Sagittarius A*, J2000) counts as up above this altitude.
    static constexpr double CentreAltitude = 10.0;
    // The Moon does not spoil the sky while its centre is below the horizon or less than this of
    // its disc is lit (taken at the night's middle).
    static constexpr double FaintMoon = 0.10;
    // A shorter window is not counted.
    static const int MinimumWindow = 30 * 60;
    // The Moon is looked at in these steps; the window's ends set by the Moon are this exact.
    static const int MoonStep = 5 * 60;

    enum MoonState
    {
        MoonFaint,  // under FaintMoon lit: no matter where it is
        MoonDown,   // below the horizon while the centre is up in the dark
        MoonUp,     // above it all that time
        MoonSets,   // sets meanwhile (moonTime)
        MoonRises,  // rises meanwhile (moonTime)
    };

    struct MilkyWayNight
    {
        Night night;
        bool centreUp;      // the centre is above CentreAltitude at some time in the dark
        int64_t upStart;    // the longest such time
        int64_t upEnd;
        bool window;        // ... for at least MinimumWindow with the Moon out of the way
        int64_t start;      // the longest such stretch
        int64_t end;
        int64_t highest;    // the centre's highest time in the window and its altitude
        double highestAltitude;
        double moonIllumination;
        MoonState moon;
        int64_t moonTime;
    };

    // The night after the local noon `localNoon`.
    static MilkyWayNight milkyWay( int64_t localNoon, const Astronomy::Observer &observer );

    // Whether the centre can stand CentreAltitude high at the latitude at all.
    static bool centreRises( double latitude );

    // The first of the nights after `localNoon` (the next `nights` local noons, a day apart) with
    // a window; false when none has one. Nights without the centre up in the dark are passed over
    // without looking at the Moon.
    static bool nextMilkyWayNight( int64_t localNoon, int nights, const Astronomy::Observer &observer, int64_t &noon );

    // ---- Shared ----

    // The altitude of a point of right ascension and declination of the date at `time`.
    static double altitude( double rightAscension, double declination, int64_t time, const Astronomy::Observer &observer );

    // The time within [from, to] when the point stands highest (its transit, else the nearer end).
    static int64_t highestTime( double rightAscension, int64_t from, int64_t to, double longitude );
};
