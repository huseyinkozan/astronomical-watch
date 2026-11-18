#pragma once

#include <cstdint>

#include "AodGlyphs.h"
#include "Astronomy.h"
#include "Corners.h"
#include "FaceConfig.h"
#include "FaceTable.h"
#include "Rtc.h"
#include "sdkconfig.h"

// Always-on face (CLAUDE.md, always-on mode): the face page shown when active mode ended, drawn
// with one bit per colour: its clock and the slots marked "also on always-on" (FaceConfig), with
// the corner gauges (Corners), at the day or night brightness. A timer wake once a minute redraws
// the face; between wakes the ESP32 deep-sleeps and the panel shows the image from its own
// memory. The state is kept in RTC memory.
class AlwaysOn
{
public:
    // What the face shows, taken when active mode ends (the minute wakes read no settings and do
    // not compute the sky day): the face page, the slots' events and names (a wake past their end
    // computes the events again), the place for the altitudes and the events, the Moon's
    // illuminated fraction in percent and whether it is waxing, what the corner gauges need, and
    // the panel brightness by day and by night in percent.
    struct Look
    {
        FaceConfig::Page page;
        FaceTable table;
        bool skyValid;  // a location is set: the sky texts can be shown
        Astronomy::Observer observer;
        int ishaAngle;
        int moonPercent;
        bool waxing;
        Corners::Input corners;
        int dayBrightness;
        int nightBrightness;
        int language;  // Language::Id: the minute wakes read no settings
    };

    enum Colour
    {
        White,
        Yellow,
        Red,
        Green,
        Blue,
        // Idle mode has no grey: white on every second pixel, which looks grey on this panel.
        Grey
    };

    // One bitmap of the face, its top-left corner at (x, y) on the screen before the burn-in
    // shift.
    struct Sprite
    {
        int16_t x;
        int16_t y;
        const AodGlyph *glyph;
        Colour colour;
    };

    // Four text slots of up to 20 characters, the clock, the corners.
    static const int MaxSprites = 104;

    // A drawn shape, on the screen before the burn-in shift: a 2 x 2 dot at (x0, y0), a disc or a
    // ring (`width` wide) of `radius` around it, or a line to (x1, y1) `width` wide.
    enum ShapeKind : uint8_t
    {
        DotShape,
        DiscShape,
        RingShape,
        LineShape
    };

    struct Shape
    {
        uint8_t kind;
        uint8_t colour;
        uint8_t radius;
        uint8_t width;
        int16_t x0;
        int16_t y0;
        int16_t x1;
        int16_t y1;
    };

    static const int MaxShapes = 16;

    // The sprites and shapes, and the box on the screen that holds them before the burn-in shift.
    struct Face
    {
        Sprite sprites[MaxSprites];
        int count;
        Shape shapes[MaxShapes];
        int shapeCount;
        int left;
        int top;
        int width;
        int height;
    };

    // Places the face's bitmaps and shapes for the local time `time` (`utcNow` the same moment as
    // Unix time; `batteryPercent` < 0: unknown). The face is kept in a static buffer until the
    // next call (it is too large for the main task's stack).
    static const Face &layout( const Look &look, const Rtc::DateTime &time, int64_t utcNow, int batteryPercent );

    // The idle-mode colour of a corner gauge, or -1 when it draws nothing.
    static int gaugeColour( Corners::Kind kind );

    // The colour of pixel (x, y) of the screen's corner gauges as the always-on face draws them,
    // in the panel's byte order; 0 where nothing is lit.
    static uint16_t cornerPixel( const Corners::Gauge gauges[Corners::CornerCount], int x, int y );

    // Called when active mode ends; the following timer wakes draw the face until the
    // motion check powers the watch off (main.cpp) or a PWR wake ends it.
    void begin( const Look &look );

    // A PWR wake returns to active mode and ends the always-on face.
    void end();

    bool isActive() const;

    // Minutes drawn since begin(), for the event log.
    int minutes() const;

    // Timer wake: draws the face for `time` (local; `utcNow` the same moment as Unix time) and
    // returns the microseconds until just after the next minute, or 0 when the face cannot go on
    // (the panel failed, or no valid time for s_maxFailedMinutes). Without a valid time it draws
    // nothing and asks for another wake in a minute. `batteryPercent` < 0: unknown; `charging`
    // draws the battery gauge's part above its value white.
    uint64_t update( const Rtc::DateTime &time, int64_t utcNow, bool timeRead, int batteryPercent, bool charging );

#if CONFIG_WATCH_AOD_CYCLE_MIN > 0
    // Battery run V: 0 when the face is shown `seconds` after power-on, -1 for the blank face.
    static int cycleStyle( uint32_t seconds );
#endif

#if CONFIG_WATCH_DEV_LOGGING
    // Development screenshot (tools/screenshot.py): `look` (face page `page`) at 19:19 on Sunday
    // 21 June, with the corner gauges of that moment in Istanbul and the battery at 92 %, written
    // to the console instead of drawn. A fixed scene keeps the published picture unchanged.
    static void capture( const Look &look, int page );
#endif
};
