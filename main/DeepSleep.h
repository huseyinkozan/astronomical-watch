#pragma once

#include <cstdint>

// Deep-sleep entry, wake-source classification and wake statistics kept in RTC memory.
// Statistics stand in for current measurement (see CLAUDE.md, power evaluation).
class DeepSleep
{
public:
    enum class Event : uint8_t
    {
        PowerOn,
        WakePowerKey,
        WakeTimer,
        WakeOther,
        PowerKeyPressed,
        SleepPowerKey,
        SleepFallbackTimer,
    };

    // Classifies the wake cause, updates and logs the statistics and the recent events.
    void onBoot();

    // Events are kept in RTC memory, so they survive a lost log line.
    void addEvent( Event event );

    // Configures the wake sources and enters deep sleep. Does not return.
    void enter();
};
