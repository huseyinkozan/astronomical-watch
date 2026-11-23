#include "Tide.h"

#include <cmath>

namespace {

const double s_synodicMonth = 29.530589;

// Two neighbouring known transits: high waters at `start` and every `period` until `end`. A day
// without a transit between them is bridged: the pair then spans four half periods, not two.
struct Span
{
    int64_t start;  // the first high water: the earlier transit plus the interval
    int64_t end;    // the later transit plus the interval
    double period;  // half the time between two upper transits (about 12 h 25 min)
};

// The spans of the known transits, in time order; returns how many.
int spans( const int64_t transits[Tide::TransitCount], int intervalMinutes, Span out[Tide::TransitCount] )
{
    const int64_t interval = static_cast<int64_t>( intervalMinutes ) * 60;
    int count = 0;
    int previous = -1;
    for ( int i = 0; i < Tide::TransitCount; ++i ) {
        if ( transits[i] == 0 ) {
            continue;
        }
        if ( previous >= 0 ) {
            const int halves = 2 * ( i - previous );
            out[count].start = transits[previous] + interval;
            out[count].end = transits[i] + interval;
            out[count].period = static_cast<double>( transits[i] - transits[previous] ) / halves;
            ++count;
        }
        previous = i;
    }
    return count;
}

}  // namespace

Tide::State Tide::at( const int64_t transits[TransitCount], int intervalMinutes, double moonAge, int64_t now )
{
    State state = {};
    if ( intervalMinutes < 0 ) {
        return state;
    }
    Span list[TransitCount];
    const int count = spans( transits, intervalMinutes, list );
    for ( int i = 0; i < count; ++i ) {
        const Span &span = list[i];
        if ( now < span.start || now >= span.end ) {
            continue;
        }
        const double phase = ( now - span.start ) / span.period;
        const double whole = std::floor( phase );
        const double part = phase - whole;
        state.valid = true;
        state.level = static_cast<int>( std::lround( 500.0 * ( 1.0 + std::cos( 2.0 * M_PI * part ) ) ) );
        state.rising = part >= 0.5;
        state.nextHigh = span.start + std::llround( ( whole + 1.0 ) * span.period );
        state.nextLow = span.start + std::llround( ( part < 0.5 ? whole + 0.5 : whole + 1.5 ) * span.period );
        break;
    }
    // Spring tides at new and full Moon, neap tides at the quarters: the absolute value turns the
    // month's one cosine into two peaks.
    state.spring = static_cast<int>( std::lround( 1000.0 * std::fabs( std::cos( 2.0 * M_PI * moonAge / s_synodicMonth ) ) ) );
    return state;
}

int Tide::waters( const int64_t transits[TransitCount], int intervalMinutes, int64_t from, int64_t to, Water out[],
                  int max )
{
    if ( intervalMinutes < 0 ) {
        return 0;
    }
    Span list[TransitCount];
    const int count = spans( transits, intervalMinutes, list );
    int n = 0;
    for ( int i = 0; i < count && n < max; ++i ) {
        const Span &span = list[i];
        // Every half of a half period: high, low, high, low...
        for ( int step = 0; n < max; ++step ) {
            const int64_t time = span.start + std::llround( step * span.period / 2.0 );
            if ( time >= span.end ) {
                break;
            }
            if ( time >= from && time < to ) {
                out[n].time = time;
                out[n].high = step % 2 == 0;
                ++n;
            }
        }
    }
    return n;
}
