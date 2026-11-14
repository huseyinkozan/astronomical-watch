#include "Zones.h"

int Zones::offsetAt( const Zone &zone, int64_t utcSeconds )
{
    const int64_t minutes = utcSeconds / 60;
    int offset = zone.offset;
    for ( int i = 0; i < 2; ++i ) {
        if ( zone.changeAt[i] != 0 && minutes >= zone.changeAt[i] ) {
            offset = zone.offsetAfter[i];
        }
    }
    return offset;
}

bool Zones::advance( Zone &zone, int64_t utcSeconds )
{
    const int64_t minutes = utcSeconds / 60;
    bool changed = false;
    while ( zone.changeAt[0] != 0 && minutes >= zone.changeAt[0] ) {
        zone.offset = zone.offsetAfter[0];
        zone.changeAt[0] = zone.changeAt[1];
        zone.offsetAfter[0] = zone.offsetAfter[1];
        zone.changeAt[1] = 0;
        zone.offsetAfter[1] = 0;
        changed = true;
    }
    return changed;
}
