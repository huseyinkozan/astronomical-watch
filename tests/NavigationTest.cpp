// Host test of main/Almanac.cpp and main/Aiming.cpp (the observing page's nautical almanac and
// aiming parts) against Skyfield (tests/navigation_reference.csv,
// tools/make_navigation_reference.py), and of main/Magnetic.cpp against NOAA's WMM2025 test values
// (tests/wmm2025_test_values.txt, from the model's release).
// Build and run: sh tests/run_host_tests.sh

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Aiming.h"
#include "Almanac.h"
#include "Magnetic.h"

static int failures = 0;
static int checks = 0;

static void near( const char *name, double value, double expected, double tolerance )
{
    ++checks;
    if ( !( std::fabs( value - expected ) <= tolerance ) ) {
        printf( "FAIL %s: %.6f, expected %.6f\n", name, value, expected );
        ++failures;
    }
}

static void same( const char *name, const char *value, const char *expected )
{
    ++checks;
    if ( strcmp( value, expected ) != 0 ) {
        printf( "FAIL %s: \"%s\", expected \"%s\"\n", name, value, expected );
        ++failures;
    }
}

static double wrapped( double degrees )
{
    double value = std::fmod( degrees, 360.0 );
    if ( value > 180.0 ) {
        value -= 360.0;
    } else if ( value < -180.0 ) {
        value += 360.0;
    }
    return value;
}

static void formats()
{
    char text[32];
    Almanac::formatAngle( 112.5683, text, sizeof( text ) );
    same( "GHA", text, "112° 34.1'" );
    Almanac::formatAngle( 359.99999, text, sizeof( text ) );
    same( "GHA rounds to 0", text, "0° 00.0'" );
    Almanac::formatAngle( -1.5, text, sizeof( text ) );
    same( "GHA negative", text, "358° 30.0'" );
    Almanac::formatDeclination( 23.4367, "N", "S", text, sizeof( text ) );
    same( "declination north", text, "N 23° 26.2'" );
    Almanac::formatDeclination( -5.0, "K", "G", text, sizeof( text ) );
    same( "declination south, Turkish letters", text, "G 5° 00.0'" );
    Almanac::formatDeclination( 0.00001, "N", "S", text, sizeof( text ) );
    same( "declination zero", text, "0° 00.0'" );
    near( "noon altitude south of the zenith", Almanac::meridianAltitude( 41, 23.44 ), 72.44, 1e-9 );
    near( "noon altitude north of the zenith", Almanac::meridianAltitude( -10, 20 ), 60, 1e-9 );
}

static void panel()
{
    near( "year tilt", Aiming::yearTilt( -33.9 ), 33.9, 1e-12 );
    near( "day tilt", Aiming::dayTilt( 72.4 ), 17.6, 1e-9 );
    near( "panel south in the north", Aiming::panelAzimuth( 41 ), 180, 0 );
    near( "panel north in the south", Aiming::panelAzimuth( -33 ), 0, 0 );
    // Straight up at the sub-satellite point; due south of it on its meridian.
    const Astronomy::Position up = Aiming::dish( { 0, 42 }, 42 );
    near( "dish at the sub-satellite point", up.altitude, 90, 1e-9 );
    near( "dish due south", Aiming::dish( { 41, 42 }, 42 ).azimuth, 180, 1e-9 );
    near( "dish due north", Aiming::dish( { -30, 42 }, 42 ).azimuth, 0, 1e-9 );
    near( "satellite below the horizon", Aiming::dish( { 41, 29 }, -150 ).altitude < 0 ? 1 : 0, 1, 0 );
}

