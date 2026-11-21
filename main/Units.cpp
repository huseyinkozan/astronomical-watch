#include "Units.h"

#include <cmath>

static const double s_pi = 3.14159265358979323846;
// IAU 2012 (exact); the Julian year at the speed of light (exact); 648000/π AU.
static const double s_auKm = 149597870.7;
static const double s_lightYearKm = 9460730472580.8;
static const double s_parsecKm = 648000.0 / s_pi * s_auKm;

// Each unit in the category's base unit (degrees, km); temperatures are converted through Kelvin.
static const double s_angleDegrees[3] = { 15.0, 1.0, 180.0 / s_pi };
static const double s_distanceKm[4] = { 1.0, s_auKm, s_lightYearKm, s_parsecKm };

static double toKelvin( int unit, double value )
{
    if ( unit == Units::Celsius ) {
        return value + 273.15;
    }
    if ( unit == Units::Fahrenheit ) {
        return ( value - 32.0 ) * 5.0 / 9.0 + 273.15;
    }
    return value;
}

static double fromKelvin( int unit, double kelvin )
{
    if ( unit == Units::Celsius ) {
        return kelvin - 273.15;
    }
    if ( unit == Units::Fahrenheit ) {
        return ( kelvin - 273.15 ) * 9.0 / 5.0 + 32.0;
    }
    return kelvin;
}

int Units::unitCount( int category )
{
    return category == Distance ? 4 : 3;
}

double Units::convert( int category, int from, int to, double value )
{
    const int count = unitCount( category );
    if ( from < 0 || from >= count || to < 0 || to >= count || from == to ) {
        return value;
    }
    if ( category == Temperature ) {
        return fromKelvin( to, toKelvin( from, value ) );
    }
    const double *base = category == Distance ? s_distanceKm : s_angleDegrees;
    return value * base[from] / base[to];
}

bool Units::valid( int category, int unit, double value )
{
    // A small margin: -459.67 °F is 0 K, but not exactly in binary.
    return category != Temperature || toKelvin( unit, value ) >= -1e-9;
}

bool Units::parseHms( const char *text, double &hours )
{
    const char *p = text;
    const bool negative = *p == '-';
    if ( negative ) {
        ++p;
    }
    long fields[3] = {};
    int count = 0;
    for ( ;; ) {
        if ( *p < '0' || *p > '9' || count == 3 ) {
            return false;  // an empty field, another sign, or a fourth field
        }
        long field = 0;
        while ( *p >= '0' && *p <= '9' ) {
            field = field * 10 + ( *p - '0' );
            if ( field > 9999999 ) {
                return false;
            }
            ++p;
        }
        fields[count++] = field;
        if ( *p == '\0' ) {
            break;
        }
        if ( *p != ':' ) {
            return false;
        }
        ++p;
    }
    if ( fields[1] > 59 || fields[2] > 59 ) {
        return false;
    }
    const double value = fields[0] + fields[1] / 60.0 + fields[2] / 3600.0;
    hours = negative ? -value : value;
    return true;
}

bool Units::splitHms( double hours, bool &negative, int &h, int &m, int &s )
{
    if ( !( fabs( hours ) < 1e6 ) ) {
        return false;
    }
    negative = hours < 0;
    const long long seconds = llround( fabs( hours ) * 3600.0 );
    h = static_cast<int>( seconds / 3600 );
    m = static_cast<int>( seconds / 60 % 60 );
    s = static_cast<int>( seconds % 60 );
    // A value that rounds to 0 s has no sign.
    negative = negative && seconds > 0;
    return true;
}
