#include "Almanac.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Astronomy.h"
#include "Planets.h"

static const double s_pi = 3.14159265358979323846;
static const double s_auKm = 149597870.7;
static const double s_earthRadiusKm = 6378.14;
// The Sun's semi-diameter at 1 AU (IAU), arcseconds.
static const double s_sunSemiDiameter = 959.63;
static const int64_t s_dayS = 86400;

static double toRadians( double degrees )
{
    return degrees * s_pi / 180.0;
}

static double normalised( double degrees )
{
    const double value = std::fmod( degrees, 360.0 );
    return value < 0 ? value + 360.0 : value;
}

static double signedAngle( double degrees )
{
    const double value = normalised( degrees );
    return value > 180.0 ? value - 360.0 : value;
}

// The nutation in longitude, degrees (Meeus 22, the four main terms: within 0.5").
static double nutationInLongitude( int64_t time )
{
    const double t = ( time / 86400.0 + 2440587.5 - 2451545.0 ) / 36525.0;
    const double omega = toRadians( 125.04452 - 1934.136261 * t );
    const double sunLongitude = toRadians( 280.4665 + 36000.7698 * t );
    const double moonLongitude = toRadians( 218.3165 + 481267.8813 * t );
    return ( -17.20 * std::sin( omega ) - 1.32 * std::sin( 2 * sunLongitude ) - 0.23 * std::sin( 2 * moonLongitude ) +
             0.21 * std::sin( 2 * omega ) ) / 3600.0;
}

static const Planets::Body s_planets[Almanac::BodyCount] = { Planets::Earth, Planets::Earth, Planets::Venus,
                                                             Planets::Mars, Planets::Jupiter, Planets::Saturn };

Almanac::Place Almanac::place( Body body, int64_t time )
{
    Place result = {};
    double rightAscension = 0;
    double siderealTime = Astronomy::greenwichSiderealTime( time );
    if ( body == Sun ) {
        Astronomy::sunEquatorial( time, rightAscension, result.declination );
        // The Sun's longitude includes the nutation: the apparent sidereal time goes with it.
        siderealTime += nutationInLongitude( time ) * std::cos( toRadians( Astronomy::obliquityOfDate( time ) ) );
        result.semiDiameter = s_sunSemiDiameter / ( Astronomy::sunDistanceKm( time ) / s_auKm ) / 60.0;
    } else if ( body == Moon ) {
        double distanceKm = 0;
        Astronomy::moonEquatorial( time, rightAscension, result.declination, distanceKm );
        result.parallax = std::asin( s_earthRadiusKm / distanceKm ) * 180.0 / s_pi * 60.0;
        result.semiDiameter = Astronomy::moonSemiDiameter( time ) * 60.0;
    } else {
        const Planets::Geocentric planet = Planets::geocentric( s_planets[body], time );
        Astronomy::precess( planet.raHours, planet.decDegrees, time, rightAscension, result.declination );
    }
    result.gha = normalised( siderealTime - rightAscension );
    result.sha = normalised( 360.0 - rightAscension );
    return result;
}

bool Almanac::meridianPassage( Body body, int64_t localMidnight, double longitude, int64_t &time )
{
    // The hour angle grows by about 360 degrees a day, the Moon's by about 348.
    const double rate = body == Moon ? 347.8 : 360.0;
    const double hourAngle = normalised( place( body, localMidnight ).gha + longitude );
    int64_t when = localMidnight + static_cast<int64_t>( std::lround( ( 360.0 - hourAngle ) / rate * s_dayS ) );
    for ( int i = 0; i < 3; ++i ) {
        const double error = signedAngle( place( body, when ).gha + longitude );
        when -= static_cast<int64_t>( std::lround( error / rate * s_dayS ) );
    }
    time = when;
    return when >= localMidnight && when < localMidnight + s_dayS;
}

double Almanac::meridianAltitude( double latitude, double declination )
{
    return 90.0 - std::fabs( latitude - declination );
}

// Whole degrees and minutes to a tenth from tenths of an arcminute.
static void degreesMinutes( long tenths, char *text, size_t size )
{
    snprintf( text, size, "%ld° %02ld.%ld'", tenths / 600, tenths % 600 / 10, tenths % 10 );
}

void Almanac::formatAngle( double degrees, char *text, size_t size )
{
    degreesMinutes( std::lround( normalised( degrees ) * 600.0 ) % ( 360L * 600 ), text, size );
}

void Almanac::formatDeclination( double degrees, const char *north, const char *south, char *text, size_t size )
{
    const long tenths = std::lround( std::fabs( degrees ) * 600.0 );
    char angle[32];
    degreesMinutes( tenths, angle, sizeof( angle ) );
    // No letter for 0° 00.0'.
    snprintf( text, size, "%s%s%s", tenths == 0 ? "" : ( degrees > 0 ? north : south ), tenths == 0 ? "" : " ", angle );
}
