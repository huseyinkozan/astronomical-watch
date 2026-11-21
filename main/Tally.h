#pragma once

#include <cstdint>

// The Tools page's counter (tasbih, meteors, laps): a count with a target and a rate. The target
// is none, 33, 99 or 100, or three steps of 33 (33 / 33 / 33, "step 2 / 3"); the rate is counts
// per hour since the first count, whose minute is kept (Settings::CounterStart) so that it survives
// sleep and power-off with the count. Plain C++ for the host tests.
class Tally
{
public:
    enum Target
    {
        NoTarget,
        Target33,
        Target99,
        Target100,
        Steps33,  // three steps of 33
        TargetCount
    };
    static const int MaxCount = 999999;

    int count = 0;
    int target = NoTarget;
    int64_t startMinute = 0;  // the first count's minute since 1970 UTC, 0: no count yet

    // `nowSeconds`: Unix time (0: not known; then no rate).
    void add( int64_t nowSeconds );
    void undo();
    void reset();
    void nextTarget();

    // The goal of one step (0: no target), the number of steps (1 without the sequence), the
    // step being counted (from 0) and the count within it.
    int goal() const;
    int stepCount() const;
    int step() const;
    int inStep() const;
    // The step's goal reached (the last count of the step).
    bool reached() const;

    // Counts per hour since the first count, -1 until a minute has passed.
    int perHour( int64_t nowSeconds ) const;
    // Minutes since the first count, -1 without one.
    int elapsedMinutes( int64_t nowSeconds ) const;
};
