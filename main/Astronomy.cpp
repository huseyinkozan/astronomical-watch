#include "Astronomy.h"

#include <cmath>

static const double s_pi = 3.14159265358979323846;
static const double s_arcsecondsPerRadian = 206264.8062;
static const double s_earthRadiusKm = 6378.14;
static const double s_auKm = 149597870.7;
static const int64_t s_dayS = 86400;
// TT - UT, about 69 s in the 2020s; it moves the Moon by about 0.01 degree.
static const double s_deltaTS = 69.0;
static const double s_synodicMonthDays = 29.530588853;

// Sampling step of the altitude search; crossings closer than this can be missed, which does not
// happen for the Sun or the Moon away from the polar circles' edge cases.
static const int s_sampleStepS = 600;
static const int s_sampleCount = s_dayS / s_sampleStepS + 1;

static double toRadians( double degrees )
{
    return degrees * s_pi / 180.0;
}

static double toDegrees( double radians )
{
    return radians * 180.0 / s_pi;
}

// 0 to 360.
static double normalised( double degrees )
{
    const double value = std::fmod( degrees, 360.0 );
    return value < 0 ? value + 360.0 : value;
}

// -180 to 180.
static double signedAngle( double degrees )
{
    const double value = normalised( degrees );
    return value > 180.0 ? value - 360.0 : value;
}

static double fraction( double value )
{
    return value - std::floor( value );
}

static double julianDay( int64_t time )
{
    return static_cast<double>( time ) / s_dayS + 2440587.5;
}

// Julian centuries of Terrestrial Time since J2000.0.
static double centuriesTt( int64_t time )
{
    return ( julianDay( time ) + s_deltaTS / s_dayS - 2451545.0 ) / 36525.0;
}

static double obliquity( double t )
{
    const double omega = toRadians( 125.04 - 1934.136 * t );
    return 23.4392911 - 0.0130042 * t + 0.00256 * std::cos( omega );
}

// Greenwich mean sidereal time in degrees (Meeus 12.4).
static double siderealTime( int64_t time )
{
    const double days = julianDay( time ) - 2451545.0;
    const double t = days / 36525.0;
    return normalised( 280.46061837 + 360.98564736629 * days + 0.000387933 * t * t - t * t * t / 38710000.0 );
}

struct Ecliptic
{
    double longitude;
    double latitude;
    double distanceKm;
};

struct Equatorial
{
    double rightAscension;
    double declination;
    double distanceKm;
};

// Apparent geocentric Sun (Meeus chapter 25, low accuracy).
static Ecliptic sunEcliptic( double t )
{
    const double meanLongitude = 280.46646 + 36000.76983 * t + 0.0003032 * t * t;
    const double meanAnomaly = toRadians( 357.52911 + 35999.05029 * t - 0.0001537 * t * t );
    const double eccentricity = 0.016708634 - 0.000042037 * t;
    const double centre = ( 1.914602 - 0.004817 * t - 0.000014 * t * t ) * std::sin( meanAnomaly ) +
                          ( 0.019993 - 0.000101 * t ) * std::sin( 2 * meanAnomaly ) +
                          0.000289 * std::sin( 3 * meanAnomaly );
    const double trueAnomaly = meanAnomaly + toRadians( centre );
    const double omega = toRadians( 125.04 - 1934.136 * t );
    Ecliptic sun = {};
    sun.longitude = normalised( meanLongitude + centre - 0.00569 - 0.00478 * std::sin( omega ) );
    sun.latitude = 0.0;
    sun.distanceKm =
        s_auKm * 1.000001018 * ( 1 - eccentricity * eccentricity ) / ( 1 + eccentricity * std::cos( trueAnomaly ) );
    return sun;
}

