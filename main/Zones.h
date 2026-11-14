#pragma once

#include <cstdint>

// A time zone as the setup portal sends it: the UTC offset now and its next two changes
// (daylight saving time), which the browser computes from the city's IANA zone. Two changes
// cover a year, so the watch follows the clock changes until the next portal visit without a
// time zone database. Plain C++ for the host tests.
struct Zone
{
    int offset;          // minutes east of UTC
    int changeAt[2];     // minutes since 1970-01-01 UTC, in order; 0: none
    int offsetAfter[2];  // the offset from that change on
};

class Zones
{
public:
    // The offset at `utcSeconds` (Unix time).
    static int offsetAt( const Zone &zone, int64_t utcSeconds );

    // Moves the changes that have passed into `offset`; true when the zone changed.
    static bool advance( Zone &zone, int64_t utcSeconds );
};
