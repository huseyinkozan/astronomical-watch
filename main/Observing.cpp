#include "Observing.h"

#include <cmath>

#include "MilkyWay.h"

static const double s_pi = 3.14159265358979323846;
// The Sun's hour angle grows by about this much a second; a star's by the sidereal rate.
static const double s_solarRate = 360.0 / 86400.0;
static const double s_siderealDay = 86164.0905;
static const double s_siderealRate = 360.0 / s_siderealDay;
static const double s_darkAltitude = -18.0;

const double Observing::PolarisRaHours = 2.0 + 31.0 / 60.0 + 49.09 / 3600.0;
const double Observing::PolarisDecDegrees = 89.0 + 15.0 / 60.0 + 50.8 / 3600.0;
const double Observing::SigmaOctantisRaHours = 21.0 + 8.0 / 60.0 + 46.86 / 3600.0;
const double Observing::SigmaOctantisDecDegrees = -( 88.0 + 57.0 / 60.0 + 23.4 / 3600.0 );

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

static double hourAngleOf( double rightAscension, int64_t time, double longitude )
{
    return signedAngle( Astronomy::greenwichSiderealTime( time ) + longitude - rightAscension );
}

// The cosine of the hour angle at which a point of `declination` stands `altitude` high; above 1
// it never gets that high, below -1 it never gets that low.
static double cosHourAngle( double altitude, double declination, double latitude )
{
    const double phi = toRadians( latitude );
    const double delta = toRadians( declination );
    const double denominator = std::cos( phi ) * std::cos( delta );
    if ( std::fabs( denominator ) < 1e-12 ) {
        return std::sin( toRadians( altitude ) ) > std::sin( phi ) * std::sin( delta ) ? 2.0 : -2.0;
    }
    return ( std::sin( toRadians( altitude ) ) - std::sin( phi ) * std::sin( delta ) ) / denominator;
}

double Observing::poleStarHourAngle( bool south, int64_t time, double longitude )
{
    double ra = 0;
    double dec = 0;
    Astronomy::precess( south ? SigmaOctantisRaHours : PolarisRaHours, south ? SigmaOctantisDecDegrees : PolarisDecDegrees,
                        time, ra, dec );
    const double hourAngle = hourAngleOf( ra, time, longitude );
    return hourAngle < 0 ? hourAngle + 360.0 : hourAngle;
}

int Observing::reticleMinutes( double hourAngle, bool south, bool inverted )
{
    // The clockwise angle from the top: a full turn of the hour angle is a full turn of the dial.
    double angle = south ? hourAngle : -hourAngle;
    if ( inverted ) {
        angle += 180.0;
    }
    angle = std::fmod( angle, 360.0 );
    if ( angle < 0 ) {
        angle += 360.0;
    }
    return static_cast<int>( std::lround( angle * 2.0 ) ) % 720;
}

// The time near `estimate` when the Sun's centre stands at the dark altitude on the side `side`
// of the meridian (+1 evening, -1 morning), its declination taken there; false when it does
// not get that low.
static bool sunEvent( int64_t estimate, int side, const Astronomy::Observer &observer, int64_t &time )
{
    int64_t t = estimate;
    for ( int i = 0; i < 4; ++i ) {
        double ra = 0;
        double dec = 0;
        Astronomy::sunEquatorial( t, ra, dec );
        const double c = cosHourAngle( s_darkAltitude, dec, observer.latitude );
        if ( c > 1.0 || c < -1.0 ) {
            return false;
        }
        const double target = side * toDegrees( std::acos( c ) );
        t += static_cast<int64_t>( std::llround( signedAngle( target - hourAngleOf( ra, t, observer.longitude ) ) / s_solarRate ) );
    }
    time = t;
    return true;
}

Observing::Night Observing::night( int64_t localNoon, const Astronomy::Observer &observer )
{
    Night result = {};
    double ra = 0;
    double dec = 0;
    Astronomy::sunEquatorial( localNoon, ra, dec );
    const int64_t transit =
        localNoon - static_cast<int64_t>( std::llround( hourAngleOf( ra, localNoon, observer.longitude ) / s_solarRate ) );
    const double c = cosHourAngle( s_darkAltitude, dec, observer.latitude );
    if ( c < -1.0 ) {
        return result;
    }
    if ( c > 1.0 ) {
        result.dark = true;
        result.dusk = transit;
        result.dawn = transit + 86400;
        return result;
    }
    const int64_t half = static_cast<int64_t>( std::llround( toDegrees( std::acos( c ) ) / s_solarRate ) );
    result.dark = sunEvent( transit + half, 1, observer, result.dusk ) &&
                  sunEvent( transit + 86400 - half, -1, observer, result.dawn ) && result.dawn > result.dusk;
    return result;
}

