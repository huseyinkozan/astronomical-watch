// Host test of the galactic centre's place (Astronomy::starPosition) and the Milky Way band of the
// half arcs (main/MilkyWay.cpp). Build and run: sh tests/run_host_tests.sh

#include <cmath>
#include <cstdio>

#include "Astronomy.h"
#include "MilkyWay.h"

static int s_checks = 0;
static int s_failures = 0;

static void check( const char *name, bool condition )
{
    ++s_checks;
    if ( !condition ) {
        printf( "FAIL %s\n", name );
        ++s_failures;
    }
}

static double angleDifference( double a, double b )
{
    const double d = std::fmod( std::fabs( a - b ), 360.0 );
    return d > 180 ? 360 - d : d;
}

int main()
{
    // Skyfield with DE421: Star(ra_hours=(17, 45, 40.04), dec_degrees=(-29, 0, 28.1)) observed
    // apparent from wgs84.latlon(...), altaz() without refraction.
    struct Reference
    {
        double latitude;
        double longitude;
        int64_t time;
        double altitude;
        double azimuth;
    };
    const Reference references[8] = {
        { 41.0082, 28.9784, 1782058740, -13.9231, 116.5345 }, { 41.0082, 28.9784, 1782072000, 15.3795, 154.9433 },
        { 41.0082, 28.9784, 1797854400, 13.8874, 208.6967 },  { 41.0082, 28.9784, 1773108000, 10.7520, 145.0283 },
        { -33.9249, 18.4241, 1782058740, 12.8694, 116.4880 }, { -33.9249, 18.4241, 1782072000, 57.0815, 92.1093 },
        { -33.9249, 18.4241, 1797854400, 70.9285, 278.9931 }, { -33.9249, 18.4241, 1773108000, 46.9612, 97.8324 } };
    for ( int i = 0; i < 8; ++i ) {
        const Reference &r = references[i];
        const Astronomy::Position p =
            Astronomy::starPosition( MilkyWay::CentreRaHours, MilkyWay::CentreDecDegrees, r.time, { r.latitude, r.longitude } );
        char name[64];
        snprintf( name, sizeof( name ), "galactic centre %d within 0.1 deg (%.3f %.3f)", i, p.altitude, p.azimuth );
        // The azimuth is ill-defined near the zenith; the altitude is checked always.
        check( name, std::fabs( p.altitude - r.altitude ) < 0.1 &&
                         ( r.altitude > 80 || angleDifference( p.azimuth, r.azimuth ) < 0.1 ) );
    }

    // The band: a circle of radius 260 through the observer that leaves it like a clock hand
    // towards the centre's azimuth (north: east left, south up, west right; south of the equator
    // mirrored, north up), bending upwards: its centre a quarter turn aside, on the upper side
    // (straight up: to the right).
    const float ox = 205;
    const float oy = 232;
    MilkyWay::Band band = MilkyWay::band( -5, 180, -30, false, ox, oy );
    check( "band hidden with the centre down", !band.shown );
    band = MilkyWay::band( 20, 180, -30, false, ox, oy );
    check( "band shown and bright in a dark sky", band.shown && band.bright );
    check( "south: the band leaves upwards, its centre to the right",
           std::fabs( band.cx - ( ox + 260 ) ) < 0.5f && std::fabs( band.cy - oy ) < 0.5f );
    check( "band radius 260", band.radius == 260 );
    band = MilkyWay::band( 20, 90, -10, false, ox, oy );
    check( "band dim in a lighter sky", band.shown && !band.bright );
    check( "east: the band leaves to the left, its centre above",
           std::fabs( band.cx - ox ) < 0.5f && std::fabs( band.cy - ( oy - 260 ) ) < 0.5f );
    band = MilkyWay::band( 20, 270, -30, false, ox, oy );
    check( "west: the band leaves to the right, its centre above",
           std::fabs( band.cx - ox ) < 0.5f && std::fabs( band.cy - ( oy - 260 ) ) < 0.5f );
    band = MilkyWay::band( 20, 210, -30, false, ox, oy );
    check( "south-west: the centre up and to the left",
           band.cx < ox && band.cy < oy && std::fabs( std::hypot( band.cx - ox, band.cy - oy ) - 260 ) < 0.5f );
    band = MilkyWay::band( 20, 0, -30, true, ox, oy );
    check( "south of the equator, north up", std::fabs( band.cx - ( ox + 260 ) ) < 0.5f && std::fabs( band.cy - oy ) < 0.5f );
    band = MilkyWay::band( 20, 90, -30, true, ox, oy );
    check( "south of the equator, east to the right", std::fabs( band.cx - ox ) < 0.5f && std::fabs( band.cy - ( oy - 260 ) ) < 0.5f );
    band = MilkyWay::band( 0, 180, -30, false, ox, oy );
    check( "centre exactly on the horizon: hidden", !band.shown );

    printf( "Milky Way: %d checks, %d failures\n", s_checks, s_failures );
    return s_failures == 0 ? 0 : 1;
}
