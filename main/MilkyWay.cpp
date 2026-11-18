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

bool MilkyWay::drawn( const Band &band, float x, float y, float observerX, float observerY, float clip )
{
    const float dx = x - observerX;
    const float dy = y - observerY;
    const float h = band.hand * static_cast<float>( M_PI ) / 180.0f;
    return dy < 0 && dx * dx + dy * dy < clip * clip && dx * std::cos( h ) + dy * std::sin( h ) > 0;
}