// Geocentric Moon: longitude and latitude from Montenbruck and Pfleger's MiniMoon, distance from
// the main terms of Meeus table 47.A.
static Ecliptic moonEcliptic( double t )
{
    const double twoPi = 2 * s_pi;
    const double meanLongitude = fraction( 0.606433 + 1336.855225 * t );
    const double l = twoPi * fraction( 0.374897 + 1325.552410 * t );   // Moon's mean anomaly
    const double ls = twoPi * fraction( 0.993133 + 99.997361 * t );    // Sun's mean anomaly
    const double d = twoPi * fraction( 0.827361 + 1236.853086 * t );   // elongation of the Moon
    const double f = twoPi * fraction( 0.259086 + 1342.227825 * t );   // argument of latitude
    const double dL = 22640 * std::sin( l ) - 4586 * std::sin( l - 2 * d ) + 2370 * std::sin( 2 * d ) +
                      769 * std::sin( 2 * l ) - 668 * std::sin( ls ) - 412 * std::sin( 2 * f ) -
                      212 * std::sin( 2 * l - 2 * d ) - 206 * std::sin( l + ls - 2 * d ) + 192 * std::sin( l + 2 * d ) -
                      165 * std::sin( ls - 2 * d ) - 125 * std::sin( d ) - 110 * std::sin( l + ls ) +
                      148 * std::sin( l - ls ) - 55 * std::sin( 2 * f - 2 * d );
    const double s = f + ( dL + 412 * std::sin( 2 * f ) + 541 * std::sin( ls ) ) / s_arcsecondsPerRadian;
    const double h = f - 2 * d;
    const double n = -526 * std::sin( h ) + 44 * std::sin( l + h ) - 31 * std::sin( -l + h ) -
                     23 * std::sin( ls + h ) + 11 * std::sin( -ls + h ) - 25 * std::sin( -2 * l + f ) +
                     21 * std::sin( -l + f );

    // Distance: arguments in degrees after Meeus (47.2 to 47.5); coefficients in 0.001 km.
    const double dm = toRadians( 297.8501921 + 445267.1114034 * t );
    const double m = toRadians( 357.5291092 + 35999.0502909 * t );
    const double mm = toRadians( 134.9633964 + 477198.8675055 * t );
    const double fm = toRadians( 93.2720950 + 483202.0175233 * t );
    static const int terms[12][5] = {
        { 0, 0, 1, 0, -20905355 }, { 2, 0, -1, 0, -3699111 }, { 2, 0, 0, 0, -2955968 }, { 0, 0, 2, 0, -569925 },
        { 0, 1, 0, 0, 48888 },     { 0, 0, 0, 2, -3149 },     { 2, 0, -2, 0, 246158 },  { 2, -1, -1, 0, -152138 },
        { 2, 0, 1, 0, -170733 },   { 2, -1, 0, 0, -204586 },  { 0, 1, -1, 0, -129620 }, { 1, 0, 0, 0, 108743 },
    };
    double sumR = 0;
    for ( int i = 0; i < 12; ++i ) {
        sumR += terms[i][4] * std::cos( terms[i][0] * dm + terms[i][1] * m + terms[i][2] * mm + terms[i][3] * fm );
    }

    Ecliptic moon = {};
    moon.longitude = normalised( 360.0 * fraction( meanLongitude + dL / 1296.0e3 ) );
    moon.latitude = toDegrees( ( 18520.0 * std::sin( s ) + n ) / s_arcsecondsPerRadian );
    moon.distanceKm = 385000.56 + sumR / 1000.0;
    return moon;
}

static Equatorial toEquatorial( const Ecliptic &ecliptic, double t )
{
    const double epsilon = toRadians( obliquity( t ) );
    const double lambda = toRadians( ecliptic.longitude );
    const double beta = toRadians( ecliptic.latitude );
    Equatorial equatorial = {};
    equatorial.rightAscension = normalised( toDegrees( std::atan2(
        std::sin( lambda ) * std::cos( epsilon ) - std::tan( beta ) * std::sin( epsilon ), std::cos( lambda ) ) ) );
    equatorial.declination = toDegrees( std::asin( std::sin( beta ) * std::cos( epsilon ) +
                                                   std::cos( beta ) * std::sin( epsilon ) * std::sin( lambda ) ) );
    equatorial.distanceKm = ecliptic.distanceKm;
    return equatorial;
}

