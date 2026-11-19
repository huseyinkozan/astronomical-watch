#include "Planets.h"

#include <cmath>

namespace {

const double s_pi = 3.14159265358979323846;
const double s_degree = s_pi / 180.0;
// The J2000 obliquity of the ecliptic (Standish's elements refer to the J2000 ecliptic).
const double s_obliquity = 23.43928 * s_degree;
// The light time of one AU in days.
const double s_lightDaysPerAu = 499.004784 / 86400.0;

// Semi-major axis (AU), eccentricity, inclination, mean longitude, longitude of perihelion and
// of the ascending node (degrees), each with its rate per Julian century from J2000.
struct Elements
{
    double a[2];
    double e[2];
    double i[2];
    double l[2];
    double perihelion[2];
    double node[2];
};

// JPL Table 1 (1800-2050), in the order of Planets::Body; the Earth's line is the Earth-Moon
// barycentre's.
const Elements s_elements[Planets::BodyCount] = {
    { { 0.38709927, 0.00000037 }, { 0.20563593, 0.00001906 }, { 7.00497902, -0.00594749 },
      { 252.25032350, 149472.67411175 }, { 77.45779628, 0.16047689 }, { 48.33076593, -0.12534081 } },
    { { 0.72333566, 0.00000390 }, { 0.00677672, -0.00004107 }, { 3.39467605, -0.00078890 },
      { 181.97909950, 58517.81538729 }, { 131.60246718, 0.00268329 }, { 76.67984255, -0.27769418 } },
    { { 1.00000261, 0.00000562 }, { 0.01671123, -0.00004392 }, { -0.00001531, -0.01294668 },
      { 100.46457166, 35999.37244981 }, { 102.93768193, 0.32327364 }, { 0.0, 0.0 } },
    { { 1.52371034, 0.00001847 }, { 0.09339410, 0.00007882 }, { 1.84969142, -0.00813131 },
      { -4.55343205, 19140.30268499 }, { -23.94362959, 0.44441088 }, { 49.55953891, -0.29257343 } },
    { { 5.20288700, -0.00011607 }, { 0.04838624, -0.00013253 }, { 1.30439695, -0.00183714 },
      { 34.39644051, 3034.74612775 }, { 14.72847983, 0.21252668 }, { 100.47390909, 0.20469106 } },
    { { 9.53667594, -0.00125060 }, { 0.05386179, -0.00050991 }, { 2.48599187, 0.00193609 },
      { 49.95424423, 1222.49362201 }, { 92.59887831, -0.41897216 }, { 113.66242448, -0.28867794 } },
    { { 19.18916464, -0.00196176 }, { 0.04725744, -0.00004397 }, { 0.77263783, -0.00242939 },
      { 313.23810451, 428.48202785 }, { 170.95427630, 0.40805281 }, { 74.01692503, 0.04240589 } },
    { { 30.06992276, 0.00026291 }, { 0.00859048, 0.00005105 }, { 1.77004347, 0.00035372 },
      { -55.12002969, 218.45945325 }, { 44.96476227, -0.32241464 }, { 131.78422574, -0.00508664 } },
};

double centuries( double daysSinceJ2000 )
{
    return daysSinceJ2000 / 36525.0;
}

double daysSinceJ2000( int64_t time )
{
    // Unix time to Julian days from 2000-01-01 12:00; the 69 s of Terrestrial Time move Mercury
    // by 0.2" and are left out.
    return static_cast<double>( time ) / 86400.0 + 2440587.5 - 2451545.0;
}

double wrapped( double degrees )
{
    double value = std::fmod( degrees, 360.0 );
    if ( value > 180 ) {
        value -= 360;
    } else if ( value < -180 ) {
        value += 360;
    }
    return value;
}

double sinDeg( double degrees )
{
    return std::sin( degrees * s_degree );
}

double cosDeg( double degrees )
{
    return std::cos( degrees * s_degree );
}

// Kepler's equation by Newton's method (radians); a few steps reach 1e-12 for e below 0.21.
double eccentricAnomaly( double meanAnomaly, double e )
{
    double eccentric = meanAnomaly + e * std::sin( meanAnomaly );
    for ( int i = 0; i < 8; ++i ) {
        eccentric -= ( eccentric - e * std::sin( eccentric ) - meanAnomaly ) / ( 1 - e * std::cos( eccentric ) );
    }
    return eccentric;
}

// Jupiter and Saturn: JPL's Table 1 leaves them 5' and 9' off, since their mutual perturbation
// (mostly the "great inequality", about 900 years long) is not in Keplerian elements. These are
// P. Schlyter's elements ("How to compute planetary positions"; epoch 2000 Jan 0.0, ecliptic of
// the date) with his perturbation terms, precessed back to the J2000 ecliptic.
void outerPlanet( int body, double days, double xyz[3] )
{
    const double d = days + 1.5;  // from 2000 Jan 0.0
    const bool jupiter = body == Planets::Jupiter;
    const double node = jupiter ? 100.4542 + 2.76854e-5 * d : 113.6634 + 2.38980e-5 * d;
    const double inclination = jupiter ? 1.3030 - 1.557e-7 * d : 2.4886 - 1.081e-7 * d;
    const double argument = jupiter ? 273.8777 + 1.64505e-5 * d : 339.3939 + 2.97661e-5 * d;
    const double a = jupiter ? 5.20256 : 9.55475;
    const double e = jupiter ? 0.048498 + 4.469e-9 * d : 0.055546 - 9.499e-9 * d;
    const double mj = 19.8950 + 0.0830853001 * d;
    const double ms = 316.9670 + 0.0334442282 * d;
    const double eccentric = eccentricAnomaly( std::fmod( jupiter ? mj : ms, 360.0 ) * s_degree, e );
    const double xv = a * ( std::cos( eccentric ) - e );
    const double yv = a * std::sqrt( 1 - e * e ) * std::sin( eccentric );
    const double r = std::sqrt( xv * xv + yv * yv );
    const double v = std::atan2( yv, xv ) / s_degree;
    const double u = ( v + argument ) * s_degree;
    const double i = inclination * s_degree;
    double longitude = std::atan2( std::sin( u ) * std::cos( i ), std::cos( u ) ) / s_degree + node;
    double latitude = std::asin( std::sin( u ) * std::sin( i ) ) / s_degree;
    if ( jupiter ) {
        longitude += -0.332 * sinDeg( 2 * mj - 5 * ms - 67.6 ) - 0.056 * sinDeg( 2 * mj - 2 * ms + 21 ) +
                     0.042 * sinDeg( 3 * mj - 5 * ms + 21 ) - 0.036 * sinDeg( mj - 2 * ms ) + 0.022 * cosDeg( mj - ms ) +
                     0.023 * sinDeg( 2 * mj - 3 * ms + 52 ) - 0.016 * sinDeg( mj - 5 * ms - 69 );
    } else {
        longitude += 0.812 * sinDeg( 2 * mj - 5 * ms - 67.6 ) - 0.229 * cosDeg( 2 * mj - 4 * ms - 2 ) +
                     0.119 * sinDeg( mj - 2 * ms - 3 ) + 0.046 * sinDeg( 2 * mj - 6 * ms - 69 ) +
                     0.014 * sinDeg( mj - 3 * ms + 32 );
        latitude += -0.020 * cosDeg( 2 * mj - 4 * ms - 2 ) + 0.018 * sinDeg( 2 * mj - 6 * ms - 49 );
    }
    // From the equinox of the date back to J2000: the general precession in longitude.
    const double t = days / 36525.0;
    longitude -= 1.396971 * t + 0.0003086 * t * t;
    xyz[0] = r * cosDeg( latitude ) * cosDeg( longitude );
    xyz[1] = r * cosDeg( latitude ) * sinDeg( longitude );
    xyz[2] = r * sinDeg( latitude );
}

// Heliocentric J2000 ecliptic rectangular coordinates (AU) `days` after J2000 from JPL's
// elements.
void keplerian( int body, double days, double xyz[3] )
{
    const Elements &el = s_elements[body];
    const double t = centuries( days );
    const double a = el.a[0] + el.a[1] * t;
    const double e = el.e[0] + el.e[1] * t;
    const double inclination = ( el.i[0] + el.i[1] * t ) * s_degree;
    const double meanLongitude = el.l[0] + el.l[1] * t;
    const double perihelion = el.perihelion[0] + el.perihelion[1] * t;
    const double node = el.node[0] + el.node[1] * t;
    const double argument = ( perihelion - node ) * s_degree;
    const double meanAnomaly = wrapped( meanLongitude - perihelion ) * s_degree;
    const double eccentric = eccentricAnomaly( meanAnomaly, e );
    const double xp = a * ( std::cos( eccentric ) - e );
    const double yp = a * std::sqrt( 1 - e * e ) * std::sin( eccentric );
    const double cw = std::cos( argument );
    const double sw = std::sin( argument );
    const double cn = std::cos( node * s_degree );
    const double sn = std::sin( node * s_degree );
    const double ci = std::cos( inclination );
    const double si = std::sin( inclination );
    xyz[0] = ( cw * cn - sw * sn * ci ) * xp + ( -sw * cn - cw * sn * ci ) * yp;
    xyz[1] = ( cw * sn + sw * cn * ci ) * xp + ( -sw * sn + cw * cn * ci ) * yp;
    xyz[2] = sw * si * xp + cw * si * yp;
}

// Heliocentric J2000 ecliptic rectangular coordinates (AU) `days` after J2000. Jupiter and
// Saturn take their direction from outerPlanet() and their distance from JPL's elements, which
// keep it within 0.02 AU (Schlyter's have no term for it: Saturn 0.04 AU off).
void ecliptic( int body, double days, double xyz[3] )
{
    keplerian( body, days, xyz );
    if ( body != Planets::Jupiter && body != Planets::Saturn ) {
        return;
    }
    const double r = std::sqrt( xyz[0] * xyz[0] + xyz[1] * xyz[1] + xyz[2] * xyz[2] );
    double direction[3] = {};
    outerPlanet( body, days, direction );
    const double length = std::sqrt( direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2] );
    for ( int i = 0; i < 3; ++i ) {
        xyz[i] = direction[i] * r / length;
    }
}

}  // namespace

