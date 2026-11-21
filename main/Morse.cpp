#include "Morse.h"

// The units of one cycle, on and off in turn, starting with on: S, O, S, then the word gap.
static const int s_segments[] = { 1, 1, 1, 1, 1, 3, 3, 1, 3, 1, 3, 3, 1, 1, 1, 1, 1, 7 };
static const int s_segmentCount = sizeof( s_segments ) / sizeof( s_segments[0] );

bool Morse::sosLit( int64_t ms, int *remainingMs )
{
    if ( ms < 0 ) {
        ms = 0;
    }
    int unitMs = static_cast<int>( ms % ( static_cast<int64_t>( CycleUnits ) * UnitMs ) );
    for ( int i = 0; i < s_segmentCount; ++i ) {
        const int length = s_segments[i] * UnitMs;
        if ( unitMs < length ) {
            if ( remainingMs != nullptr ) {
                *remainingMs = length - unitMs;
            }
            return i % 2 == 0;
        }
        unitMs -= length;
    }
    if ( remainingMs != nullptr ) {
        *remainingMs = UnitMs;
    }
    return false;
}
