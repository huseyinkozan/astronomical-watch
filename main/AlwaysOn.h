#pragma once

#include <cstdint>

#include "Rtc.h"
#include "sdkconfig.h"

// Always-on face (CLAUDE.md, always-on mode): a dim HH:MM in seven segments. A timer wake once a
// minute redraws only the digits; between wakes the ESP32 deep-sleeps and the panel shows the
// image from its own memory. The state is kept in RTC memory.
class AlwaysOn
{
public:
    // Called when active mode ends; the following timer wakes draw the face until the
    // motion check powers the watch off (main.cpp) or a PWR wake ends it.
    void begin();

    // A PWR wake returns to active mode and ends the always-on face.
    void end();

    bool isActive() const;

    // Timer wake: draws the face for `time` and returns the microseconds until just after the
    // next minute, or 0 when the face cannot go on (the panel failed, or no valid time for
    // s_maxFailedMinutes). Without a valid time it draws nothing and asks for another wake in a
    // minute.
    uint64_t update( const Rtc::DateTime &time, bool timeRead );

#if CONFIG_WATCH_DEV_LOGGING
    // Development screenshot (tools/screenshot.py): the face at 19:19, centred, written to the
    // console as rectangles instead of drawn. A fixed time keeps the published picture unchanged.
    static void capture();
#endif
};