static void magnetic( const char *path )
{
    FILE *file = fopen( path, "r" );
    if ( file == nullptr ) {
        printf( "FAIL cannot open %s\n", path );
        ++failures;
        return;
    }
    char line[512];
    int rows = 0;
    while ( fgets( line, sizeof( line ), file ) != nullptr ) {
        if ( line[0] == '#' ) {
            continue;
        }
        double v[11] = {};
        int count = 0;
        char *p = line;
        for ( ; count < 11; ++count ) {
            char *end = nullptr;
            v[count] = strtod( p, &end );
            if ( end == p ) {
                break;
            }
            p = end;
        }
        if ( count < 11 ) {
            continue;
        }
        // The published values are rounded to 0.1 nT and 0.01 degree.
        const Magnetic::Field field = Magnetic::field( v[2], v[3], v[1], v[0] );
        near( "WMM X", field.x, v[4], 0.051 );
        near( "WMM Y", field.y, v[5], 0.051 );
        near( "WMM Z", field.z, v[6], 0.051 );
        near( "WMM declination", std::atan2( field.y, field.x ) * 180.0 / 3.14159265358979323846, v[10], 0.0051 );
        if ( v[1] == 0 ) {
            near( "declination()", Magnetic::declination( v[2], v[3], v[0] ), v[10], 0.0051 );
        }
        ++rows;
    }
    fclose( file );
    near( "WMM test rows", rows, 12, 0 );
    near( "decimal year 2025", Magnetic::decimalYear( 1735689600 ), 2025.0, 1e-9 );
    near( "decimal year mid 2027", Magnetic::decimalYear( 1814400000 ), 2027.494, 0.002 );
}

