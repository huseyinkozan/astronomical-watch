#pragma once

#include <cstdint>

// The stopwatch and the countdown of the clock page. Both keep running while the watch sleeps or
// is off: they store Unix times in milliseconds (the RTC's time), not a running counter, and are
// written to NVS only when a button changes them. The countdown rings as an alarm (main.cpp) and
// shows 00:00 once it has run out; in the regatta mode its signals are short beeps instead
// (ClockPages::serviceRegatta).
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
        // 1: the regatta start mode (Regatta). In the padding after `running`, so that the saved
        // layout keeps its size; any other value reads as the normal countdown.
        uint8_t regatta;
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
    // After it rang: stopped at 00:00, its duration kept for the next start.
    void finishCountdown();

    // The regatta start mode: on, the countdown stands at 5:00; off, at zero. Reset keeps the mode.
    bool regatta() const;
    void setRegatta( bool on );
    // While it runs: the time left rounded to the nearest minute (Regatta::sync).
    void syncRegatta( int64_t nowMs );

private:
    void save();

    struct State
    {
        Stopwatch stopwatch;
        Countdown countdown;
    };
    State m_state = {};
};
