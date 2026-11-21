#pragma once

#include <cstdint>

// The countdown's regatta start mode (ClockPages): the 5-4-1-0 sequence of a sailing race
// start. The countdown runs from 5:00, short beeps at the lowest alarm level mark the 4 minute
// and 1 minute signals and the start, and "sync" rounds the time left to the nearest minute when
// the committee's signal is seen or heard. Plain C++ for the host tests.
class Regatta
{
public:
    static const int DurationS = 300;

    // A signal's sound: `beeps` tones of `toneMs` each (Ringer::signal); none when beeps is 0.
    struct Signal
    {
        int beeps;
        int toneMs;
    };

    // The signal due when the time left goes from `beforeMs` to `afterMs` (milliseconds): four
    // short beeps at 4 minutes, one longer at 1 minute, a long one at the start.
    static Signal signal( int64_t beforeMs, int64_t afterMs );

    // The time left rounded to the nearest whole minute (a half minute rounds up).
    static int64_t sync( int64_t remainingMs );
};
