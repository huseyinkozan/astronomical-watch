#include "Astrolabe.h"

#include <cmath>

static const double s_pi = 3.14159265358979323846;

static double toRadians( double degrees )
{
    return degrees * s_pi / 180.0;
}

static double toDegrees( double radians )
{
    return radians * 180.0 / s_pi;
}

// -180 to 180.
static double signedAngle( double degrees )
{
    double value = std::fmod( degrees, 360.0 );
    if ( value > 180.0 ) {
        value -= 360.0;
    } else if ( value <= -180.0 ) {
        value += 360.0;
    }
    return value;
}

const Astrolabe::Star Astrolabe::Stars[StarCount] = {
    { "Sirius", 6.7525, -16.716, -1.46 },     { "Canopus", 6.3992, -52.696, -0.74 },
    { "Rigil Kentaurus", 14.6601, -60.834, -0.27 }, { "Arcturus", 14.2610, 19.182, -0.05 },
    { "Vega", 18.6156, 38.784, 0.03 },        { "Capella", 5.2782, 45.998, 0.08 },
    { "Rigel", 5.2423, -8.202, 0.13 },        { "Procyon", 7.6550, 5.225, 0.34 },
    { "Achernar", 1.6286, -57.237, 0.46 },    { "Betelgeuse", 5.9195, 7.407, 0.50 },
    { "Altair", 19.8464, 8.868, 0.76 },       { "Acrux", 12.4433, -63.099, 0.77 },
    { "Aldebaran", 4.5987, 16.509, 0.86 },    { "Antares", 16.4901, -26.432, 0.96 },
    { "Spica", 13.4199, -11.161, 0.97 },      { "Pollux", 7.7553, 28.026, 1.14 },
    { "Fomalhaut", 22.9608, -29.622, 1.16 },  { "Deneb", 20.6905, 45.280, 1.25 },
    { "Regulus", 10.1395, 11.967, 1.35 },     { "Polaris", 2.5303, 89.264, 1.98 },
};

double Astrolabe::plateLatitude( double latitude )
{
    const double value = std::fabs( latitude );
    return value < 1.0 ? 1.0 : value;
}

// South of the equator the declinations change sign with the latitude.
static double plateDeclination( double declination, double latitude )
{
    return latitude < 0.0 ? -declination : declination;
}

double Astrolabe::declinationRadius( double declination, double latitude )
{
    return std::tan( toRadians( 90.0 - plateDeclination( declination, latitude ) ) / 2.0 );
}

Astrolabe::Point Astrolabe::project( double hourAngle, double declination, double latitude )
{
    const double r = declinationRadius( declination, latitude );
    const double h = toRadians( hourAngle );
    return { r * std::sin( h ), r * std::cos( h ) };
}

double Astrolabe::limitRadius()
{
    return std::tan( toRadians( 90.0 + PlateObliquity ) / 2.0 );
}

Astrolabe::Circle Astrolabe::almucantar( double altitude, double latitude )
{
    const double phi = plateLatitude( latitude );
    // Where it crosses the meridian: towards the equator (declination phi + altitude - 90) and on
    // the pole's side (polar distance altitude - phi, past the pole when negative).
    const double towardsEquator = std::tan( toRadians( 180.0 - phi - altitude ) / 2.0 );
    const double poleSide = std::tan( toRadians( altitude - phi ) / 2.0 );
    return { 0.0, ( towardsEquator + poleSide ) / 2.0, ( towardsEquator - poleSide ) / 2.0 };
}

Astrolabe::Circle Astrolabe::ecliptic( double siderealTime, double obliquity, double latitude )
{
    // The solstices lie on one line through the pole, at the ends of a diameter of the circle.
    const Point june = project( siderealTime - 90.0, obliquity, latitude );
    const Point december = project( siderealTime - 270.0, -obliquity, latitude );
    return { ( june.x + december.x ) / 2.0, ( june.y + december.y ) / 2.0,
             std::hypot( june.x - december.x, june.y - december.y ) / 2.0 };
}