Planets::Heliocentric Planets::heliocentric( Body body, int64_t time )
{
    double xyz[3] = {};
    ecliptic( body, daysSinceJ2000( time ), xyz );
    Heliocentric result = {};
    result.distance = std::sqrt( xyz[0] * xyz[0] + xyz[1] * xyz[1] + xyz[2] * xyz[2] );
    result.longitude = std::atan2( xyz[1], xyz[0] ) / s_degree;
    if ( result.longitude < 0 ) {
        result.longitude += 360;
    }
    result.latitude = std::asin( xyz[2] / result.distance ) / s_degree;
    return result;
}

Planets::Geocentric Planets::geocentric( Body body, int64_t time )
{
    const double days = daysSinceJ2000( time );
    double earth[3] = {};
    ecliptic( Earth, days, earth );
    // The planet where the light we see left it: one step of the light time is enough.
    double planet[3] = {};
    ecliptic( body, days, planet );
    double d[3] = { planet[0] - earth[0], planet[1] - earth[1], planet[2] - earth[2] };
    const double first = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
    ecliptic( body, days - first * s_lightDaysPerAu, planet );
    for ( int i = 0; i < 3; ++i ) {
        d[i] = planet[i] - earth[i];
    }
    // Ecliptic to equatorial (J2000).
    const double x = d[0];
    const double y = std::cos( s_obliquity ) * d[1] - std::sin( s_obliquity ) * d[2];
    const double z = std::sin( s_obliquity ) * d[1] + std::cos( s_obliquity ) * d[2];
    Geocentric result = {};
    result.distance = std::sqrt( x * x + y * y + z * z );
    double ra = std::atan2( y, x ) / s_degree;
    if ( ra < 0 ) {
        ra += 360;
    }
    result.raHours = ra / 15;
    result.decDegrees = std::asin( z / result.distance ) / s_degree;
    return result;
}