// Geocentric altitude and azimuth (azimuth from north through east).
static Astronomy::Position toHorizontal( const Equatorial &equatorial, int64_t time,
                                         const Astronomy::Observer &observer )
{
    const double hourAngle = toRadians( siderealTime( time ) + observer.longitude - equatorial.rightAscension );
    const double latitude = toRadians( observer.latitude );
    const double declination = toRadians( equatorial.declination );
    const double sinAltitude = std::sin( latitude ) * std::sin( declination ) +
                               std::cos( latitude ) * std::cos( declination ) * std::cos( hourAngle );
    const double east = -std::cos( declination ) * std::sin( hourAngle );
    const double north = std::sin( declination ) * std::cos( latitude ) -
                         std::cos( declination ) * std::cos( hourAngle ) * std::sin( latitude );
    Astronomy::Position position = {};
    position.altitude = toDegrees( std::asin( sinAltitude ) );
    position.azimuth = normalised( toDegrees( std::atan2( east, north ) ) );
    return position;
}

// Horizontal parallax of the Moon, degrees.
static double moonParallax( double distanceKm )
{
    return toDegrees( std::asin( s_earthRadiusKm / distanceKm ) );
}

Astronomy::Position Astronomy::sunPosition( int64_t time, const Observer &observer )
{
    const double t = centuriesTt( time );
    return toHorizontal( toEquatorial( sunEcliptic( t ), t ), time, observer );
}

Astronomy::Position Astronomy::moonPosition( int64_t time, const Observer &observer )
{
    const double t = centuriesTt( time );
    const Equatorial moon = toEquatorial( moonEcliptic( t ), t );
    Position position = toHorizontal( moon, time, observer );
    // Topocentric: the parallax lowers the Moon by up to about one degree near the horizon.
    position.altitude -= moonParallax( moon.distanceKm ) * std::cos( toRadians( position.altitude ) );
    return position;
}

// Altitude above the threshold of rise and set: the Sun's centre at -0.833 degrees (refraction
// and semi-diameter); the Moon's geocentric centre at 0.7275 parallax - 0.5667 degrees (Meeus 15).
typedef double ( *AltitudeFunction )( int64_t time, const Astronomy::Observer &observer );

static double sunAltitude( int64_t time, const Astronomy::Observer &observer )
{
    return Astronomy::sunPosition( time, observer ).altitude;
}

static double moonRiseAltitude( int64_t time, const Astronomy::Observer &observer )
{
    const double t = centuriesTt( time );
    const Equatorial moon = toEquatorial( moonEcliptic( t ), t );
    const double threshold = 0.7275 * moonParallax( moon.distanceKm ) - 0.5667;
    return toHorizontal( moon, time, observer ).altitude - threshold;
}

// First rising and setting through `threshold` between the samples, refined by bisection to one
// second. samples[i] is the altitude at start + i * s_sampleStepS.
static Astronomy::Crossings findCrossings( AltitudeFunction altitude, const double *samples, int64_t start,
                                           const Astronomy::Observer &observer, double threshold )
{
    Astronomy::Crossings crossings = {};
    bool anyAbove = false;
    bool anyBelow = false;
    for ( int i = 0; i < s_sampleCount; ++i ) {
        if ( samples[i] > threshold ) {
            anyAbove = true;
        } else {
            anyBelow = true;
        }
    }
    for ( int i = 0; i + 1 < s_sampleCount; ++i ) {
        const bool aboveBefore = samples[i] > threshold;
        const bool aboveAfter = samples[i + 1] > threshold;
        if ( aboveBefore == aboveAfter ) {
            continue;
        }
        Astronomy::Event &event = aboveAfter ? crossings.rising : crossings.setting;
        if ( event.occurs ) {
            continue;
        }
        int64_t low = start + static_cast<int64_t>( i ) * s_sampleStepS;
        int64_t high = low + s_sampleStepS;
        while ( high - low > 1 ) {
            const int64_t middle = ( low + high ) / 2;
            if ( ( altitude( middle, observer ) > threshold ) == aboveBefore ) {
                low = middle;
            } else {
                high = middle;
            }
        }
        event.occurs = true;
        event.time = high;
    }
    crossings.alwaysAbove = anyAbove && !anyBelow;
    crossings.alwaysBelow = anyBelow && !anyAbove;
    return crossings;
}

