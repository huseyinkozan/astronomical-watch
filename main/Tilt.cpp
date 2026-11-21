#include "Tilt.h"

#include <cmath>

static const double s_pi = 3.14159265358979323846;
static const double s_weight = 0.25;
static const double s_bubbleRangeDegrees = 45.0;

static double toDegrees( double radians )
{
    return radians * 180.0 / s_pi;
}

Tilt::Vector Tilt::fromSensor( int xMg, int yMg, int zMg, int mirror )
{
    Vector v = {};
    v.right = yMg / 1000.0;
    v.up = xMg / 1000.0;
    v.out = -zMg / 1000.0;
    // The panel draws the picture mirrored; the directions the user sees turn with it.
    if ( ( mirror & 1 ) != 0 ) {
        v.right = -v.right;
    }
    if ( ( mirror & 2 ) != 0 ) {
        v.up = -v.up;
    }
    return v;
}

void Tilt::add( const Vector &sample )
{
    if ( !m_valid ) {
        m_value = sample;
        m_valid = true;
        return;
    }
    m_value.right += s_weight * ( sample.right - m_value.right );
    m_value.up += s_weight * ( sample.up - m_value.up );
    m_value.out += s_weight * ( sample.out - m_value.out );
}

void Tilt::reset()
{
    m_valid = false;
    m_value = { 0, 0, 0 };
}

bool Tilt::valid() const
{
    return m_valid;
}

Tilt::Vector Tilt::value() const
{
    return m_value;
}

double Tilt::pitch( const Vector &v )
{
    return toDegrees( std::atan2( v.up, std::hypot( v.right, v.out ) ) );
}

double Tilt::roll( const Vector &v )
{
    return toDegrees( std::atan2( v.right, std::hypot( v.up, v.out ) ) );
}

double Tilt::slope( const Vector &v )
{
    return toDegrees( std::atan2( std::hypot( v.right, v.up ), v.out ) );
}

double Tilt::edgeAltitude( const Vector &v )
{
    return std::fabs( roll( v ) );
}

bool Tilt::heightFromDistance( double distance, double altitude, double &height )
{
    if ( altitude <= 0.0 || altitude >= 90.0 ) {
        return false;
    }
    height = distance * std::tan( altitude * s_pi / 180.0 );
    return true;
}

bool Tilt::distanceFromHeight( double height, double altitude, double &distance )
{
    if ( altitude <= 0.0 || altitude >= 90.0 ) {
        return false;
    }
    distance = height / std::tan( altitude * s_pi / 180.0 );
    return true;
}

double Tilt::ringFraction( double slopeDegrees )
{
    if ( slopeDegrees <= 0.0 ) {
        return 0.0;
    }
    if ( slopeDegrees >= s_bubbleRangeDegrees ) {
        return 1.0;
    }
    return std::sqrt( slopeDegrees / s_bubbleRangeDegrees );
}

void Tilt::bubble( const Vector &v, double &right, double &up )
{
    const double horizontal = std::hypot( v.right, v.up );
    right = 0.0;
    up = 0.0;
    if ( horizontal <= 0.0 ) {
        return;
    }
    const double fraction = ringFraction( slope( v ) );
    right = fraction * v.right / horizontal;
    up = fraction * v.up / horizontal;
}
