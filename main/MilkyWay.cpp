#include "MilkyWay.h"

#include <cmath>

MilkyWay::Band MilkyWay::band( double centreAltitude, double centreAzimuth, double sunAltitude, bool south,
                               float observerX, float observerY )
{
    Band band = {};
    band.radius = Radius;
    band.shown = centreAltitude > 0;
    band.bright = band.shown && sunAltitude <= -18;
    // Screen angles clockwise from the right (y downwards): left 180, up 270, right 360. The band
    // leaves the observer towards the centre's direction and bends upwards, its centre a quarter
    // turn aside: clockwise on the left half, the other way on the right half.
    double hand = std::fmod( ( south ? 270 : 90 ) + centreAzimuth, 360.0 );
    if ( hand > 0 && hand < 180 ) {
        hand = 360 - hand;
    }
    band.hand = static_cast<float>( hand == 0 ? 360 : hand );
    const bool rightHalf = hand > 270 || hand < 90;
    const double screen = rightHalf ? hand - 90 : hand + 90;
    const double a = screen * M_PI / 180.0;
    band.cx = observerX + static_cast<float>( Radius * std::cos( a ) );
    band.cy = observerY + static_cast<float>( Radius * std::sin( a ) );
    return band;
}

double MilkyWay::hourAngle( int64_t time, double longitude )
{
    // J2000 right ascension: the precession since moves the band by well under a degree.
    return std::fmod( Astronomy::greenwichSiderealTime( time ) + longitude - CentreRaHours * 15 + 720, 360.0 );
}

float MilkyWay::dialAngle( double hourAngle )
{
    return static_cast<float>( std::fmod( 270 + hourAngle, 360.0 ) );
}

MilkyWay::DialBand MilkyWay::dialBand( double centreAltitude, double sunAltitude, float dialAngle, float dialX, float dialY )
{
    DialBand band = {};
    band.radius = DialRadius;
    band.angle = dialAngle;
    band.centreUp = centreAltitude > 0;
    const double strength = ( -12 - sunAltitude ) / 6;
    band.strength = static_cast<float>( strength < 0 ? 0 : ( strength > 1 ? 1 : strength ) );
    band.shown = band.strength > 0;
    const double a = ( dialAngle + 90 ) * M_PI / 180.0;
    band.cx = dialX + static_cast<float>( DialRadius * std::cos( a ) );
    band.cy = dialY + static_cast<float>( DialRadius * std::sin( a ) );
    return band;
}

float MilkyWay::dialFade( const DialBand &band, float x, float y, float dialX, float dialY )
{
    if ( !band.shown ) {
        return 0;
    }
    const float ring = std::hypot( x - band.cx, y - band.cy ) - band.radius;
    const float distance = std::hypot( x - dialX, y - dialY );
    // Inside the Sun's orbit (r 150), clear of its dots.
    if ( ring > DialWidth / 2.0f || ring < -DialWidth / 2.0f || distance > 147 ) {
        return 0;
    }
    float fade = ( distance - 50 ) / 60;
    fade = fade < 0 ? 0 : ( fade > 1 ? 1 : fade );
    const float a = band.angle * static_cast<float>( M_PI ) / 180.0f;
    const bool ahead = ( x - dialX ) * std::cos( a ) + ( y - dialY ) * std::sin( a ) > 0;
    return fade * ( ahead && band.centreUp ? 1.0f : 0.35f ) * band.strength;
}

bool MilkyWay::drawn( const Band &band, float x, float y, float observerX, float observerY, float clip )
{
    const float dx = x - observerX;
    const float dy = y - observerY;
    const float h = band.hand * static_cast<float>( M_PI ) / 180.0f;
    return dy < 0 && dx * dx + dy * dy < clip * clip && dx * std::cos( h ) + dy * std::sin( h ) > 0;
}
