// Host test of main/Astrolabe.cpp (the observing page's astrolabe): the projection's geometry by
// its properties, the alidade's answers and the stars against Skyfield
// (tests/astrolabe_reference.csv, tools/make_astrolabe_reference.py).
// Build and run: sh tests/run_host_tests.sh

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Astrolabe.h"

static int failures = 0;
static int checks = 0;
static const double s_pi = 3.14159265358979323846;

static void near( const char *name, double value, double expected, double tolerance )
{
    ++checks;
    if ( !( std::fabs( value - expected ) <= tolerance ) ) {
        printf( "FAIL %s: %.6f, expected %.6f\n", name, value, expected );
        ++failures;
    }
}

static double toRadians( double degrees )
{
    return degrees * s_pi / 180.0;
}

static double toDegrees( double radians )
{
    return radians * 180.0 / s_pi;
}

// The hour angle and declination of a point of altitude `alt` and azimuth `az` (from north
// through east) at latitude `lat`.
static void toEquatorial( double alt, double az, double lat, double &hourAngle, double &declination )
{
    const double a = toRadians( alt );
    const double z = toRadians( az );
    const double phi = toRadians( lat );
    const double sinDec = std::sin( phi ) * std::sin( a ) + std::cos( phi ) * std::cos( a ) * std::cos( z );
    declination = toDegrees( std::asin( sinDec ) );
    // West positive: an east azimuth gives a negative hour angle.
    const double y = -std::sin( z ) * std::cos( a );
    const double x = std::cos( phi ) * std::sin( a ) - std::sin( phi ) * std::cos( a ) * std::cos( z );
    hourAngle = toDegrees( std::atan2( y, x ) );
}

static double distance( const Astrolabe::Point &p, double x, double y )
{
    return std::hypot( p.x - x, p.y - y );
}

