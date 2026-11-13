#pragma once

#include <cstdint>

#include "AodGlyphs.h"
#include "Rtc.h"
#include "sdkconfig.h"

// Always-on face (CLAUDE.md, always-on mode): a dim HH:MM in one of the aodStyles, with the
// Moon's phase, the date and a low-battery mark. A timer wake once a minute redraws the face;
// between wakes the ESP32 deep-sleeps and the panel shows the image from its own memory. The
// state is kept in RTC memory. The always-on page of active mode (AodPage) shows the same layout.
class AlwaysOn
{
public:
    // Bits of Settings::AodItems.
    enum Item
    {
        MoonItem = 1,
        DateItem = 2,
        BatteryItem = 4
    };

    // What the face shows, taken when active mode ends (the minute wakes read no settings and
    // compute no astronomy): the style (an aodStyles index), the Item bits, and the Moon's
    // illuminated fraction in percent and whether it is waxing.
    struct Look
    {
        int style;
        int items;
        int moonPercent;
        bool waxing;
    };

    enum Colour
    {
        White,
        Yellow,
        Red
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

    // Places the face's bitmaps for the local time: the time, the moon and the date under it, and
    // the battery mark over it (under the date for HH over MM). The moon's glyph is kept in a
    // static buffer until the next call.
    static Face layout( const Look &look, const Rtc::DateTime &time, bool batteryLow );

    // Called when active mode ends; the following timer wakes draw the face until the
    // motion check powers the watch off (main.cpp) or a PWR wake ends it.
    void begin( const Look &look );

    // A PWR wake returns to active mode and ends the always-on face.
    void end();

    bool isActive() const;

    // Timer wake: draws the face for `time` (local) and returns the microseconds until just
    // after the next minute, or 0 when the face cannot go on (the panel failed, or no valid time
    // for s_maxFailedMinutes). Without a valid time it draws nothing and asks for another wake in
    // a minute. `batteryLow` shows the low-battery mark (when that item is on).
    uint64_t update( const Rtc::DateTime &time, bool timeRead, bool batteryLow );

#if CONFIG_WATCH_AOD_CYCLE_MIN > 0
    // Battery run V: the style shown `seconds` after power-on, or -1 for the blank face.
    static int cycleStyle( uint32_t seconds );
#endif

#if CONFIG_WATCH_DEV_LOGGING
    // Development screenshot (tools/screenshot.py): the face at 19:19 on Sunday 21 June, the
    // Moon 45 % and waxing, the battery low, centred, written to the console instead of drawn. A
    // fixed scene keeps the published picture unchanged.
    static void capture( int style );
#endif
};
