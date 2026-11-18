#include "Sky.h"

#include "MilkyWay.h"

#include <cmath>

#include "Dates.h"

static const int64_t s_dayS = 86400;
static const int s_darkStepS = 600;
static const double s_kaabaLatitude = 21.4225;
static const double s_kaabaLongitude = 39.8262;

static double toRadians( double degrees )
{
    return degrees * M_PI / 180.0;
}

// -180 to 180 degrees.
static double signedAngle( double degrees )
{
    degrees = std::fmod( degrees, 360.0 );
    if ( degrees > 180.0 ) {
        degrees -= 360.0;
    } else if ( degrees <= -180.0 ) {
        degrees += 360.0;
    }
    return degrees;
}

static double qiblaBearing( const Astronomy::Observer &observer )
{
    const double phi1 = toRadians( observer.latitude );
    const double phi2 = toRadians( s_kaabaLatitude );
    const double deltaLambda = toRadians( s_kaabaLongitude - observer.longitude );
    const double y = std::sin( deltaLambda ) * std::cos( phi2 );
    const double x = std::cos( phi1 ) * std::sin( phi2 ) - std::sin( phi1 ) * std::cos( phi2 ) * std::cos( deltaLambda );
    const double bearing = std::atan2( y, x ) * 180.0 / M_PI;
    return bearing < 0 ? bearing + 360.0 : bearing;
}

// The first time from sunrise to sunset when the Sun's azimuth passes `bearing`: 10-minute
// steps, then bisection to a second.
static Astronomy::Event sunAtBearing( const Astronomy::SunDay &sun, double bearing, const Astronomy::Observer &observer )
{
    Astronomy::Event event = {};
    if ( !sun.riseSet.rising.occurs || !sun.riseSet.setting.occurs ) {
        return event;
    }
    const int64_t end = sun.riseSet.setting.time;
    int64_t before = sun.riseSet.rising.time;
    double diffBefore = signedAngle( Astronomy::sunPosition( before, observer ).azimuth - bearing );
    for ( int64_t time = before + s_darkStepS; time <= end + s_darkStepS; time += s_darkStepS ) {
        const int64_t at = time < end ? time : end;
        const double diff = signedAngle( Astronomy::sunPosition( at, observer ).azimuth - bearing );
        // A sign change near the bearing, not across its opposite direction.
        if ( ( diffBefore < 0 ) != ( diff < 0 ) && std::fabs( diff - diffBefore ) < 90.0 ) {
            int64_t low = before;
            int64_t high = at;
            while ( high - low > 1 ) {
                const int64_t middle = ( low + high ) / 2;
                const double d = signedAngle( Astronomy::sunPosition( middle, observer ).azimuth - bearing );
                if ( ( d < 0 ) == ( diffBefore < 0 ) ) {
                    low = middle;
                } else {
                    high = middle;
                }
            }
            event.occurs = true;
            event.time = high;
            return event;
        }
        before = at;
        diffBefore = diff;
        if ( at == end ) {
            break;
        }
    }
    return event;
}

