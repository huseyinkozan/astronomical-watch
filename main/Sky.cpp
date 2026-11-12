#include "Sky.h"

#include <cmath>

static const int64_t s_dayS = 86400;
static const int s_darkStepS = 600;

Sky::Day Sky::computeDay( int day, int utcOffsetMinutes, const Astronomy::Observer &observer, Hijri::Method method )
{
    Day result = {};
    result.day = day;
    result.localMidnight = static_cast<int64_t>( day ) * s_dayS - static_cast<int64_t>( utcOffsetMinutes ) * 60;
    result.sun = Astronomy::sunDay( result.localMidnight, observer );
    const Astronomy::SunDay yesterday = Astronomy::sunDay( result.localMidnight - s_dayS, observer );
    result.dayLengthChangeSeconds = result.sun.dayLengthSeconds - yesterday.dayLengthSeconds;
    result.moon = Astronomy::moonDay( result.localMidnight, observer );
    result.phase = Astronomy::moonPhase( result.localMidnight + s_dayS / 2 );
    result.hijri = Hijri::date( day, utcOffsetMinutes, observer, method );
    result.crescent = Hijri::crescent( day, utcOffsetMinutes, observer );
    result.nextSeason = Astronomy::nextSeason( result.localMidnight, result.season );

    // Tonight's astronomical night, from today's dusk to tomorrow's dawn (taken as today's dawn a
    // day later: a few minutes off at most outside the polar regions), and its longest stretch
    // with the Moon below the horizon, in 10-minute steps.
    const Astronomy::Crossings &night = result.sun.astronomical;
    if ( night.setting.occurs && night.rising.occurs ) {
        const int64_t dusk = night.setting.time;
        const int64_t dawn = night.rising.time + s_dayS;
        int64_t runStart = 0;
        bool inRun = false;
        for ( int64_t time = dusk; time <= dawn; time += s_darkStepS ) {
            const bool moonDown = Astronomy::moonPosition( time, observer ).altitude < 0;
            if ( moonDown && !inRun ) {
                runStart = time;
                inRun = true;
            }
            const bool runEnds = inRun && ( !moonDown || time + s_darkStepS > dawn );
            if ( runEnds ) {
                const int64_t runEnd = moonDown ? dawn : time;
                if ( runEnd - runStart > result.darkEnd - result.darkStart ) {
                    result.darkStart = runStart;
                    result.darkEnd = runEnd;
                    result.darkWindow = true;
                }
                inRun = false;
            }
        }
    }
    return result;
}

Sky::Now Sky::computeNow( int64_t time, const Day &day, const Astronomy::Observer &observer )
{
    Now now = {};
    now.sun = Astronomy::sunPosition( time, observer );
    now.moon = Astronomy::moonPosition( time, observer );
    now.sunUp = now.sun.altitude > -0.833;
    const Astronomy::Crossings &sun = day.sun.riseSet;
    if ( now.sunUp && sun.setting.occurs && sun.setting.time > time ) {
        now.secondsToSunEvent = static_cast<int>( sun.setting.time - time );
    } else if ( !now.sunUp && sun.rising.occurs ) {
        // Before today's sunrise, or after sunset: tomorrow's is about a day after today's.
        const int64_t rising = sun.rising.time > time ? sun.rising.time : sun.rising.time + s_dayS;
        now.secondsToSunEvent = static_cast<int>( rising - time );
    }
    return now;
}

int Sky::phaseIndex( const Astronomy::MoonPhase &phase )
{
    return static_cast<int>( std::floor( ( phase.elongation + 22.5 ) / 45.0 ) ) % 8;
}

Sky::Tide Sky::tide( const Astronomy::MoonPhase &phase )
{
    // Within about 2.5 days (30 degrees of elongation) of a syzygy or a quarter.
    const double half = std::fmod( phase.elongation, 180.0 );
    if ( half < 30.0 || half > 150.0 ) {
        return TideSpring;
    }
    if ( std::fabs( half - 90.0 ) < 30.0 ) {
        return TideNeap;
    }
    return TideMiddle;
}