static void geometry()
{
    // The equator has radius 1, the near tropic tan(33.28 degrees), the limit the far tropic.
    near( "equator", Astrolabe::declinationRadius( 0, 41 ), 1, 1e-12 );
    near( "near tropic", Astrolabe::declinationRadius( 23.44, 41 ), std::tan( toRadians( 33.28 ) ), 1e-12 );
    near( "limit", Astrolabe::limitRadius(), std::tan( toRadians( 56.72 ) ), 1e-12 );
    near( "south: tropics swap", Astrolabe::declinationRadius( -23.44, -34 ), Astrolabe::declinationRadius( 23.44, 41 ),
          1e-12 );

    // Orientation: the meridian upwards, east on the left, the zenith above the pole.
    const Astrolabe::Point zenith = Astrolabe::project( 0, 41, 41 );
    near( "zenith x", zenith.x, 0, 1e-12 );
    near( "zenith above the pole", zenith.y > 0 ? 1 : 0, 1, 0 );
    double h = 0;
    double d = 0;
    toEquatorial( 0, 90, 41, h, d );
    near( "east point: hour angle -90", h, -90, 1e-9 );
    near( "east point on the left", Astrolabe::project( h, d, 41 ).x, -1, 1e-9 );
    // Time runs clockwise: an hour later a star stands further right at the top.
    near( "clockwise", Astrolabe::project( 15, 10, 41 ).x > Astrolabe::project( 0, 10, 41 ).x ? 1 : 0, 1, 0 );

    // Almucantars: every point of the altitude lies on the circle, at several latitudes.
    const double latitudes[5] = { 41, 69.65, 5, -33.87, 0.2 };
    const double altitudes[4] = { 0, 15, 45, 75 };
    for ( int i = 0; i < 5; ++i ) {
        const double lat = Astrolabe::plateLatitude( latitudes[i] ) * ( latitudes[i] < 0 ? -1 : 1 );
        for ( int j = 0; j < 4; ++j ) {
            const Astrolabe::Circle circle = Astrolabe::almucantar( altitudes[j], latitudes[i] );
            double worst = 0;
            for ( int az = 0; az < 360; az += 15 ) {
                toEquatorial( altitudes[j], az, lat, h, d );
                const Astrolabe::Point p = Astrolabe::project( h, d, latitudes[i] );
                if ( std::hypot( p.x, p.y ) > 50 ) {
                    continue;  // near the projection's pole: rounding only
                }
                const double error = std::fabs( distance( p, circle.x, circle.y ) - circle.radius );
                worst = error > worst ? error : worst;
            }
            char name[64];
            snprintf( name, sizeof( name ), "almucantar %.0f at %.2f", altitudes[j], latitudes[i] );
            near( name, worst, 0, 1e-9 );
        }
    }

    // The ecliptic: every longitude lies on the circle, for any sidereal time.
    const double epsilon = 23.4366;
    for ( int i = 0; i < 4; ++i ) {
        const double lat = i < 2 ? 41 : -33.87;
        const double sidereal = 37.0 + 100.0 * i;
        const Astrolabe::Circle circle = Astrolabe::ecliptic( sidereal, epsilon, lat );
        double worst = 0;
        for ( int lambda = 0; lambda < 360; lambda += 10 ) {
            const double l = toRadians( lambda );
            const double e = toRadians( epsilon );
            const double ra = toDegrees( std::atan2( std::sin( l ) * std::cos( e ), std::cos( l ) ) );
            const double dec = toDegrees( std::asin( std::sin( e ) * std::sin( l ) ) );
            const Astrolabe::Point p = Astrolabe::project( sidereal - ra, dec, lat );
            const double error = std::fabs( distance( p, circle.x, circle.y ) - circle.radius );
            worst = error > worst ? error : worst;
        }
        char name[48];
        snprintf( name, sizeof( name ), "ecliptic %d", i );
        near( name, worst, 0, 1e-9 );
        // It touches both tropics, so it stays inside the limit.
        near( "ecliptic inside", std::hypot( circle.x, circle.y ) + circle.radius, Astrolabe::limitRadius(), 0.01 );
    }

    // The part of a circle inside the limit: its ends on the limit, its middle inside, the rest out.
    const double limit = Astrolabe::limitRadius();
    for ( int i = 0; i < 3; ++i ) {
        const double lat = i == 0 ? 41 : ( i == 1 ? 69.65 : 5 );
        const Astrolabe::Circle horizon = Astrolabe::almucantar( 0, lat );
        double start = 0;
        double end = 0;
        near( "horizon partly inside", Astrolabe::arcInside( horizon, limit, start, end ) ? 1 : 0, 1, 0 );
        near( "arc ordered", end > start ? 1 : 0, 1, 0 );
        if ( end - start >= 360 ) {
            // Far north the whole horizon fits inside the limit.
            near( "whole horizon inside", std::hypot( horizon.x, horizon.y ) + horizon.radius < limit ? 1 : 0, 1, 0 );
            continue;
        }
        const double angles[3] = { start, ( start + end ) / 2, end };
        const double expected[3] = { limit, -1, limit };
        for ( int k = 0; k < 3; ++k ) {
            const double t = toRadians( angles[k] );
            const double r = std::hypot( horizon.x + horizon.radius * std::cos( t ), horizon.y + horizon.radius * std::sin( t ) );
            if ( expected[k] > 0 ) {
                near( "arc end on the limit", r, limit, 1e-9 );
            } else {
                near( "arc middle inside", r < limit ? 1 : 0, 1, 0 );
            }
        }
        const double outside = toRadians( start - 5 );
        near( "before the arc outside",
              std::hypot( horizon.x + horizon.radius * std::cos( outside ), horizon.y + horizon.radius * std::sin( outside ) ) > limit
                  ? 1
                  : 0,
              1, 0 );
    }
    double start = 0;
    double end = 0;
    near( "small circle wholly inside", Astrolabe::arcInside( { 0.2, 0.1, 0.3 }, limit, start, end ) ? 1 : 0, 1, 0 );
    near( "wholly: 360", end - start, 360, 1e-12 );
    near( "far circle outside", Astrolabe::arcInside( { 5, 0, 0.5 }, limit, start, end ) ? 1 : 0, 0, 0 );

    // Sundial hour lines: tan(angle) = sin(latitude) tan(hour angle).
    near( "sundial at the pole", Astrolabe::sundialAngle( 90, 37 ), 37, 1e-9 );
    near( "sundial 1 pm at 40", Astrolabe::sundialAngle( 40, 15 ), toDegrees( std::atan( std::sin( toRadians( 40 ) ) * std::tan( toRadians( 15 ) ) ) ),
          1e-9 );
    near( "sundial 6 pm", Astrolabe::sundialAngle( 41, 90 ), 90, 1e-9 );
    near( "sundial 6 am", Astrolabe::sundialAngle( 41, -90 ), -90, 1e-9 );
    near( "sundial 7 pm beyond", Astrolabe::sundialAngle( 41, 105 ) > 90 ? 1 : 0, 1, 0 );
    near( "sundial south: same size", Astrolabe::sundialAngle( -40, 15 ), Astrolabe::sundialAngle( 40, 15 ), 1e-12 );
    near( "sundial noon", Astrolabe::sundialAngle( 41, 0 ), 0, 1e-12 );
}