static void sampleDay( AltitudeFunction altitude, int64_t start, const Astronomy::Observer &observer,
                       double *samples )
{
    for ( int i = 0; i < s_sampleCount; ++i ) {
        samples[i] = altitude( start + static_cast<int64_t>( i ) * s_sampleStepS, observer );
    }
}

Astronomy::SunDay Astronomy::sunDay( int64_t localMidnight, const Observer &observer )
{
    double samples[s_sampleCount];
    sampleDay( sunAltitude, localMidnight, observer, samples );
    SunDay day = {};
    day.riseSet = findCrossings( sunAltitude, samples, localMidnight, observer, -0.833 );
    day.civil = findCrossings( sunAltitude, samples, localMidnight, observer, -6.0 );
    day.nautical = findCrossings( sunAltitude, samples, localMidnight, observer, -12.0 );
    day.astronomical = findCrossings( sunAltitude, samples, localMidnight, observer, -18.0 );
    day.goldenHour = findCrossings( sunAltitude, samples, localMidnight, observer, 6.0 );
    day.blueHour = findCrossings( sunAltitude, samples, localMidnight, observer, -4.0 );

    // Transit: the hour angle is zero. The Sun's hour angle grows by about 360 degrees a day.
    int64_t noon = localMidnight + s_dayS / 2;
    for ( int i = 0; i < 4; ++i ) {
        const double t = centuriesTt( noon );
        const Equatorial sun = toEquatorial( sunEcliptic( t ), t );
        const double hourAngle = signedAngle( siderealTime( noon ) + observer.longitude - sun.rightAscension );
        noon -= static_cast<int64_t>( std::lround( hourAngle / 360.0 * s_dayS ) );
    }
    day.noon.occurs = noon >= localMidnight && noon < localMidnight + s_dayS;
    day.noon.time = noon;
    day.noonAltitude = sunAltitude( noon, observer );

    const Crossings &sun = day.riseSet;
    const int64_t end = localMidnight + s_dayS;
    if ( sun.alwaysAbove ) {
        day.dayLengthSeconds = s_dayS;
    } else if ( sun.rising.occurs && sun.setting.occurs ) {
        const int64_t length = sun.setting.time - sun.rising.time;
        day.dayLengthSeconds = static_cast<int>( length > 0 ? length : s_dayS + length );
    } else if ( sun.rising.occurs ) {
        day.dayLengthSeconds = static_cast<int>( end - sun.rising.time );
    } else if ( sun.setting.occurs ) {
        day.dayLengthSeconds = static_cast<int>( sun.setting.time - localMidnight );
    }
    return day;
}

Astronomy::Crossings Astronomy::moonDay( int64_t localMidnight, const Observer &observer )
{
    double samples[s_sampleCount];
    sampleDay( moonRiseAltitude, localMidnight, observer, samples );
    return findCrossings( moonRiseAltitude, samples, localMidnight, observer, 0.0 );
}

bool Astronomy::moonsetWithin( int64_t start, int span, const Observer &observer, int64_t &moonset )
{
    if ( moonRiseAltitude( start, observer ) <= 0 ) {
        return false;
    }
    for ( int64_t low = start; low < start + span; low += s_sampleStepS ) {
        int64_t high = low + s_sampleStepS;
        if ( moonRiseAltitude( high, observer ) > 0 ) {
            continue;
        }
        while ( high - low > 1 ) {
            const int64_t middle = ( low + high ) / 2;
            if ( moonRiseAltitude( middle, observer ) > 0 ) {
                low = middle;
            } else {
                high = middle;
            }
        }
        moonset = high;
        return true;
    }
    return false;
}

// Moon minus Sun in ecliptic longitude, 0 to 360.
static double elongationAt( int64_t time )
{
    const double t = centuriesTt( time );
    return normalised( moonEcliptic( t ).longitude - sunEcliptic( t ).longitude );
}

