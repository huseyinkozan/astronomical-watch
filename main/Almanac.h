#pragma once

#include <cstddef>
#include <cstdint>

// The observing page's nautical almanac (docs/superpowers/plans/2026-11-21-tools.md, stage 4):
// the Sun's, the Moon's and the four navigational planets' Greenwich hour angle and declination
// as a nautical almanac lists them, their semi-diameters and the Moon's horizontal parallax, and
// the meridian passage. From Astronomy and Planets; plain C++ for the host tests
// (tests/NavigationTest.cpp, against Skyfield: tools/make_navigation_reference.py). Times are Unix
// seconds, angles degrees unless named otherwise.
class Almanac
{
public:
    enum Body
    {
        Sun,
        Moon,
        Venus,
        Mars,
        Jupiter,
        Saturn,
        BodyCount
    };

    struct Place
    {
        double gha;           // the Greenwich hour angle, 0 to 360
        double declination;
        double sha;           // the sidereal hour angle: 360 minus the right ascension
        double semiDiameter;  // arcminutes: the Sun and the Moon (0 for the planets)
        double parallax;      // the Moon's horizontal parallax, arcminutes (0 for the others)
    };

    // The Sun's apparent place against the apparent sidereal time; the Moon's and the planets'
    // against the mean equinox and the mean sidereal time, which leaves out only the nutation's
    // part across the hour circle and the aberration (under half an arcminute).
    static Place place( Body body, int64_t time );

    // The body's upper meridian passage at `longitude` in the local day from `localMidnight`
    // (the Sun's: local apparent noon). False on the day of a month without one (the Moon).
    static bool meridianPassage( Body body, int64_t localMidnight, double longitude, int64_t &time );

    // The geocentric altitude of a body of `declination` on the meridian at `latitude`.
    static double meridianAltitude( double latitude, double declination );

    // As an almanac writes them: "112° 34.1'" (0 to 360), and a declination with the
    // hemisphere's letter, "N 23° 26.2'" (`north`, `south`: the language's letters).
    static void formatAngle( double degrees, char *text, size_t size );
    static void formatDeclination( double degrees, const char *north, const char *south, char *text, size_t size );
};