// The reference: stars against Astronomy::starPosition, the Sun's altitude back to its time,
// Polaris' altitude back to the latitude.
static void reference( const char *path )
{
    FILE *file = fopen( path, "r" );
    if ( file == nullptr ) {
        printf( "FAIL cannot open %s\n", path );
        ++failures;
        return;
    }
    char line[256];
    double worstStar = 0;
    int64_t worstSun = 0;
    double worstPolaris = 0;
    int stars = 0;
    int suns = 0;
    int polaris = 0;
    while ( fgets( line, sizeof( line ), file ) != nullptr ) {
        if ( line[0] == '#' ) {
            continue;
        }
        int index = 0;
        double lat = 0;
        double lon = 0;
        long long time = 0;
        double alt = 0;
        double az = 0;
        if ( sscanf( line, "star,%d,%lf,%lf,%lld,%lf,%lf", &index, &lat, &lon, &time, &alt, &az ) == 6 ) {
            const Astrolabe::Star &star = Astrolabe::Stars[index];
            const Astronomy::Position p = Astronomy::starPosition( star.raHours, star.decDegrees, time, { lat, lon } );
            // The angle between the two directions.
            const double cosAngle = std::sin( toRadians( alt ) ) * std::sin( toRadians( p.altitude ) ) +
                                    std::cos( toRadians( alt ) ) * std::cos( toRadians( p.altitude ) ) *
                                        std::cos( toRadians( az - p.azimuth ) );
            const double angle = toDegrees( std::acos( cosAngle > 1 ? 1 : cosAngle ) );
            worstStar = angle > worstStar ? angle : worstStar;
            ++stars;
        } else if ( sscanf( line, "sun,%lf,%lf,%lld,%lf", &lat, &lon, &time, &alt ) == 4 ) {
            // Measured 20 min before or after: the nearer of the day's two times is the right one.
            for ( int side = -1; side <= 1; side += 2 ) {
                int64_t found = 0;
                if ( !Astrolabe::timeFromSunAltitude( alt, time + side * 1200, { lat, lon }, found ) ) {
                    printf( "FAIL sun %.2f %.2f %lld: no time\n", lat, lon, time );
                    ++failures;
                    continue;
                }
                const int64_t error = std::llabs( found - time );
                worstSun = error > worstSun ? error : worstSun;
                ++suns;
            }
        } else if ( sscanf( line, "polaris,%lf,%lf,%lld,%lf", &lat, &lon, &time, &alt ) == 4 ) {
            double found = 0;
            if ( !Astrolabe::latitudeFromPolaris( alt, time, lon, found ) ) {
                printf( "FAIL polaris %.2f: no latitude\n", lat );
                ++failures;
                continue;
            }
            const double error = std::fabs( found - lat );
            worstPolaris = error > worstPolaris ? error : worstPolaris;
            ++polaris;
        }
    }
    fclose( file );
    // Stars: nutation and aberration (Skyfield's, left out here) are up to about 0.01 degree.
    near( "stars against Skyfield", worstStar, 0, 0.02 );
    // Sun: its position is good to about 0.01 degree; where it climbs slowly (Tromso in winter)
    // that is a few seconds of time. A minute is far below what a sighting by hand can tell.
    near( "Sun's time against Skyfield (s)", static_cast<double>( worstSun ), 0, 60 );
    // Polaris: the same 0.01 degree.
    near( "latitude from Polaris against Skyfield", worstPolaris, 0, 0.02 );
    near( "reference rows", stars > 0 && suns > 0 && polaris > 0 ? 1 : 0, 1, 0 );
    printf( "     stars: %d, largest error %.4f deg; Sun: %d, largest error %lld s; Polaris: %d, largest error %.4f deg\n",
            stars, worstStar, suns, static_cast<long long>( worstSun ), polaris, worstPolaris );
}

int main( int argc, char **argv )
{
    geometry();
    reference( argc > 1 ? argv[1] : "tests/astrolabe_reference.csv" );
    // Out of reach: above the noon altitude no time, a negative Polaris altitude past the pole.
    int64_t time = 0;
    near( "Sun above its noon altitude", Astrolabe::timeFromSunAltitude( 80, 1782043200, { 41.01, 28.98 }, time ) ? 1 : 0, 0,
          0 );
    // 2026-06-21 at Istanbul: 72.5 degrees at noon; 60 degrees in the morning and the afternoon.
    near( "morning by the morning", Astrolabe::timeFromSunAltitude( 60, 1782021600, { 41.01, 28.98 }, time ) ? 1 : 0, 1, 0 );
    near( "morning: before noon", time < 1782036000 ? 1 : 0, 1, 0 );
    near( "afternoon by the afternoon", Astrolabe::timeFromSunAltitude( 60, 1782050400, { 41.01, 28.98 }, time ) ? 1 : 0, 1, 0 );
    near( "afternoon: after noon", time > 1782036000 ? 1 : 0, 1, 0 );
    printf( "Astrolabe: %d checks, %d failures\n", checks, failures );
    return failures == 0 ? 0 : 1;
}
