// Host test of main/Astronomy.cpp against Skyfield reference values (tests/astronomy_reference.csv,
// made by tools/make_astronomy_reference.py). Build and run: sh tests/run_host_tests.sh
//
// Every quantity is compared within a tolerance that matches the accuracy the watch needs:
// Sun times within a minute, Moon times within two, angles within a few tenths of a degree.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "Astronomy.h"
#include "Hijri.h"

struct Tolerance
{
    const char *prefix;
    double limit;  // seconds for times, degrees for angles, the unit of the value otherwise
};

// The first matching prefix applies.
static const Tolerance s_tolerances[] = {
    { "sun_rise", 60 },          { "sun_set", 60 },           { "civil_", 60 },
    { "nautical_", 60 },         { "astronomical_", 60 },     { "golden_", 60 },
    { "blue_", 60 },             { "noon_altitude", 0.05 },   { "noon", 30 },
    { "moon_rise", 120 },        { "moon_set", 120 },         { "sun_altitude", 0.05 },
    { "sun_azimuth", 0.1 },      { "moon_altitude", 0.3 },    { "moon_azimuth", 0.5 },
    { "moon_illumination", 0.01 }, { "moon_elongation", 0.3 }, { "moon_age", 0.02 },
    { "next_season_index", 0 },  { "next_season", 600 },     { "crescent_v", 0.3 },
};

struct Result
{
    bool known;
    bool occurs;
    double value;
};

static Result eventResult( const Astronomy::Event &event )
{
    Result result = { true, event.occurs, static_cast<double>( event.time ) };
    return result;
}

static Result valueResult( double value )
{
    Result result = { true, true, value };
    return result;
}

static Result compute( const std::string &quantity, int64_t midnight, const Astronomy::Observer &observer )
{
    const Astronomy::SunDay day = Astronomy::sunDay( midnight, observer );
    const char *names[6] = { "sun", "civil", "nautical", "astronomical", "golden", "blue" };
    const Astronomy::Crossings *crossings[6] = { &day.riseSet,      &day.civil,      &day.nautical,
                                                 &day.astronomical, &day.goldenHour, &day.blueHour };
    for ( int i = 0; i < 6; ++i ) {
        if ( quantity == std::string( names[i] ) + "_rise" ) {
            return eventResult( crossings[i]->rising );
        }
        if ( quantity == std::string( names[i] ) + "_set" ) {
            return eventResult( crossings[i]->setting );
        }
    }
    if ( quantity == "noon" ) {
        return eventResult( day.noon );
    }
    if ( quantity == "noon_altitude" ) {
        return valueResult( day.noonAltitude );
    }
    if ( quantity == "moon_rise" || quantity == "moon_set" ) {
        const Astronomy::Crossings moon = Astronomy::moonDay( midnight, observer );
        return eventResult( quantity == "moon_rise" ? moon.rising : moon.setting );
    }
    for ( int hour = 9; hour <= 21; hour += 12 ) {
        const int64_t time = midnight + hour * 3600;
        const std::string suffix = "_" + std::to_string( hour );
        if ( quantity == "sun_altitude" + suffix ) {
            return valueResult( Astronomy::sunPosition( time, observer ).altitude );
        }
        if ( quantity == "sun_azimuth" + suffix ) {
            return valueResult( Astronomy::sunPosition( time, observer ).azimuth );
        }
        if ( quantity == "moon_altitude" + suffix ) {
            return valueResult( Astronomy::moonPosition( time, observer ).altitude );
        }
        if ( quantity == "moon_azimuth" + suffix ) {
            return valueResult( Astronomy::moonPosition( time, observer ).azimuth );
        }
    }
    const Astronomy::MoonPhase phase = Astronomy::moonPhase( midnight + 12 * 3600 );
    if ( quantity == "moon_illumination_12" ) {
        return valueResult( phase.illumination );
    }
    if ( quantity == "moon_elongation_12" ) {
        return valueResult( phase.elongation );
    }
    if ( quantity == "moon_age_12" ) {
        return valueResult( phase.ageDays );
    }
    if ( quantity == "crescent_v" ) {
        // Local day number and UTC offset from the local midnight (offsets within 12 hours).
        const int day = static_cast<int>( ( midnight + 43200 ) / 86400 );
        const int offsetMinutes = static_cast<int>( ( static_cast<int64_t>( day ) * 86400 - midnight ) / 60 );
        const Hijri::Crescent crescent = Hijri::crescent( day, offsetMinutes, observer );
        Result result = { true, crescent.zone != Hijri::ZoneNone, crescent.v };
        return result;
    }
    int season = 0;
    const int64_t next = Astronomy::nextSeason( midnight, season );
    if ( quantity == "next_season" ) {
        return valueResult( static_cast<double>( next ) );
    }
    if ( quantity == "next_season_index" ) {
        return valueResult( season );
    }
    Result unknown = { false, false, 0 };
    return unknown;
}

