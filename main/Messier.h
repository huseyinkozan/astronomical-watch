#pragma once

#include <cstdint>

#include "Astronomy.h"
#include "Observing.h"

// The deep-sky finder's catalogue (docs/superpowers/plans/2026-11-21-tools.md, stage 4): the 110
// Messier objects, and which of them stand above a given altitude now. Plain C++ for the host
// tests (tests/ObservingTest.cpp, against Skyfield: tools/make_observing_reference.py).
class Messier
{
public:
    static const int Count = 110;

    // J2000 (OpenNGC, tools/make_messier_table.py), packed: the right ascension in tenths of a
    // minute of time, the declination in arcminutes, the visual magnitude in tenths.
    struct Object
    {
        uint16_t rightAscension;
        int16_t declination;
        uint8_t magnitude;
    };
    static const Object Objects[Count];

    // Object `index` (M1 is 0): J2000 hours, degrees; its magnitude.
    static double raHours( int index );
    static double decDegrees( int index );
    static double magnitude( int index );

    // An object above the altitude: its index, altitude now (whole degrees) and the minute
    // (Unix time / 60) when it stands highest in tonight's darkness, 0 without darkness.
    struct Entry
    {
        uint8_t index;
        int8_t altitude;
        int32_t bestMinute;
    };

    // The objects above `minimumAltitude` at `time` (precessed to the date, without refraction),
    // highest first, into `entries` (room for Count); returns how many.
    static int visible( int64_t time, const Astronomy::Observer &observer, const Observing::Night &night,
                        double minimumAltitude, Entry *entries );
};
