#pragma once

// The Tools page's unit converter: angle and time (an hour angle or right ascension in h:m:s,
// degrees, radians; 24 h = 360°), astronomical distances and temperatures. Plain C++ for the
// host tests.
class Units
{
public:
    enum Category
    {
        AngleTime,
        Distance,
        Temperature,
        CategoryCount
    };
    enum AngleUnit
    {
        Hours,
        Degrees,
        Radians
    };
    enum DistanceUnit
    {
        Kilometres,
        AstronomicalUnits,
        LightYears,
        Parsecs
    };
    enum TemperatureUnit
    {
        Celsius,
        Fahrenheit,
        Kelvin
    };
    static const int MaxUnits = 4;

    static int unitCount( int category );
    // `value` in unit `from` of `category`, in unit `to`.
    static double convert( int category, int from, int to, double value );
    // False for a temperature below absolute zero.
    static bool valid( int category, int unit, double value );

    // "[-]h[:m[:s]]" in whole numbers, minutes and seconds below 60, to hours.
    static bool parseHms( const char *text, double &hours );
    // Hours to whole hours, minutes and seconds (rounded to the second); false from a million
    // hours on (no room on the row).
    static bool splitHms( double hours, bool &negative, int &h, int &m, int &s );
};
