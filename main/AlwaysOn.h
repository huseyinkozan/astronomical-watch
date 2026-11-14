#pragma once

#include <cstdint>

#include "AodGlyphs.h"
#include "Corners.h"
#include "Rtc.h"
#include "sdkconfig.h"

// Always-on face (CLAUDE.md, always-on mode): a dim HH:MM in one of the aodStyles with the date
// under it, the Moon's phase in the bottom-left corner and the corner gauges (Corners), at the day or
// night brightness. A timer wake once a minute redraws the face;
// between wakes the ESP32 deep-sleeps and the panel shows the image from its own memory. The
// state is kept in RTC memory. The always-on page of active mode (AodPage) shows the same layout.
class AlwaysOn
{
public:
    // Bits of Settings::AodItems.
    enum Item
    {
        MoonItem = 1,
        DateItem = 2
    };

    // What the face shows, taken when active mode ends (the minute wakes read no settings and
    // compute no astronomy): the style (an aodStyles index), the Item bits, the Moon's
    // illuminated fraction in percent and whether it is waxing, what the corner gauges need, and
    // the panel brightness by day and by night in percent.
    struct Look
    {
        int style;
        int items;
        int moonPercent;
        bool waxing;
        Corners::Input corners;
        int dayBrightness;
        int nightBrightness;
    };

    // Top-left corner of the moon on the screen: in the bottom-left corner, inside the Moon's
    // gauge, as on the watch faces.
    static const int MoonX = 48;
    static const int MoonY = 417;

    enum Colour
    {
        White,
        Yellow,
        Red,
        Green,
        Blue
    };

    // One bitmap of the face, `x` and `y` from the face's top-left corner: a glyph of `font` with
    // `code`, or the moon (font null).
    struct Sprite
    {
        int x;
        int y;
        const AodGlyph *glyph;
        const lv_font_t *font;
        uint32_t code;
        Colour colour;
    };

    static const int MaxSprites = 16;

    struct Face
    {
        Sprite sprites[MaxSprites];
        int count;
        int width;
        int height;
    };

    // Places the face's bitmaps for the local time: the time and the date under it.
    static Face layout( const Look &look, const Rtc::DateTime &time );

    // The moon's bitmap for MoonX, MoonY, or null when the Moon item is off. Kept in a static
    // buffer until the next call.
    static const AodGlyph *moon( const Look &look );

    // The idle-mode colour of a corner gauge, or -1 when it draws nothing.
    static int gaugeColour( Corners::Kind kind );

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
    // nothing and asks for another wake in a minute. `batteryPercent` < 0: unknown.
    uint64_t update( const Rtc::DateTime &time, int64_t utcNow, bool timeRead, int batteryPercent );

#if CONFIG_WATCH_AOD_CYCLE_MIN > 0
    // Battery run V: the style shown `seconds` after power-on, or -1 for the blank face.
    static int cycleStyle( uint32_t seconds );
#endif

#if CONFIG_WATCH_DEV_LOGGING
    // Development screenshot (tools/screenshot.py): the face at 19:19 on Sunday 21 June, the
    // Moon 45 % and waxing, with the corner gauges of that moment in Istanbul and the battery at
    // 92 %, written to the console instead of drawn. A fixed scene keeps the published picture
    // unchanged.
    static void capture( int style );
#endif
};
