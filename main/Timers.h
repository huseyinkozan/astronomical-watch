#pragma once

#include <cstdint>

// The stopwatch and the countdown of the clock page. Both keep running while the watch sleeps or
// is off: they store Unix times in milliseconds (the RTC's time), not a running counter, and are
// written to NVS only when a button changes them. The countdown gives no signal yet (the
// vibration motor comes with the alarms); it shows 00:00 once it has run out.
class Timers
{
public:
    static const int LapCount = 3;    // the latest laps shown
    static const int MaxMinutes = 99;  // the countdown's limit, so that it fits as mm:ss

    struct Stopwatch
    {
        bool running;
        int64_t startMs;        // when the running stretch began
        int64_t accumulatedMs;  // of the stretches before it
        int laps;               // laps taken since the reset
        int64_t lastLapMs;      // stopwatch time at the latest lap
        int64_t lapMs[LapCount];  // lap times, the latest first
    };

    struct Countdown
    {
        bool running;
        int durationS;       // as set with the buttons
        int64_t endMs;       // when running: when it reaches zero
        int64_t remainingMs;  // when paused
    };

    // Reads the saved state; nothing saved (or a different layout): both at zero.
    void load();

    const Stopwatch &stopwatch() const;
    int64_t elapsedMs( int64_t nowMs ) const;
    void startStopStopwatch( int64_t nowMs );
    void lap( int64_t nowMs );
    void resetStopwatch();

    const Countdown &countdown() const;
    int64_t remainingMs( int64_t nowMs ) const;
    void addMinutes( int minutes );
    void startPauseCountdown( int64_t nowMs );
    void resetCountdown();

private:
    void save();

    struct State
    {
        Stopwatch stopwatch;
        Countdown countdown;
    };
    State m_state = {};
};
