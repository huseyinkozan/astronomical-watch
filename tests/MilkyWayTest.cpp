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

    // The hand stays over the horizon: a centre in the half of the sky behind the screen (south of
    // the equator the screen faces north) is folded up, east and west kept on their sides.
    band = MilkyWay::band( 40, 180, -30, false, ox, oy );
    check( "north, due south: the hand straight up", std::fabs( band.hand - 270 ) < 0.01f );
    band = MilkyWay::band( 40, 110, -30, true, ox, oy );
    check( "south of the equator, south-east: the hand up and to the right (east)",
           band.hand > 270 && band.hand < 360 );
    band = MilkyWay::band( 40, 246, -30, true, ox, oy );
    check( "south of the equator, west: the hand up and to the left", band.hand > 180 && band.hand < 270 );
    // Only the part that leaves the observer along the hand: a nearly flat hand's circle comes
    // back over the horizon on the other side, which is not drawn.
    for ( int i = 0; i < 2; ++i ) {
        const double azimuth = i == 0 ? 92.2 : 279.8;
        band = MilkyWay::band( 30, azimuth, -30, true, ox, oy );
        int ahead = 0;
        int behind = 0;
        for ( int degree = 0; degree < 360; ++degree ) {
            const float a = degree * static_cast<float>( M_PI ) / 180.0f;
            const float x = band.cx + band.radius * cosf( a );
            const float y = band.cy + band.radius * sinf( a );
            if ( !MilkyWay::drawn( band, x, y, ox, oy, 160 ) ) {
                continue;
            }
            const float h = band.hand * static_cast<float>( M_PI ) / 180.0f;
            if ( ( x - ox ) * cosf( h ) + ( y - oy ) * sinf( h ) > 0 ) {
                ++ahead;
            } else {
                ++behind;
            }
        }
        char name[64];
        snprintf( name, sizeof( name ), "south of the equator, azimuth %.1f: one piece along the hand", azimuth );
        check( name, ahead > 10 && behind == 0 );
    }
    check( "below the horizon: not drawn", !MilkyWay::drawn( band, ox + 10, oy + 5, ox, oy, 160 ) );
    check( "outside the half disc: not drawn", !MilkyWay::drawn( band, ox, oy - 170, ox, oy, 160 ) );

    // The Full arcs band: a circle of r 200 through the dial's centre, its tangent there along
    // the galactic centre's place on the 24 h dial (270 + its hour angle: the top at the transit).
    // It fades towards the clock (nothing within 50 px, full from 110), ends inside the Sun's ring,
    // shows only at night (the Sun below -12 degrees, full from -18), and the end towards the
    // centre is bright while the centre is up, the other end and both while it is down dim.
    {
        const Astronomy::Observer istanbul = { 41.0082, 28.9784 };
        int64_t transit = 0;
        double highest = -90;
        for ( int64_t t = 1782072000; t < 1782072000 + 86400; t += 60 ) {
            const double altitude = Astronomy::starPosition( MilkyWay::CentreRaHours, MilkyWay::CentreDecDegrees, t, istanbul ).altitude;
            if ( altitude > highest ) {
                highest = altitude;
                transit = t;
            }
        }
        const float atTransit = MilkyWay::dialAngle( MilkyWay::hourAngle( transit, istanbul.longitude ) );
        check( "dial: the transit at the top", std::fabs( atTransit - 270 ) < 1 );
        const float later = MilkyWay::dialAngle( MilkyWay::hourAngle( transit + 6 * 3600, istanbul.longitude ) );
        check( "dial: six hours later a quarter turn on", std::fabs( later - 0 ) < 2 || std::fabs( later - 360 ) < 2 );
    }
    const float dx = 205;
    const float dy = 251;
    MilkyWay::DialBand dial = MilkyWay::dialBand( 20, -25, 270, dx, dy );
    check( "dial band: circle centre a quarter turn aside", std::fabs( dial.cx - ( dx + 200 ) ) < 0.5f && std::fabs( dial.cy - dy ) < 0.5f );
    // Points on the circle at a given distance from the dial's centre, up (towards the centre's
    // place) and down.
    auto onCircle = [&]( float distance, bool up, float &x, float &y ) {
        // The circle through the dial's centre: a chord of length `distance` from it.
        const float half = std::asin( distance / ( 2 * dial.radius ) );
        const float a = static_cast<float>( M_PI ) + ( up ? 2 : -2 ) * half;
        x = dial.cx + dial.radius * cosf( a );
        y = dial.cy + dial.radius * sinf( a );
    };
    float px = 0;
    float py = 0;
    onCircle( 30, true, px, py );
    check( "dial band: nothing at the clock", MilkyWay::dialFade( dial, px, py, dx, dy ) == 0 );
    onCircle( 130, true, px, py );
    check( "dial band: our sky's end full", std::fabs( MilkyWay::dialFade( dial, px, py, dx, dy ) - 1 ) < 0.01f && py < dy );
    onCircle( 130, false, px, py );
    check( "dial band: the other end dim", std::fabs( MilkyWay::dialFade( dial, px, py, dx, dy ) - 0.35f ) < 0.01f && py > dy );
    onCircle( 80, true, px, py );
    check( "dial band: half way faded", std::fabs( MilkyWay::dialFade( dial, px, py, dx, dy ) - 0.5f ) < 0.02f );
    onCircle( 149, true, px, py );
    check( "dial band: ends inside the Sun's ring", MilkyWay::dialFade( dial, px, py, dx, dy ) == 0 );
    check( "dial band: off the band", MilkyWay::dialFade( dial, dx + 60, dy - 120, dx, dy ) == 0 );
    dial = MilkyWay::dialBand( -10, -25, 270, dx, dy );
    onCircle( 130, true, px, py );
    check( "dial band: the centre down, both ends dim", std::fabs( MilkyWay::dialFade( dial, px, py, dx, dy ) - 0.35f ) < 0.01f );
    dial = MilkyWay::dialBand( 20, -15, 270, dx, dy );
    onCircle( 130, true, px, py );
    check( "dial band: half strength at -15 degrees", std::fabs( MilkyWay::dialFade( dial, px, py, dx, dy ) - 0.5f ) < 0.01f );
    dial = MilkyWay::dialBand( 20, -5, 270, dx, dy );
    check( "dial band: hidden by day", !dial.shown );

    printf( "Milky Way: %d checks, %d failures\n", s_checks, s_failures );
    return s_failures == 0 ? 0 : 1;
}
