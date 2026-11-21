#include "Regatta.h"

static bool passes( int64_t beforeMs, int64_t afterMs, int64_t markMs )
{
    return beforeMs > markMs && afterMs <= markMs;
}

Regatta::Signal Regatta::signal( int64_t beforeMs, int64_t afterMs )
{
    if ( passes( beforeMs, afterMs, 0 ) ) {
        return { 1, 1500 };
    }
    if ( passes( beforeMs, afterMs, 60000 ) ) {
        return { 1, 600 };
    }
    if ( passes( beforeMs, afterMs, 240000 ) ) {
        return { 4, 200 };
    }
    return { 0, 0 };
}

int64_t Regatta::sync( int64_t remainingMs )
{
    if ( remainingMs <= 0 ) {
        return 0;
    }
    return ( remainingMs + 30000 ) / 60000 * 60000;
}
