#include "Hayiz.h"

#include <cstdint>

static const int s_maxDays = 10;
static const int s_minDays = 3;
static const int s_minPurity = 15;

namespace {

struct Labels
{
    uint8_t kinds[CycleLog::Days];  // Hayiz::Kind, a byte each (static RAM)
    int first;  // the day of kinds[0]
    int habit;
};

bool bleeds( const CycleLog &log, int day )
{
    return log.flow( day ) > 0;
}

// Every day the log covers, labelled in order.
void label( const CycleLog &log, Labels &out )
{
    const int newest = log.newestDay;
    out.first = newest - CycleLog::Days + 1;
    out.habit = 0;
    for ( int i = 0; i < CycleLog::Days; ++i ) {
        out.kinds[i] = Hayiz::Purity;
    }
    bool hadHayz = false;
    int lastEnd = 0;
    int day = out.first;
    while ( day <= newest ) {
        if ( !bleeds( log, day ) ) {
            ++day;
            continue;
        }
        if ( hadHayz && day - lastEnd - 1 < s_minPurity ) {
            out.kinds[day - out.first] = Hayiz::Istihaze;
            ++day;
            continue;
        }
        const int start = day;
        const int windowEnd = start + s_maxDays - 1;
        if ( windowEnd + 1 <= newest && bleeds( log, windowEnd + 1 ) ) {
            // Past ten days: the habit is hayız; the bleeding after it falls in the purity rule.
            const int length = out.habit > 0 ? out.habit : s_maxDays;
            for ( int d = start; d < start + length; ++d ) {
                out.kinds[d - out.first] = Hayiz::Hayz;
            }
            out.habit = length;
            hadHayz = true;
            lastEnd = start + length - 1;
            day = start + length;
            continue;
        }
        int lastBleeding = start;
        for ( int d = start; d <= windowEnd && d <= newest; ++d ) {
            if ( bleeds( log, d ) ) {
                lastBleeding = d;
            }
        }
        const bool ongoing = windowEnd > newest && lastBleeding >= newest - 1;
        const int length = lastBleeding - start + 1;
        if ( length >= s_minDays || ongoing ) {
            for ( int d = start; d <= lastBleeding; ++d ) {
                out.kinds[d - out.first] = Hayiz::Hayz;
            }
            if ( !ongoing ) {
                out.habit = length;
            }
            hadHayz = true;
            lastEnd = lastBleeding;
        } else {
            for ( int d = start; d <= lastBleeding; ++d ) {
                if ( bleeds( log, d ) ) {
                    out.kinds[d - out.first] = Hayiz::Istihaze;
                }
            }
        }
        day = lastBleeding + 1;
    }
}

Labels s_labels;

}  // namespace

Hayiz::Kind Hayiz::kindOn( const CycleLog &log, int day )
{
    label( log, s_labels );
    const int index = day - s_labels.first;
    return index >= 0 && index < CycleLog::Days ? static_cast<Kind>( s_labels.kinds[index] ) : Purity;
}

Hayiz::Today Hayiz::today( const CycleLog &log, int day )
{
    label( log, s_labels );
    Today t = { Purity, 0, s_labels.habit };
    const int index = day - s_labels.first;
    if ( index < 0 || index >= CycleLog::Days ) {
        return t;
    }
    t.kind = static_cast<Kind>( s_labels.kinds[index] );
    if ( t.kind == Hayz ) {
        int start = index;
        while ( start > 0 && s_labels.kinds[start - 1] == Hayz ) {
            --start;
        }
        t.day = index - start + 1;
    } else if ( t.kind == Purity ) {
        for ( int i = index - 1; i >= 0; --i ) {
            if ( s_labels.kinds[i] == Hayz ) {
                t.day = index - i;
                break;
            }
        }
    }
    return t;
}

int Hayiz::hayzDays( const CycleLog &log, int first, int length )
{
    label( log, s_labels );
    int count = 0;
    for ( int day = first; day < first + length; ++day ) {
        const int index = day - s_labels.first;
        if ( index >= 0 && index < CycleLog::Days && s_labels.kinds[index] == Hayz ) {
            ++count;
        }
    }
    return count;
}

int Hayiz::balance( const CycleLog &log )
{
    const int left = log.earlierBalance + log.ramadanAdded - log.madeUp;
    return left > 0 ? left : 0;
}
