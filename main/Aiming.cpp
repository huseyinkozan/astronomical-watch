#include "Aiming.h"

#include <cmath>

static const double s_pi = 3.14159265358979323846;
// WGS 84 and the geostationary orbit's radius, km.
static const double s_equatorRadius = 6378.137;
static const double s_flattening = 1 / 298.257223563;
static const double s_geostationaryRadius = 42164.17;
static const int s_stepS = 600;
static const int64_t s_dayS = 86400;

static double toRadians( double degrees )
{
    return degrees * s_pi / 180.0;
}

static double toDegrees( double radians )
{
    return radians * 180.0 / s_pi;
}

static double signedAngle( double degrees )
{
    double value = std::fmod( degrees, 360.0 );
    value = value < 0 ? value + 360.0 : value;
    return value > 180.0 ? value - 360.0 : value;
}

Astronomy::Position Aiming::dish( const Astronomy::Observer &observer, double satelliteLongitude )
{
    // The place and the satellite in Earth-fixed coordinates, then the line between them in the
    // place's east, north and up.
    const double phi = toRadians( observer.latitude );
    const double lambda = toRadians( observer.longitude );
    const double eccentricity2 = s_flattening * ( 2 - s_flattening );
    const double curvature = s_equatorRadius / std::sqrt( 1 - eccentricity2 * std::sin( phi ) * std::sin( phi ) );
    const double px = curvature * std::cos( phi ) * std::cos( lambda );
    const double py = curvature * std::cos( phi ) * std::sin( lambda );
    const double pz = curvature * ( 1 - eccentricity2 ) * std::sin( phi );
    const double satellite = toRadians( satelliteLongitude );
    const double dx = s_geostationaryRadius * std::cos( satellite ) - px;
    const double dy = s_geostationaryRadius * std::sin( satellite ) - py;
    const double dz = -pz;
    const double east = -std::sin( lambda ) * dx + std::cos( lambda ) * dy;
    const double north = -std::sin( phi ) * std::cos( lambda ) * dx - std::sin( phi ) * std::sin( lambda ) * dy +
                         std::cos( phi ) * dz;
    const double up = std::cos( phi ) * std::cos( lambda ) * dx + std::cos( phi ) * std::sin( lambda ) * dy +
                      std::sin( phi ) * dz;
    Astronomy::Position result = {};
    result.altitude = toDegrees( std::atan2( up, std::sqrt( east * east + north * north ) ) );
    result.azimuth = toDegrees( std::atan2( east, north ) );
    // atan2 gives -0 or a tiny negative due north: 0 then, not 360.
    result.azimuth = result.azimuth < 0 ? result.azimuth + 360.0 : result.azimuth;
    result.azimuth = result.azimuth >= 360.0 ? 0.0 : result.azimuth;
    return result;
}

bool Aiming::sunAtAzimuth( int64_t localMidnight, const Astronomy::Observer &observer, double azimuth, int64_t &time )
{
    Astronomy::Position previous = Astronomy::sunPosition( localMidnight, observer );
    for ( int64_t t = localMidnight + s_stepS; t <= localMidnight + s_dayS; t += s_stepS ) {
        const Astronomy::Position now = Astronomy::sunPosition( t, observer );
        const double before = signedAngle( previous.azimuth - azimuth );
        const double after = signedAngle( now.azimuth - azimuth );
        // A crossing of the direction, not of the opposite one (where the sign flips by 360).
        if ( ( before <= 0 ) != ( after <= 0 ) && std::fabs( before - after ) < 180.0 &&
             ( previous.altitude > 0 || now.altitude > 0 ) ) {
            int64_t low = t - s_stepS;
            int64_t high = t;
            while ( high - low > 1 ) {
                const int64_t middle = ( low + high ) / 2;
                const double value = signedAngle( Astronomy::sunPosition( middle, observer ).azimuth - azimuth );
                if ( ( value <= 0 ) == ( before <= 0 ) ) {
                    low = middle;
                } else {
                    high = middle;
                }
            }
            if ( Astronomy::sunPosition( high, observer ).altitude > 0 ) {
                time = high;
                return true;
            }
        }
        previous = now;
    }
    return false;
}

double Aiming::yearTilt( double latitude )
{
    return std::fabs( latitude );
}

double Aiming::dayTilt( double noonAltitude )
{
    return 90.0 - noonAltitude;
}

double Aiming::panelAzimuth( double latitude )
{
    return latitude >= 0 ? 180.0 : 0.0;
}