Astronomy::MoonPhase Astronomy::moonPhase( int64_t time )
{
    const double t = centuriesTt( time );
    const Ecliptic moon = moonEcliptic( t );
    const Ecliptic sun = sunEcliptic( t );
    // Angle Sun - Earth - Moon, then the phase angle Sun - Moon - Earth (Meeus 48.2 and 48.3).
    const double cosPsi = std::cos( toRadians( moon.latitude ) ) * std::cos( toRadians( moon.longitude - sun.longitude ) );
    const double psi = std::acos( cosPsi );
    const double phaseAngle = std::atan2( sun.distanceKm * std::sin( psi ), moon.distanceKm - sun.distanceKm * cosPsi );
    MoonPhase phase = {};
    phase.illumination = ( 1 + std::cos( phaseAngle ) ) / 2;
    phase.elongation = normalised( moon.longitude - sun.longitude );
    phase.ageDays = static_cast<double>( time - previousNewMoon( time ) ) / s_dayS;
    return phase;
}

int64_t Astronomy::previousNewMoon( int64_t time )
{
    // The elongation grows by about 12.19 degrees a day.
    const double rate = 360.0 / s_synodicMonthDays / s_dayS;
    int64_t newMoon = time - static_cast<int64_t>( elongationAt( time ) / rate );
    for ( int i = 0; i < 5; ++i ) {
        newMoon -= static_cast<int64_t>( signedAngle( elongationAt( newMoon ) ) / rate );
    }
    if ( newMoon > time ) {
        newMoon = previousNewMoon( newMoon - s_dayS );
    }
    return newMoon;
}

double Astronomy::moonSunSeparation( int64_t time )
{
    const double t = centuriesTt( time );
    const Ecliptic moon = moonEcliptic( t );
    const Ecliptic sun = sunEcliptic( t );
    return toDegrees( std::acos( std::cos( toRadians( moon.latitude ) ) *
                                 std::cos( toRadians( moon.longitude - sun.longitude ) ) ) );
}

double Astronomy::moonSemiDiameter( int64_t time )
{
    // The Moon's radius is 0.2725 Earth radii.
    return 0.2725 * moonParallax( moonEcliptic( centuriesTt( time ) ).distanceKm );
}

bool Astronomy::sunsetNear( int64_t localNoon, const Observer &observer, int64_t &sunset )
{
    const double latitude = toRadians( observer.latitude );
    int64_t time = localNoon;
    for ( int i = 0; i < 3; ++i ) {
        const double t = centuriesTt( time );
        const Equatorial sun = toEquatorial( sunEcliptic( t ), t );
        const double declination = toRadians( sun.declination );
        const double cosHourAngle = ( std::sin( toRadians( -0.833 ) ) - std::sin( latitude ) * std::sin( declination ) ) /
                                    ( std::cos( latitude ) * std::cos( declination ) );
        if ( cosHourAngle < -1.0 || cosHourAngle > 1.0 ) {
            return false;
        }
        const double setHourAngle = toDegrees( std::acos( cosHourAngle ) );
        const double hourAngle = signedAngle( siderealTime( time ) + observer.longitude - sun.rightAscension );
        // The Sun's hour angle grows by about 360 degrees a day.
        time += static_cast<int64_t>( std::lround( ( setHourAngle - hourAngle ) / 360.0 * s_dayS ) );
    }
    sunset = time;
    return true;
}

int64_t Astronomy::nextSeason( int64_t time, int &season )
{
    // The Sun's longitude grows by about 0.9856 degrees a day.
    const double rate = 0.98564736 / s_dayS;
    const double longitude = sunEcliptic( centuriesTt( time ) ).longitude;
    season = ( static_cast<int>( longitude / 90.0 ) + 1 ) % 4;
    const double target = season * 90.0;
    int64_t when = time + static_cast<int64_t>( normalised( target - longitude ) / rate );
    for ( int i = 0; i < 4; ++i ) {
        when -= static_cast<int64_t>( signedAngle( sunEcliptic( centuriesTt( when ) ).longitude - target ) / rate );
    }
    return when;
}
