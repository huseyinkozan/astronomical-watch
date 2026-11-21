#include "Optics.h"

#include <cmath>

const Optics::Sensor Optics::Sensors[SensorCount] = {
    { "FF", 1.0, 5.9 },      // full frame, 24 MP
    { "APS-C", 1.5, 3.9 },   // 24 MP (Nikon, Sony, Fujifilm)
    { "APS-C", 1.6, 3.7 },   // Canon, 24 MP
    { "M4/3", 2.0, 3.3 },    // 20 MP
    { "1\"", 2.7, 2.4 },     // 20 MP
};

// The shutter speeds' denominators below a second, then whole seconds.
static const int s_shutters[Optics::ShutterCount] = { -8000, -4000, -2000, -1000, -500, -250, -125, -60, -30, -15,
                                                      -8,    -4,    -2,    1,     2,    4,    8,    15,  30 };

double Optics::magnification( double scopeFocal, double eyepieceFocal )
{
    return eyepieceFocal > 0 ? scopeFocal / eyepieceFocal : 0;
}

double Optics::exitPupil( double aperture, double magnification )
{
    return magnification > 0 ? aperture / magnification : 0;
}

double Optics::trueField( double apparentField, double magnification )
{
    return magnification > 0 ? apparentField / magnification : 0;
}

double Optics::limitingMagnitude( double aperture )
{
    return 2.7 + 5.0 * std::log10( aperture );
}

double Optics::dawesLimit( double aperture )
{
    return 116.0 / aperture;
}

double Optics::focalRatio( double scopeFocal, double aperture )
{
    return scopeFocal / aperture;
}

double Optics::rule500( double focal, double crop )
{
    return 500.0 / ( focal * crop );
}

double Optics::npfRule( double focal, double fNumber, double pixelPitch )
{
    return ( 35.0 * fNumber + 30.0 * pixelPitch ) / focal;
}

double Optics::ndExposure( double base, int stops )
{
    return base * std::ldexp( 1.0, stops );
}

int Optics::trailFrames( int minutes, int interval )
{
    return interval > 0 ? ( minutes * 60 + interval - 1 ) / interval : 0;
}

double Optics::trailArc( int minutes )
{
    return minutes * 60.0 * 360.0 / 86164.0905;
}

double Optics::shutterSeconds( int index )
{
    index = index < 0 ? 0 : ( index >= ShutterCount ? ShutterCount - 1 : index );
    return s_shutters[index] < 0 ? -1.0 / s_shutters[index] : s_shutters[index];
}
