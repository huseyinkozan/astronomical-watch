#pragma once

#include <cstdint>

#include "FaceConfig.h"

// What a short tap on the active face opens (Display opens it as the launcher does, so it becomes
// the last opened part). The zones, first match wins: the Sun and the Moon on the arc templates,
// the corner icons, the corner gauges (an L along both edges of each corner), the text slots, the
// ring between the arcs, the clock. Screen coordinates (410 x 502); a mirrored picture has its
// touch points mirrored to match, so the same coordinates hold. Plain C++ for the host tests.
class FaceTap
{
public:
    enum Target
    {
        NoTarget,
        SunPart,        // the information page's parts
        MoonPart,
        PrayerPart,
        BatteryPart,
        TimePart,
        SkyPart,        // the planets page's sky
        WorldPart,      // the clock page's parts
        GregorianPart,
        HijriPart,
        DaysPart,
        AlarmsPart,
        StopwatchPart,
        CountdownPart,
        SettingsPart,   // the settings page's first part (the display timeout)
        SetupPart,      // the settings page's setup portal part
        EmergencyPart   // the tools page's emergency card
    };

    struct Input
    {
        const FaceConfig::Page *page;
        const uint8_t *gauges;  // FaceConfig::Pages::gauges, by corner
        bool sunShown;          // the Sun drawn on the arcs, at (sunX, sunY)
        int sunX;
        int sunY;
        bool moonShown;
        int moonX;
        int moonY;
        bool stopwatchRunning;
        bool countdownRunning;
    };

    static Target at( const Input &input, int x, int y );

    // The part each kind of item opens.
    static Target gaugeTarget( int gauge );
    static Target cornerTarget( int item, bool stopwatchRunning, bool countdownRunning );
    static Target textTarget( int item );
};
