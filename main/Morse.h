#pragma once

#include <cstdint>

// The torch's SOS: "... --- ..." in Morse timing (a dot one unit, a dash three, one unit between
// the signs of a letter, three between letters, seven before it starts again), repeated. The
// light and the optional beeps follow the same clock. Plain C++ for the host tests.
class Morse
{
public:
    static const int UnitMs = 200;
    static const int CycleUnits = 34;

    // Whether the signal is on `ms` after the start, and how many milliseconds that lasts.
    static bool sosLit( int64_t ms, int *remainingMs = nullptr );
};