static void reference( const char *path )
{
    FILE *file = fopen( path, "r" );
    if ( file == nullptr ) {
        printf( "FAIL cannot open %s\n", path );
        ++failures;
        return;
    }
    // Worst errors: GHA and declination per body (arcmin), the rest.
    double worstGha[Almanac::BodyCount] = {};
    double worstDec[Almanac::BodyCount] = {};
    double worstSd = 0;
    double worstHp = 0;
    double worstPass = 0;
    double worstNoonAltitude = 0;
    double worstDish = 0;
    double worstSunAz = 0;
    int passes = 0;
    int sunRows = 0;
    // Tolerances in arcminutes. The Sun: Meeus' low-accuracy theory (0.01 degree); the Moon:
    // MiniMoon (a few arcminutes); the planets: JPL's approximate elements, whose geocentric errors
    // grow for Mars near opposition (tests/PlanetsTest.cpp), plus the nutation and aberration left
    // out (under 0.5').
    static const double ghaLimit[Almanac::BodyCount] = { 0.5, 4.0, 1.5, 3.0, 2.0, 2.5 };
    static const double decLimit[Almanac::BodyCount] = { 0.6, 3.0, 1.5, 3.0, 2.0, 2.0 };
    char line[256];
    while ( fgets( line, sizeof( line ), file ) != nullptr ) {
        if ( line[0] == '#' ) {
            continue;
        }
        char kind[8] = {};
        const char *comma = strchr( line, ',' );
        if ( comma == nullptr || comma - line >= static_cast<long>( sizeof( kind ) ) ) {
            continue;
        }
        memcpy( kind, line, comma - line );
        const char *rest = comma + 1;
        if ( strcmp( kind, "place" ) == 0 ) {
            int body = 0;
            long long time = 0;
            double gha = 0, dec = 0, sha = 0, sd = 0, hp = 0;
            sscanf( rest, "%d,%lld,%lf,%lf,%lf,%lf,%lf", &body, &time, &gha, &dec, &sha, &sd, &hp );
            const Almanac::Place place = Almanac::place( static_cast<Almanac::Body>( body ), time );
            const double ghaError = std::fabs( wrapped( place.gha - gha ) ) * 60.0;
            const double decError = std::fabs( place.declination - dec ) * 60.0;
            worstGha[body] = ghaError > worstGha[body] ? ghaError : worstGha[body];
            worstDec[body] = decError > worstDec[body] ? decError : worstDec[body];
            near( "GHA (arcmin)", ghaError, 0, ghaLimit[body] );
            near( "declination (arcmin)", decError, 0, decLimit[body] );
            near( "SHA (arcmin)", std::fabs( wrapped( place.sha - sha ) ) * 60.0, 0, ghaLimit[body] );
            // The Moon's radius taken as 0.2725 Earth radii (1738 km).
            const double sdError = std::fabs( place.semiDiameter - sd );
            const double hpError = std::fabs( place.parallax - hp );
            worstSd = sdError > worstSd ? sdError : worstSd;
            worstHp = hpError > worstHp ? hpError : worstHp;
            near( "semi-diameter (arcmin)", sdError, 0, 0.1 );
            near( "horizontal parallax (arcmin)", hpError, 0, 0.3 );
        } else if ( strcmp( kind, "pass" ) == 0 ) {
            int body = 0, occurs = 0;
            double lat = 0, lon = 0, altitude = 0;
            long long midnight = 0, time = 0;
            sscanf( rest, "%d,%lf,%lf,%lld,%d,%lld,%lf", &body, &lat, &lon, &midnight, &occurs, &time, &altitude );
            int64_t found = 0;
            const bool has = Almanac::meridianPassage( static_cast<Almanac::Body>( body ), midnight, lon, found );
            near( "pass occurs", has ? 1 : 0, occurs, 0 );
            if ( has && occurs ) {
                // The Moon's hour angle grows by a quarter of a degree a minute: its 4' gives 16 s.
                const double error = std::fabs( static_cast<double>( found - time ) );
                worstPass = error > worstPass ? error : worstPass;
                near( "pass time (s)", error, 0, body == Almanac::Moon ? 30 : 10 );
                ++passes;
            }
            if ( has && occurs && body == Almanac::Sun ) {
                const double declination = Almanac::place( Almanac::Sun, found ).declination;
                const double error = std::fabs( Almanac::meridianAltitude( lat, declination ) - altitude );
                worstNoonAltitude = error > worstNoonAltitude ? error : worstNoonAltitude;
                near( "noon altitude", error, 0, 0.02 );
            }
        } else if ( strcmp( kind, "dish" ) == 0 ) {
            double lat = 0, lon = 0, satellite = 0, elevation = 0, azimuth = 0;
            sscanf( rest, "%lf,%lf,%lf,%lf,%lf", &lat, &lon, &satellite, &elevation, &azimuth );
            const Astronomy::Position dish = Aiming::dish( { lat, lon }, satellite );
            // Straight up the azimuth means nothing.
            const double azimuthError = elevation > 89.9 ? 0 : std::fabs( wrapped( dish.azimuth - azimuth ) );
            const double error = std::fmax( std::fabs( dish.altitude - elevation ), azimuthError );
            worstDish = error > worstDish ? error : worstDish;
            near( "dish", error, 0, 0.01 );
        } else if ( strcmp( kind, "sunaz" ) == 0 ) {
            double lat = 0, lon = 0, azimuth = 0;
            long long midnight = 0, time = 0;
            int occurs = 0;
            sscanf( rest, "%lf,%lf,%lld,%lf,%d,%lld", &lat, &lon, &midnight, &azimuth, &occurs, &time );
            int64_t found = 0;
            const bool has = Aiming::sunAtAzimuth( midnight, { lat, lon }, azimuth, found );
            near( "Sun at the azimuth occurs", has ? 1 : 0, occurs, 0 );
            if ( has && occurs ) {
                // The Sun's 0.01 degree is a few seconds of its azimuth; slower near the horizon.
                const double error = std::fabs( static_cast<double>( found - time ) );
                worstSunAz = error > worstSunAz ? error : worstSunAz;
                near( "Sun at the azimuth (s)", error, 0, 60 );
                ++sunRows;
            }
        }
    }
    fclose( file );
    static const char *names[Almanac::BodyCount] = { "Sun", "Moon", "Venus", "Mars", "Jupiter", "Saturn" };
    for ( int i = 0; i < Almanac::BodyCount; ++i ) {
        printf( "     %-8s GHA %.2f' (limit %.1f')  declination %.2f' (limit %.1f')\n", names[i], worstGha[i],
                ghaLimit[i], worstDec[i], decLimit[i] );
    }
    printf( "     semi-diameter %.3f', parallax %.3f'; passes %d, worst %.0f s; noon altitude %.4f deg; dish %.4f deg; "
            "Sun at an azimuth %d, worst %.0f s\n",
            worstSd, worstHp, passes, worstPass, worstNoonAltitude, worstDish, sunRows, worstSunAz );
}

int main( int argc, char **argv )
{
    formats();
    panel();
    if ( argc > 2 ) {
        reference( argv[1] );
        magnetic( argv[2] );
    }
    printf( "Navigation: %d checks, %d failures\n", checks, failures );
    return failures == 0 ? 0 : 1;
}
