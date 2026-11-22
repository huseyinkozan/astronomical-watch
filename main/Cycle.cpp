#include "Cycle.h"

#include <cmath>

// A cycle starts with a bleeding day after at least this many clean days.
static const int s_cleanToStart = 10;
// The averages use the last complete cycles, as Ogino-Knaus does.
static const int s_averaged = 6;

bool Cycle::teenActive( const CycleLog &log, int today )
{
    if ( log.teenMode == CycleLog::TeenOn ) {
        return true;
    }
    if ( log.teenMode != CycleLog::TeenAuto ) {
        return false;
    }
    return log.firstPeriodDay == 0 || today - log.firstPeriodDay < 3 * 365;
}

static int roundDays( double value )
{
    return static_cast<int>( std::floor( value + 0.5 ) );
}

static void addPeriod( Cycle::State &s, const Cycle::Period &period )
{
    if ( s.count == Cycle::MaxCycles ) {
        for ( int i = 1; i < Cycle::MaxCycles; ++i ) {
            s.periods[i - 1] = s.periods[i];
        }
        --s.count;
    }
    s.periods[s.count++] = period;
}

Cycle::State Cycle::analyse( const CycleLog &log, int today )
{
    State s = {};
    s.today = today;
    s.teen = teenActive( log, today );
    const int shortLimit = s.teen ? 21 : 24;
    const int longLimit = s.teen ? 45 : 38;
    const int bleedingLimit = s.teen ? 7 : 8;

    // The periods: a start after enough clean days (or the log's first bleeding day); bleeding
    // with at most one clean day between belongs to the period; later bleeding is between
    // periods.
    int lastBleeding = 0;
    bool inPeriod = false;
    Period current = {};
    bool any = false;
    for ( int day = today - CycleLog::Days + 1; day <= today; ++day ) {
        const int flow = log.flow( day );
        if ( flow == 0 ) {
            continue;
        }
        // Spotting, a single light day, is no period: day 1 is the first day of a heavier flow or
        // of two bleeding days (a light day today counts until tomorrow is logged).
        if ( flow == 1 && log.flow( day - 1 ) == 0 && day != today && log.flow( day + 1 ) == 0 ) {
            continue;
        }
        if ( !any || day - lastBleeding > s_cleanToStart ) {
            if ( any ) {
                current.cycleLength = day - current.start;
                addPeriod( s, current );
            }
            current = {};
            current.start = day;
            // Within the clean days a start needs of the log's first day: the period (and so its
            // cycle) may have begun earlier.
            current.cut = day - ( today - CycleLog::Days + 1 ) <= s_cleanToStart;
            inPeriod = true;
            any = true;
        } else if ( inPeriod && day - lastBleeding <= 2 ) {
            // Part of the period.
        } else {
            inPeriod = false;
            current.warnings |= BetweenPeriods;
        }
        if ( inPeriod ) {
            current.length = day - current.start + 1;
            ++current.bleedingDays;
            if ( flow == 3 ) {
                ++current.heavyDays;
            }
        }
        lastBleeding = day;
    }
    if ( any ) {
        addPeriod( s, current );
    }
    if ( s.count == 0 ) {
        return s;
    }

    // Warnings of each period and cycle.
    for ( int i = 0; i < s.count; ++i ) {
        Period &p = s.periods[i];
        if ( p.cycleLength > 0 && p.cycleLength < shortLimit && !p.cut ) {
            p.warnings |= Short;
        }
        if ( p.cycleLength > longLimit && !p.cut ) {
            p.warnings |= Long;
        }
        if ( p.length > bleedingLimit ) {
            p.warnings |= LongBleeding;
        }
        if ( p.heavyDays >= 3 ) {
            p.warnings |= ManyHeavy;
        }
    }
    Period &last = s.periods[s.count - 1];
    const int sinceStart = today - last.start;
    if ( sinceStart + 1 > longLimit ) {
        last.warnings |= Long;
    }
    if ( sinceStart >= 90 ) {
        last.warnings |= NoPeriod90;
    }
    s.warnings = last.warnings;
    s.cycleDay = sinceStart + 1;

    // The last complete cycles.
    double sum = 0;
    s.shortest = 0;
    s.longest = 0;
    int used[s_averaged] = {};
    for ( int i = s.count - 2; i >= 0 && s.complete < s_averaged; --i ) {
        if ( s.periods[i].cut ) {
            continue;
        }
        used[s.complete] = i;
        const int length = s.periods[i].cycleLength;
        sum += length;
        s.shortest = s.complete == 0 || length < s.shortest ? length : s.shortest;
        s.longest = length > s.longest ? length : s.longest;
        ++s.complete;
    }
    double mean = 0;
    if ( s.complete > 0 ) {
        mean = sum / s.complete;
        double squares = 0;
        for ( int k = 0; k < s.complete; ++k ) {
            const double d = s.periods[used[k]].cycleLength - mean;
            squares += d * d;
        }
        s.spread = s.complete > 1 ? std::sqrt( squares / ( s.complete - 1 ) ) : 0;
        s.meanLength = roundDays( mean );
    }
    s.irregular = s.complete < 3 || s.longest - s.shortest > 9;

    // The next start.
    if ( s.teen ) {
        if ( s.complete >= 3 ) {
            s.nextEarly = s.shortest - 2;
            s.nextLate = s.longest + 2;
        } else {
            s.nextEarly = 21;
            s.nextLate = 45;
        }
        s.nextMean = s.complete > 0 ? s.meanLength : 33;
    } else if ( s.complete == 1 ) {
        s.nextEarly = s.meanLength - 4;
        s.nextLate = s.meanLength + 4;
        s.nextMean = s.meanLength;
    } else if ( s.complete > 1 ) {
        const double margin = 2 * s.spread > 2 ? 2 * s.spread : 2;
        s.nextEarly = roundDays( mean - margin );
        s.nextLate = roundDays( mean + margin );
        s.nextMean = s.meanLength;
    }
    s.carryPad = s.nextEarly > 0 && sinceStart >= s.nextEarly;

    // The fertile window, never narrower than the wider candidate (too narrow is the unsafe
    // error); none for adolescents.
    if ( !s.teen ) {
        if ( s.complete < 3 ) {
            s.fertileFirst = 6;
            s.fertileLast = 21;
        } else if ( s.complete < s_averaged ) {
            s.fertileFirst = s.irregular ? 6 : 8;
            s.fertileLast = s.irregular ? 21 : 19;
        } else {
            // Ogino-Knaus, widened to Standard Days' 8-19 (a regular cycle's Ogino window is
            // narrower) and, when irregular, to 6-21.
            s.fertileFirst = s.shortest - 18 < 8 ? s.shortest - 18 : 8;
            s.fertileLast = s.longest - 11 > 19 ? s.longest - 11 : 19;
            if ( s.irregular ) {
                s.fertileFirst = s.fertileFirst < 6 ? s.fertileFirst : 6;
                s.fertileLast = s.fertileLast > 21 ? s.fertileLast : 21;
            }
            s.fertileFirst = s.fertileFirst < 1 ? 1 : s.fertileFirst;
        }
        if ( s.complete >= 3 ) {
            s.ovulationDay = s.nextMean - 13;
        }
    }
    return s;
}
