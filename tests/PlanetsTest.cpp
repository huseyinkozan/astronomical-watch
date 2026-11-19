// Host test of main/Planets.cpp against Skyfield with the JPL DE421 ephemeris
// (tests/planet_reference.csv, made by tools/make_planet_reference.py).
// Build and run: sh tests/run_host_tests.sh

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Planets.h"

static const char *s_names[Planets::BodyCount] = { "mercury", "venus", "earth", "mars", "jupiter", "saturn", "uranus", "neptune" };

static int bodyOf( const char *name )
{
    for ( int i = 0; i < Planets::BodyCount; ++i ) {
        if ( std::strcmp( name, s_names[i] ) == 0 ) {
            return i;
        }
    }
    return -1;
}

static double angleDifference( double a, double b )
{
    double d = std::fmod( a - b, 360.0 );
    if ( d > 180 ) {
        d -= 360;
    } else if ( d < -180 ) {
        d += 360;
    }
    return d;
}

// The largest error of each body: angles in arcminutes, distances in AU, altitude and azimuth
// in degrees (azimuth times the cosine of the altitude: the distance on the sky).
static double s_angle[Planets::BodyCount] = {};
static double s_distance[Planets::BodyCount] = {};
static double s_sky[Planets::BodyCount] = {};

int main( int argc, char **argv )
{
    FILE *file = std::fopen( argc > 1 ? argv[1] : "tests/planet_reference.csv", "r" );
    if ( file == nullptr ) {
        printf( "FAIL no reference file\n" );
        return 1;
    }
    char line[160];
    int checks = 0;
    std::fgets( line, sizeof( line ), file );
    while ( std::fgets( line, sizeof( line ), file ) != nullptr ) {
        long long seconds = 0;
        char name[16] = {};
        char quantity[32] = {};
        double value = 0;
        if ( std::sscanf( line, "%lld,%15[^,],%31[^,],%lf", &seconds, name, quantity, &value ) != 4 ) {
            continue;
        }
        const int body = bodyOf( name );
        if ( body < 0 ) {
            continue;
        }
        const Planets::Body b = static_cast<Planets::Body>( body );
        ++checks;
        const Planets::Heliocentric helio = Planets::heliocentric( b, seconds );
        if ( std::strcmp( quantity, "helio_longitude" ) == 0 ) {
            s_angle[body] = std::fmax( s_angle[body], std::fabs( angleDifference( helio.longitude, value ) ) * 60 );
        } else if ( std::strcmp( quantity, "helio_latitude" ) == 0 ) {
            s_angle[body] = std::fmax( s_angle[body], std::fabs( helio.latitude - value ) * 60 );
        } else if ( std::strcmp( quantity, "helio_distance" ) == 0 ) {
            s_distance[body] = std::fmax( s_distance[body], std::fabs( helio.distance - value ) );
        } else if ( std::strcmp( quantity, "ra" ) == 0 || std::strcmp( quantity, "dec" ) == 0 ||
                    std::strcmp( quantity, "distance" ) == 0 ) {
            const Planets::Geocentric geo = Planets::geocentric( b, seconds );
            if ( quantity[0] == 'r' ) {
                const double error = angleDifference( geo.raHours * 15, value * 15 ) * std::cos( geo.decDegrees * M_PI / 180 );
                s_angle[body] = std::fmax( s_angle[body], std::fabs( error ) * 60 );
            } else if ( quantity[0] == 'd' && quantity[1] == 'e' ) {
                s_angle[body] = std::fmax( s_angle[body], std::fabs( geo.decDegrees - value ) * 60 );
            } else {
                s_distance[body] = std::fmax( s_distance[body], std::fabs( geo.distance - value ) );
            }
        } else {
            const bool istanbul = std::strncmp( quantity, "istanbul", 8 ) == 0;
            const Astronomy::Observer observer = istanbul ? Astronomy::Observer { 41.01, 28.98 }
                                                          : Astronomy::Observer { -33.87, 151.21 };
            const Astronomy::Position position = Planets::position( b, seconds, observer );
            double error = 0;
            if ( std::strstr( quantity, "altitude" ) != nullptr ) {
                error = position.altitude - value;
            } else {
                error = angleDifference( position.azimuth, value ) * std::cos( position.altitude * M_PI / 180 );
            }
            s_sky[body] = std::fmax( s_sky[body], std::fabs( error ) );
        }
    }
    std::fclose( file );

    // Mercury, Venus and the Earth within an arcminute. Mars 2': seen from the Earth near
    // opposition (0.4 AU) the two bodies' errors of a few tenths of an arcminute grow by the ratio
    // of the distances. Jupiter and Saturn 2': JPL's elements alone were 5' and 9' off, Schlyter's
    // great-inequality terms bring them to 0.9' and 1.7'. Saturn's distance 0.02 AU: JPL's elements
    // keep it to 0.018 AU, the second decimal of about 9.6 AU on the distance part, 10 s of light.
    // Uranus and Neptune 2' and 0.01 AU: JPL's elements alone give 1.8' and 0.8'.
    const double angleLimits[Planets::BodyCount] = { 1, 1, 1, 2, 2, 2, 2, 2 };
    const double distanceLimits[Planets::BodyCount] = { 0.002, 0.002, 0.002, 0.002, 0.01, 0.02, 0.01, 0.01 };
    int failures = 0;
    for ( int i = 0; i < Planets::BodyCount; ++i ) {
        const double angleLimit = angleLimits[i];
        const double distanceLimit = distanceLimits[i];
        const double skyLimit = 0.05;
        const bool ok = s_angle[i] <= angleLimit && s_distance[i] <= distanceLimit && s_sky[i] <= skyLimit;
        printf( "%s %-8s angle %5.2f' (limit %.0f')  distance %.4f AU (limit %.3f)  sky %.3f deg (limit %.2f)\n",
                ok ? "    " : "FAIL", s_names[i], s_angle[i], angleLimit, s_distance[i], distanceLimit, s_sky[i], skyLimit );
        failures += ok ? 0 : 1;
    }
    // Rise and set times (tests/planet_risings.csv, tools/make_planet_risings.py): within 2 min
    // at middle and low latitudes; a day without a rise or a set must say so.
    FILE *risings = std::fopen( argc > 2 ? argv[2] : "tests/planet_risings.csv", "r" );
    if ( risings == nullptr ) {
        printf( "FAIL no rise and set reference\n" );
        return 1;
    }
    double worst[2] = {};  // middle and low latitudes, high latitudes (Tromso, Reykjavik)
    std::fgets( line, sizeof( line ), risings );
    while ( std::fgets( line, sizeof( line ), risings ) != nullptr ) {
        char place[16] = {};
        double latitude = 0;
        double longitude = 0;
        long long midnight = 0;
        char name[16] = {};
        char event[8] = {};
        char value[24] = {};
        if ( std::sscanf( line, "%15[^,],%lf,%lf,%lld,%15[^,],%7[^,],%23s", place, &latitude, &longitude, &midnight, name, event,
                          value ) != 7 ) {
            continue;
        }
        const int body = bodyOf( name );
        if ( body < 0 ) {
            continue;
        }
        ++checks;
        const Astronomy::Crossings crossings =
            Planets::riseSet( static_cast<Planets::Body>( body ), midnight, Astronomy::Observer { latitude, longitude } );
        const Astronomy::Event &found = event[0] == 'r' ? crossings.rising : crossings.setting;
        const bool none = std::strcmp( value, "none" ) == 0;
        if ( none != !found.occurs ) {
            ++failures;
            printf( "FAIL %s %s %s %lld: expected %s, got %s\n", place, name, event, midnight, value,
                    found.occurs ? "a time" : "none" );
            continue;
        }
        if ( none ) {
            continue;
        }
        const bool high = std::fabs( latitude ) > 60;
        worst[high ? 1 : 0] = std::fmax( worst[high ? 1 : 0], std::fabs( static_cast<double>( found.time - std::atoll( value ) ) ) );
    }
    std::fclose( risings );
    const double limits[2] = { 120, 120 };
    for ( int i = 0; i < 2; ++i ) {
        const bool ok = worst[i] <= limits[i];
        printf( "%s rise and set, %s latitudes: largest error %.0f s (limit %.0f s)\n", ok ? "    " : "FAIL",
                i == 0 ? "middle and low" : "high", worst[i], limits[i] );
        failures += ok ? 0 : 1;
    }

    printf( "Planets: %d checks, %d failures\n", checks, failures );
    return failures == 0 ? 0 : 1;
}
