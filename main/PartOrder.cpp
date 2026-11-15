#include "PartOrder.h"

int PartOrder::decode( int order, int hidden, int count, int alwaysShown, int parts[MaxParts] )
{
    count = count > MaxParts ? MaxParts : count;
    int natural[MaxParts] = {};
    bool seen[MaxParts] = {};
    bool valid = order > 0;
    for ( int i = 0; i < count && valid; ++i ) {
        natural[i] = ( order >> ( 3 * i ) ) & 7;
        valid = natural[i] < count && !seen[natural[i]];
        if ( valid ) {
            seen[natural[i]] = true;
        }
    }
    int shown = 0;
    for ( int i = 0; i < count; ++i ) {
        const int part = valid ? natural[i] : i;
        if ( part == alwaysShown || ( hidden & ( 1 << part ) ) == 0 ) {
            parts[shown++] = part;
        }
    }
    if ( shown == 0 ) {
        parts[shown++] = valid ? natural[0] : 0;
    }
    return shown;
}

int PartOrder::position( const int *parts, int shown, int part )
{
    for ( int i = 0; i < shown; ++i ) {
        if ( parts[i] == part ) {
            return i;
        }
    }
    return -1;
}

int PartOrder::step( const int *parts, int shown, int current, int steps )
{
    const int at = position( parts, shown, current );
    if ( at < 0 ) {
        return shown > 0 ? parts[0] : current;
    }
    return parts[( ( at + steps ) % shown + shown ) % shown];
}
