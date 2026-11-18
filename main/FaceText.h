#pragma once

#include <cstdint>

#include "FaceConfig.h"

class FaceTable;

// The text of a face slot (FaceConfig::TextItem) or of a text corner (FaceConfig::CornerItem) in
// the current language; an empty text when its data is missing (no clock set, no location, the
// battery not read). The same texts serve the active face and the always-on face. Plain C++ for
// the host tests.
class FaceText
{
public:
    struct Inputs
    {
        bool timeValid;
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
        int weekday;  // 0 Monday, as Language counts
        bool skyValid;
        int64_t now;  // Unix time
        const FaceTable *table;
        int batteryPercent;  // -1: not read
        double sunAltitude;  // degrees
        double moonAltitude;
        bool altitudesValid;
    };

    // `parameter`: the city or day counter of a CityTime or DayCounter slot.
    static void text( FaceConfig::TextItem item, int parameter, const Inputs &in, char *out, int size );

    // Empty for the corners drawn as icons (MoonIcon, WeekdayIcon).
    static void corner( FaceConfig::CornerItem item, const Inputs &in, char *out, int size );

    // How many bytes of the UTF-8 `text` fit `maxWidth` pixels at `advance` pixels a character,
    // whole characters only (a slot text is cut, never wrapped).
    static int fit( const char *text, int advance, int maxWidth );
};