Sky::Day Sky::computeDay( int day, int utcOffsetMinutes, const Astronomy::Observer &observer, Hijri::Method method,
                          int ishaAngle )
{
    Day result = {};
    result.day = day;
    result.localMidnight = static_cast<int64_t>( day ) * s_dayS - static_cast<int64_t>( utcOffsetMinutes ) * 60;
    result.sun = Astronomy::sunDay( result.localMidnight, observer );
    const Astronomy::SunDay yesterday = Astronomy::sunDay( result.localMidnight - s_dayS, observer );
    result.dayLengthChangeSeconds = result.sun.dayLengthSeconds - yesterday.dayLengthSeconds;
    result.moon = Astronomy::moonDay( result.localMidnight, observer );
    result.sunsetBefore = yesterday.riseSet.setting;
    // The Moon's events move by about 50 min a day, so the neighbouring days are computed.
    result.moonBefore = Astronomy::moonDay( result.localMidnight - s_dayS, observer );
    result.moonAfter = Astronomy::moonDay( result.localMidnight + s_dayS, observer );
    result.phase = Astronomy::moonPhase( result.localMidnight + s_dayS / 2 );
    result.hijri = Hijri::date( day, utcOffsetMinutes, observer, method );
    result.crescent = Hijri::crescent( day, utcOffsetMinutes, observer );
    result.nextSeason = Astronomy::nextSeason( result.localMidnight, result.season );
    // Mean solar noon at the observer's longitude on this date, against the Sun's transit.
    if ( result.sun.noon.occurs ) {
        const int64_t meanNoon = static_cast<int64_t>( day ) * s_dayS + s_dayS / 2 -
                                 static_cast<int64_t>( std::lround( observer.longitude * 240.0 ) );
        result.equationOfTime = static_cast<int>( meanNoon - result.sun.noon.time );
    }
    result.qiblaBearing = qiblaBearing( observer );
    result.qibla = sunAtBearing( result.sun, result.qiblaBearing, observer );
    result.prayers = prayerTimes( day, observer, ishaAngle );
    result.isoWeek = Dates::isoWeek( day );
    result.dayOfYear = Dates::dayOfYear( day );

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

// To the nearest minute, as Diyanet publishes them.
static int64_t roundedMinute( int64_t time )
{
    return ( time + 30 ) / 60 * 60;
}

Sky::PrayerTimes Sky::prayerTimes( int day, const Astronomy::Observer &observer, int ishaAngle )
{
    // Like Diyanet: the Sun's declination and the equation of time taken once for the date (at
    // 0 h UT), then each time from the hour angle at which the Sun reaches its altitude.
    double declination = 0;
    double equation = 0;
    Astronomy::sunDeclinationAndEquation( static_cast<int64_t>( day ) * s_dayS, declination, equation );
    const double transit = static_cast<double>( day ) * s_dayS + s_dayS / 2 - observer.longitude * 240.0 - equation * 60.0;
    const double phi = toRadians( observer.latitude );
    const double delta = toRadians( declination );
    // Seconds from the transit at which the Sun's centre is at `altitude`; false when it never is.
    auto fromTransit = [&]( double altitude, double &seconds ) {
        const double cosH = ( std::sin( toRadians( altitude ) ) - std::sin( phi ) * std::sin( delta ) ) /
                            ( std::cos( phi ) * std::cos( delta ) );
        if ( cosH < -1.0 || cosH > 1.0 ) {
            return false;
        }
        seconds = std::acos( cosH ) * 180.0 / M_PI * 240.0;
        return true;
    };
    // Asr: a shadow as long as the object plus its noon shadow.
    const double asrAltitude = std::atan( 1.0 / ( 1.0 + std::tan( std::fabs( phi - delta ) ) ) ) * 180.0 / M_PI;
    const double altitudes[PrayerCount] = { -18.0, -0.8333, 0, 0, -0.8333, -static_cast<double>( ishaAngle ) };
    static const int s_sides[PrayerCount] = { -1, -1, 0, 1, 1, 1 };
    static const int s_precautionMinutes[PrayerCount] = { 0, -7, 5, 4, 7, 0 };
    PrayerTimes result = {};
    double offsets[PrayerCount] = {};
    for ( int i = 0; i < PrayerCount; ++i ) {
        if ( i == Dhuhr ) {
            result.valid[i] = true;
        } else {
            result.valid[i] = fromTransit( i == Asr ? asrAltitude : altitudes[i], offsets[i] );
        }
        const double time = transit + s_sides[i] * offsets[i] + s_precautionMinutes[i] * 60;
        result.times[i] = roundedMinute( static_cast<int64_t>( std::floor( time ) ) );
    }
    result.approximate = observer.latitude > 44.5 || observer.latitude < -44.5;
    // Where Diyanet uses 16 degrees it limits imsak and the night prayer in short summer nights
    // by a rule not reproduced here (Bucharest, tests/PrayerTest.cpp): from nights under 9 h.
    if ( ishaAngle == 16 && result.valid[Sunrise] && s_dayS - 2 * offsets[Sunrise] < 9 * 3600 ) {
        result.approximate = true;
    }
    // No dark enough twilight (summer at high latitudes): a third of the night after sunset and
    // before sunrise.
    if ( ( !result.valid[Imsak] || !result.valid[Isha] ) && result.valid[Sunrise] && result.valid[Maghrib] ) {
        const double night = s_dayS - 2 * offsets[Sunrise];
        if ( !result.valid[Imsak] ) {
            result.times[Imsak] = roundedMinute( static_cast<int64_t>( transit - offsets[Sunrise] - night / 3 ) );
            result.valid[Imsak] = true;
        }
        if ( !result.valid[Isha] ) {
            result.times[Isha] = roundedMinute( static_cast<int64_t>( transit + offsets[Maghrib] + night / 3 ) );
            result.valid[Isha] = true;
        }
        result.approximate = true;
    }
    return result;
}

Sky::Now Sky::computeNow( int64_t time, const Day &day, const Astronomy::Observer &observer )
{
    Now now = {};
    now.sun = Astronomy::sunPosition( time, observer );
    now.moon = Astronomy::moonPosition( time, observer );
    now.galacticCentre = Astronomy::starPosition( MilkyWay::CentreRaHours, MilkyWay::CentreDecDegrees, time, observer );
    now.south = observer.latitude < 0;
    now.time = time;
    now.sunUp = now.sun.altitude > -0.833;
    const Astronomy::Crossings &sun = day.sun.riseSet;
    if ( now.sunUp && sun.setting.occurs && sun.setting.time > time ) {
        now.secondsToSunEvent = static_cast<int>( sun.setting.time - time );
    } else if ( !now.sunUp && sun.rising.occurs ) {
        // Before today's sunrise, or after sunset: tomorrow's is about a day after today's.
        const int64_t rising = sun.rising.time > time ? sun.rising.time : sun.rising.time + s_dayS;
        now.secondsToSunEvent = static_cast<int>( rising - time );
    }
    const int64_t solar = time + static_cast<int64_t>( std::lround( observer.longitude * 240.0 ) ) + day.equationOfTime;
    now.solarSeconds = static_cast<int>( ( solar % s_dayS + s_dayS ) % s_dayS );
    double sidereal = std::fmod( Astronomy::greenwichSiderealTime( time ) + observer.longitude, 360.0 );
    if ( sidereal < 0 ) {
        sidereal += 360.0;
    }
    now.siderealSeconds = static_cast<int>( sidereal / 360.0 * s_dayS );
    // The last sunset: today's once it has passed, otherwise yesterday's.
    now.ezaniSeconds = -1;
    const Astronomy::Event &todaySet = sun.setting;
    const Astronomy::Event &lastSet = todaySet.occurs && time >= todaySet.time ? todaySet : day.sunsetBefore;
    if ( lastSet.occurs && time >= lastSet.time ) {
        now.ezaniSeconds = static_cast<int>( ( time - lastSet.time ) % ( s_dayS / 2 ) );
    }
    now.nextPrayer = -1;
    const PrayerTimes &prayers = day.prayers;
    for ( int i = 0; i < PrayerCount && now.nextPrayer < 0; ++i ) {
        if ( prayers.valid[i] && prayers.times[i] > time ) {
            now.nextPrayer = i;
            now.secondsToPrayer = static_cast<int>( prayers.times[i] - time );
        }
    }
    // After the night prayer: tomorrow's imsak, about a day after today's.
    if ( now.nextPrayer < 0 && prayers.valid[Imsak] && prayers.times[Imsak] + s_dayS > time ) {
        now.nextPrayer = Imsak;
        now.secondsToPrayer = static_cast<int>( prayers.times[Imsak] + s_dayS - time );
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