static double toleranceFor( const std::string &quantity )
{
    for ( size_t i = 0; i < sizeof( s_tolerances ) / sizeof( s_tolerances[0] ); ++i ) {
        if ( quantity.compare( 0, strlen( s_tolerances[i].prefix ), s_tolerances[i].prefix ) == 0 ) {
            return s_tolerances[i].limit;
        }
    }
    return 0;
}

int main( int argc, char **argv )
{
    const char *path = argc > 1 ? argv[1] : "tests/astronomy_reference.csv";
    FILE *file = fopen( path, "r" );
    if ( file == nullptr ) {
        printf( "Cannot open %s\n", path );
        return 1;
    }
    char line[256];
    int checked = 0;
    int failed = 0;
    // Largest error per quantity, to see how much margin each tolerance leaves.
    std::string worstName[64];
    double worstError[64] = {};
    int worstCount = 0;
    while ( fgets( line, sizeof( line ), file ) != nullptr ) {
        if ( line[0] == '#' || strncmp( line, "case,", 5 ) == 0 ) {
            continue;
        }
        char name[64];
        char quantity[64];
        char value[64];
        double latitude = 0;
        double longitude = 0;
        long long midnight = 0;
        if ( sscanf( line, "%63[^,],%lf,%lf,%lld,%63[^,],%63s", name, &latitude, &longitude, &midnight, quantity,
                     value ) != 6 ) {
            continue;
        }
        const Astronomy::Observer observer = { latitude, longitude };
        const Result result = compute( quantity, midnight, observer );
        if ( !result.known ) {
            printf( "SKIP %s %s: unknown quantity\n", name, quantity );
            continue;
        }
        ++checked;
        const bool expectNone = strcmp( value, "none" ) == 0;
        bool ok = false;
        double error = 0;
        if ( expectNone || !result.occurs ) {
            ok = expectNone && !result.occurs;
        } else {
            const double expected = atof( value );
            error = result.value - expected;
            if ( strstr( quantity, "azimuth" ) != nullptr || strstr( quantity, "elongation" ) != nullptr ) {
                error = std::fmod( error + 540.0, 360.0 ) - 180.0;
            }
            ok = std::fabs( error ) <= toleranceFor( quantity );
            int slot = 0;
            while ( slot < worstCount && worstName[slot] != quantity ) {
                ++slot;
            }
            if ( slot == worstCount && worstCount < 64 ) {
                worstName[worstCount++] = quantity;
            }
            if ( slot < 64 && std::fabs( error ) > std::fabs( worstError[slot] ) ) {
                worstError[slot] = error;
            }
        }
        if ( !ok ) {
            ++failed;
            printf( "FAIL %s %s: expected %s, got %s %.4f (error %.4f)\n", name, quantity, value,
                    result.occurs ? "" : "none", result.value, error );
        }
    }
    fclose( file );
    printf( "\nLargest error per quantity:\n" );
    for ( int i = 0; i < worstCount; ++i ) {
        printf( "  %-22s %10.4f (limit %g)\n", worstName[i].c_str(), worstError[i], toleranceFor( worstName[i] ) );
    }
    printf( "\n%d checked, %d failed\n", checked, failed );
    return failed == 0 ? 0 : 1;
}