bool Astrolabe::arcInside( const Circle &circle, double limit, double &start, double &end )
{
    const double d = std::hypot( circle.x, circle.y );
    const double r = circle.radius;
    start = 0.0;
    end = 360.0;
    if ( d <= 0.0 || r <= 0.0 ) {
        return r <= limit;
    }
    // |centre + r u(t)|^2 <= limit^2  <=>  cos(t - direction) <= k.
    const double k = ( limit * limit - d * d - r * r ) / ( 2.0 * r * d );
    if ( k >= 1.0 ) {
        return true;
    }
    if ( k <= -1.0 ) {
        return false;
    }
    const double direction = toDegrees( std::atan2( circle.y, circle.x ) );
    const double half = toDegrees( std::acos( k ) );
    start = direction + half;
    end = direction + 360.0 - half;
    return true;
}

double Astrolabe::hourAngle( double rightAscension, int64_t time, double longitude )
{
    return signedAngle( Astronomy::greenwichSiderealTime( time ) + longitude - rightAscension );
}

// The morning or afternoon time of `altitude` on the UTC day from `utcMidnight`, with the Sun's
// declination and equation of time taken at `at`: local apparent noon is 12:00 local mean time
// minus the equation of time, the Sun's hour angle at the altitude away from it.
static bool sunTime( double altitude, int64_t utcMidnight, bool afternoon, int64_t at, const Astronomy::Observer &observer,
                     int64_t &time )
{
    double declination = 0;
    double equation = 0;
    Astronomy::sunDeclinationAndEquation( at, declination, equation );
    const double phi = toRadians( observer.latitude );
    const double delta = toRadians( declination );
    const double denominator = std::cos( phi ) * std::cos( delta );
    if ( std::fabs( denominator ) < 1e-9 ) {
        return false;
    }
    const double cosH = ( std::sin( toRadians( altitude ) ) - std::sin( phi ) * std::sin( delta ) ) / denominator;
    if ( cosH > 1.0 || cosH < -1.0 ) {
        return false;
    }
    const double hourAngleMinutes = toDegrees( std::acos( cosH ) ) * 4.0;
    const double minutes = 720.0 - equation - 4.0 * observer.longitude + ( afternoon ? hourAngleMinutes : -hourAngleMinutes );
    time = utcMidnight + static_cast<int64_t>( std::lround( minutes * 60.0 ) );
    return true;
}

bool Astrolabe::timeFromSunAltitude( double altitude, int64_t now, const Astronomy::Observer &observer, int64_t &time )
{
    const int64_t utcMidnight = now - ( ( now % 86400 ) + 86400 ) % 86400;
    bool found = false;
    int64_t best = 0;
    int64_t bestDistance = 0;
    for ( int day = -1; day <= 1; ++day ) {
        const int64_t midnight = utcMidnight + day * 86400;
        for ( int branch = 0; branch < 2; ++branch ) {
            // First with the Sun of the day's middle, then again with the Sun of that moment.
            int64_t candidate = 0;
            if ( !sunTime( altitude, midnight, branch == 1, midnight + 43200, observer, candidate ) ||
                 !sunTime( altitude, midnight, branch == 1, candidate, observer, candidate ) ) {
                continue;
            }
            const int64_t distance = candidate > now ? candidate - now : now - candidate;
            if ( !found || distance < bestDistance ) {
                best = candidate;
                bestDistance = distance;
                found = true;
            }
        }
    }
    time = best;
    return found;
}

bool Astrolabe::latitudeFromPolaris( double altitude, int64_t time, double longitude, double &latitude )
{
    double rightAscension = 0;
    double declination = 0;
    Astronomy::precess( Stars[Polaris].raHours, Stars[Polaris].decDegrees, time, rightAscension, declination );
    const double h = toRadians( hourAngle( rightAscension, time, longitude ) );
    const double delta = toRadians( declination );
    // sin(altitude) = a sin(phi) + b cos(phi) = r sin(phi + psi).
    const double a = std::sin( delta );
    const double b = std::cos( delta ) * std::cos( h );
    const double r = std::hypot( a, b );
    const double s = std::sin( toRadians( altitude ) ) / r;
    if ( s > 1.0 || s < -1.0 ) {
        return false;
    }
    const double psi = toDegrees( std::atan2( b, a ) );
    latitude = toDegrees( std::asin( s ) ) - psi;
    return latitude >= -90.0 && latitude <= 90.0;
}

double Astrolabe::sundialAngle( double latitude, double hourAngle )
{
    const double h = toRadians( hourAngle );
    return toDegrees( std::atan2( std::sin( toRadians( std::fabs( latitude ) ) ) * std::sin( h ), std::cos( h ) ) );
}
