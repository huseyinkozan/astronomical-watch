#pragma once

#include <cstdint>

// An approximate tide from the Moon alone, as Casio's tide graph (docs/superpowers/specs/
// 2026-11-23-tide-design.md): high water at the Moon's upper and lower transits plus the place's
// lunitidal interval, the level a cosine of half the time between two upper transits. It tells
// when the water is high or low, not how high. Plain C++ for the host tests (tests/TideTest.cpp);
// times are Unix seconds.
class Tide
{
public:
    // The Moon's upper transits of yesterday, today, tomorrow and the day after (0: none that day).
    static const int TransitCount = 4;
    // The interval setting's "not set": nothing shows the tide.
    static const int Off = -1;

    struct State
    {
        bool valid;        // an interval set and transits around the moment
        int level;         // thousandths: 0 low water, 1000 high water
        bool rising;
        int64_t nextHigh;  // 0 when unknown
        int64_t nextLow;
        int spring;        // thousandths: 1000 at new and full Moon, 0 at the quarters
    };

    // `moonAge`: days since the new Moon.
    static State at( const int64_t transits[TransitCount], int intervalMinutes, double moonAge, int64_t now );

    struct Water
    {
        int64_t time;
        bool high;
    };

    // The high and low waters in [from, to), in time order; returns how many (at most `max`).
    static int waters( const int64_t transits[TransitCount], int intervalMinutes, int64_t from, int64_t to, Water out[],
                       int max );
};