Astronomy::Position Planets::position( Body body, int64_t time, const Astronomy::Observer &observer )
{
    const Geocentric geo = geocentric( body, time );
    return Astronomy::starPosition( geo.raHours, geo.decDegrees, time, observer );
}

// The planet's right ascension and declination at the start, the middle and the end of the day:
// the altitude search interpolates them (Meeus 15) instead of solving the orbits at each of its
// 160 steps, which took 1.7 s for the five planets on the watch. The search runs in one task at
// a time, so a file-scope copy is enough.
static int64_t s_dayStart = 0;
static double s_ra[3] = {};
static double s_dec[3] = {};

static double interpolated( const double values[3], int64_t time )
{
    const double n = static_cast<double>( time - s_dayStart ) / ( 12 * 3600.0 );
    const double a = values[1] - values[0];
    const double b = values[2] - values[1];
    return values[0] + n * a + n * ( n - 1 ) / 2 * ( b - a );
}

static double planetAltitude( int64_t time, const Astronomy::Observer &observer, int )
{
    double ra = std::fmod( interpolated( s_ra, time ), 24.0 );
    if ( ra < 0 ) {
        ra += 24;
    }
    return Astronomy::starPosition( ra, interpolated( s_dec, time ), time, observer ).altitude;
}

Astronomy::Crossings Planets::riseSet( Body body, int64_t localMidnight, const Astronomy::Observer &observer )
{
    s_dayStart = localMidnight;
    for ( int i = 0; i < 3; ++i ) {
        const Geocentric geo = geocentric( body, localMidnight + i * 12 * 3600 );
        s_ra[i] = geo.raHours;
        s_dec[i] = geo.decDegrees;
        // Continuous through 0 h.
        if ( i > 0 && s_ra[i] - s_ra[i - 1] > 12 ) {
            s_ra[i] -= 24;
        } else if ( i > 0 && s_ra[i] - s_ra[i - 1] < -12 ) {
            s_ra[i] += 24;
        }
    }
    return Astronomy::dayCrossings( planetAltitude, body, localMidnight, observer, -0.5667 );
}

Planets::Day Planets::day( int64_t localMidnight, const Astronomy::Observer &observer )
{
    Day result = {};
    for ( int i = 0; i < BodyCount; ++i ) {
        if ( i != Earth ) {
            result.riseSet[i] = riseSet( static_cast<Body>( i ), localMidnight, observer );
        }
    }
    return result;
}