double Observing::altitude( double rightAscension, double declination, int64_t time, const Astronomy::Observer &observer )
{
    const double h = toRadians( hourAngleOf( rightAscension, time, observer.longitude ) );
    const double phi = toRadians( observer.latitude );
    const double delta = toRadians( declination );
    return toDegrees( std::asin( std::sin( phi ) * std::sin( delta ) + std::cos( phi ) * std::cos( delta ) * std::cos( h ) ) );
}

int64_t Observing::highestTime( double rightAscension, int64_t from, int64_t to, double longitude )
{
    const double start = hourAngleOf( rightAscension, from, longitude );
    // The next transit: the hour angle reaches 0 (or 360).
    const double toTransit = ( start <= 0 ? -start : 360.0 - start ) / s_siderealRate;
    if ( from + toTransit <= to ) {
        return from + static_cast<int64_t>( std::llround( toTransit ) );
    }
    const double end = signedAngle( start + ( to - from ) * s_siderealRate );
    return std::fabs( start ) <= std::fabs( end ) ? from : to;
}

bool Observing::centreRises( double latitude )
{
    return 90.0 - std::fabs( latitude - MilkyWay::CentreDecDegrees ) > CentreAltitude;
}

Observing::MilkyWayNight Observing::milkyWay( int64_t localNoon, const Astronomy::Observer &observer )
{
    MilkyWayNight result = {};
    result.night = night( localNoon, observer );
    const Night &n = result.night;
    const int64_t middle = n.dark ? ( n.dusk + n.dawn ) / 2 : localNoon + 43200;
    result.moonIllumination = Astronomy::moonPhase( middle ).illumination;
    if ( !n.dark ) {
        return result;
    }
    double ra = 0;
    double dec = 0;
    Astronomy::precess( MilkyWay::CentreRaHours, MilkyWay::CentreDecDegrees, middle, ra, dec );
    const double c = cosHourAngle( CentreAltitude, dec, observer.latitude );
    if ( c > 1.0 ) {
        return result;
    }
    // The longest time the centre is up in the dark: around one of its transits near the night.
    int64_t from = n.dusk;
    int64_t to = n.dawn;
    if ( c >= -1.0 ) {
        const double half = toDegrees( std::acos( c ) ) / s_siderealRate;
        const double transit = middle - hourAngleOf( ra, middle, observer.longitude ) / s_siderealRate;
        int64_t best = 0;
        for ( int k = -1; k <= 1; ++k ) {
            const double centre = transit + k * s_siderealDay;
            const int64_t rise = static_cast<int64_t>( std::llround( centre - half ) );
            const int64_t set = static_cast<int64_t>( std::llround( centre + half ) );
            const int64_t a = rise > n.dusk ? rise : n.dusk;
            const int64_t b = set < n.dawn ? set : n.dawn;
            if ( b - a > best ) {
                best = b - a;
                from = a;
                to = b;
            }
        }
        if ( best <= 0 ) {
            return result;
        }
    }
    result.centreUp = true;
    result.upStart = from;
    result.upEnd = to;
    result.start = from;
    result.end = to;
    if ( result.moonIllumination < FaintMoon ) {
        result.moon = MoonFaint;
    } else {
        // The longest run of samples with the Moon down; the first change tells how it moves.
        bool firstDown = false;
        bool changed = false;
        bool inRun = false;
        int64_t runStart = 0;
        int64_t bestLength = -1;
        for ( int64_t t = from;; t += MoonStep ) {
            const int64_t at = t < to ? t : to;
            const bool down = Astronomy::moonPosition( at, observer ).altitude < 0;
            if ( at == from ) {
                firstDown = down;
            } else if ( !changed && down != firstDown ) {
                changed = true;
                result.moonTime = at;
            }
            if ( down && !inRun ) {
                inRun = true;
                runStart = at;
            }
            if ( inRun && ( !down || at == to ) ) {
                const int64_t runEnd = down ? at : at - MoonStep;
                if ( runEnd - runStart > bestLength ) {
                    bestLength = runEnd - runStart;
                    result.start = runStart;
                    result.end = runEnd;
                }
                inRun = false;
            }
            if ( at == to ) {
                break;
            }
        }
        if ( !changed ) {
            result.moon = firstDown ? MoonDown : MoonUp;
        } else {
            result.moon = firstDown ? MoonRises : MoonSets;
        }
        if ( bestLength < 0 ) {
            result.start = result.end = from;
        }
    }
    result.window = result.end - result.start >= MinimumWindow;
    result.highest = highestTime( ra, result.start, result.end, observer.longitude );
    result.highestAltitude = altitude( ra, dec, result.highest, observer );
    return result;
}

bool Observing::nextMilkyWayNight( int64_t localNoon, int nights, const Astronomy::Observer &observer, int64_t &noon )
{
    for ( int i = 1; i <= nights; ++i ) {
        const int64_t candidate = localNoon + static_cast<int64_t>( i ) * 86400;
        if ( milkyWay( candidate, observer ).window ) {
            noon = candidate;
            return true;
        }
    }
    return false;
}
