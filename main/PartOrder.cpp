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
    if ( !valid && order > 0 ) {
        // An order saved before parts were added: its first k fields a permutation of 0..k-1,
        // the rest zero. It keeps its places; the new parts follow.
        for ( int k = count - 1; k > 0 && !valid; --k ) {
            bool prefix = ( order >> ( 3 * k ) ) == 0;
            bool used[MaxParts] = {};
            for ( int i = 0; i < k && prefix; ++i ) {
                natural[i] = ( order >> ( 3 * i ) ) & 7;
                prefix = natural[i] < k && !used[natural[i]];
                if ( prefix ) {
                    used[natural[i]] = true;
                }
            }
            if ( prefix ) {
                for ( int i = k; i < count; ++i ) {
                    natural[i] = i;
                }
                valid = true;
            }
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

int PartOrder::keep( const int *parts, int shown, int part )
{
    return position( parts, shown, part ) >= 0 || shown < 1 ? part : parts[0];
}
