#pragma once

#include <cstdint>

// Sun and Moon positions and events from the time and the observer's location (M10). Plain C++17
// with <cmath> only, no ESP-IDF, so that tests/AstronomyTest.cpp checks it on the host against
// Skyfield (tools/make_astronomy_reference.py).
//
// Times are Unix seconds (UTC); angles are degrees. A "day" starts at local midnight, given as
// the Unix time of that midnight, so the caller applies the UTC offset.
//
// Sources: J. Meeus, Astronomical Algorithms, 2nd ed. (Sun: chapter 25, low accuracy; sidereal
// time: 12; Moon distance: 47, main terms) and O. Montenbruck, T. Pfleger, Astronomy on the
// Personal Computer (Moon: MiniMoon series). Accuracy about 0.01 degree for the Sun and a few
// arcminutes for the Moon, which puts rise and set times within about a minute.
class Astronomy
{
public:
    struct Observer
    {
        double latitude;   // north positive
        double longitude;  // east positive
    };

    // Topocentric, without refraction. Azimuth from north through east.
    struct Position
    {
        double altitude;
        double azimuth;
    };

    struct Event
    {
        bool occurs;
        int64_t time;
    };

    // The first times in a day when the body's centre rises above or sets below an altitude. When
    // neither happens, the body stays above (or below) that altitude all day.
    struct Crossings
    {
        Event rising;
        Event setting;
        bool alwaysAbove;
        bool alwaysBelow;
    };

    struct SunDay
    {
        Crossings riseSet;       // upper limb on the horizon with refraction: -0.833 degrees
        Crossings civil;         // -6 degrees
        Crossings nautical;      // -12 degrees
        Crossings astronomical;  // -18 degrees
        Crossings goldenHour;    // +6 degrees: the golden hour lies between this and -4 degrees
        Crossings blueHour;      // -4 degrees: the blue hour lies between this and -6 degrees
        Event noon;              // transit (local apparent noon)
        double noonAltitude;
        int dayLengthSeconds;    // above -0.833 degrees; 0 or 86400 without rise or set
    };

    struct MoonPhase
    {
        double illumination;  // illuminated fraction of the disc, 0 to 1
        double elongation;    // Moon minus Sun in ecliptic longitude, 0 to 360: 0 new, 180 full
        double ageDays;       // since the last new Moon
    };

    static Position sunPosition( int64_t time, const Observer &observer );
    static Position moonPosition( int64_t time, const Observer &observer );

    static SunDay sunDay( int64_t localMidnight, const Observer &observer );

    // Moonrise and moonset: the upper limb on the horizon, with refraction and parallax.
    static Crossings moonDay( int64_t localMidnight, const Observer &observer );

    // The first moonset within `span` seconds after `start`, searched forwards and stopping
    // there: much faster than moonDay() when the Moon sets soon. False when the Moon is already
    // down at `start` or does not set within the span.
    static bool moonsetWithin( int64_t start, int span, const Observer &observer, int64_t &moonset );

    static MoonPhase moonPhase( int64_t time );

    // New Moon (conjunction in ecliptic longitude) at or before `time`.
    static int64_t previousNewMoon( int64_t time );

    // The next equinox or solstice after `time`; season 0: March equinox, 1: June solstice,
    // 2: September equinox, 3: December solstice.
    static int64_t nextSeason( int64_t time, int &season );

    // Geocentric angle between the centres of the Moon and the Sun.
    static double moonSunSeparation( int64_t time );

    // Geocentric semi-diameter of the Moon.
    static double moonSemiDiameter( int64_t time );

    // Sunset (centre at -0.833 degrees) of the day around `localNoon`, from the Sun's hour angle:
    // within about a minute and much faster than sunDay(). False when the Sun does not set.
    static bool sunsetNear( int64_t localNoon, const Observer &observer, int64_t &sunset );
};
