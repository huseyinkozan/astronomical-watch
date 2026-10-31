#pragma once

#include <cstdint>

#include "Rtc.h"

// Always-on face (CLAUDE.md, always-on mode): a dim HH:MM in seven segments. A timer wake once a
// minute redraws only the digits; between wakes the ESP32 deep-sleeps and the panel shows the
// image from its own memory. The state is kept in RTC memory.
class AlwaysOn
{
public:
    // Called when the awake window ends; the following timer wakes draw the face for `minutes`.
    void begin( int minutes );

    // A PWR wake returns to active mode and ends the always-on face.
    void end();

    bool isActive() const;

    // Timer wake: draws the face for `time` and returns the microseconds until just after the
    // next minute, or 0 when the always-on time is over.
    uint64_t update( const Rtc::DateTime &time );
};
