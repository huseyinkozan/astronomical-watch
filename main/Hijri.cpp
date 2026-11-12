#include "Hijri.h"

#include <cmath>

#include "LandMask.h"

static const int64_t s_dayS = 86400;
// Evenings checked after a conjunction before the month starts anyway (a month has 29 or 30 days).
static const int s_maxEvenings = 4;
// Latest sunset that counts for Diyanet's rule, after 00:00 UT of the next day (see diyanetEvening).
static const int64_t s_diyanetLastSunsetS = 4 * 3600;

static double toRadians( double degrees )
{
    return degrees * 3.14159265358979323846 / 180.0;
}

static int64_t localMidnight( int day, int utcOffsetMinutes )
{
    return static_cast<int64_t>( day ) * s_dayS - static_cast<int64_t>( utcOffsetMinutes ) * 60;
}

// Local day number that contains `time`.
static int dayOf( int64_t time, int utcOffsetMinutes )
{
    const int64_t local = time + static_cast<int64_t>( utcOffsetMinutes ) * 60;
    return static_cast<int>( local >= 0 ? local / s_dayS : ( local - s_dayS + 1 ) / s_dayS );
}

Hijri::Crescent Hijri::crescent( int day, int utcOffsetMinutes, const Astronomy::Observer &observer )
{
    Crescent result = { ZoneNone, 0.0, 0 };
    int64_t sunset = 0;
    if ( !Astronomy::sunsetNear( localMidnight( day, utcOffsetMinutes ) + s_dayS / 2, observer, sunset ) ) {
        return result;
    }
    const int64_t conjunction = Astronomy::previousNewMoon( sunset );
    if ( sunset - conjunction > 3 * s_dayS ) {
        return result;
    }
    // The Moon must set after the Sun (a young crescent within hours).
    int64_t moonset = 0;
    if ( !Astronomy::moonsetWithin( sunset, s_dayS / 2, observer, moonset ) ) {
        return result;
    }
    const int64_t lag = moonset - sunset;
    // Odeh, like Yallop, evaluates the crescent at the best time: sunset plus 4/9 of the lag.
    const int64_t bestTime = sunset + lag * 4 / 9;
    if ( bestTime < conjunction ) {
        return result;
    }
    const Astronomy::Position moonPosition = Astronomy::moonPosition( bestTime, observer );
    const Astronomy::Position sunPosition = Astronomy::sunPosition( bestTime, observer );
    // Topocentric, airless arc of vision and arc of light (elongation).
    const double arcv = moonPosition.altitude - sunPosition.altitude;
    const double moonAltitude = toRadians( moonPosition.altitude );
    const double sunAltitude = toRadians( sunPosition.altitude );
    const double arcl = std::acos( std::sin( moonAltitude ) * std::sin( sunAltitude ) +
                                   std::cos( moonAltitude ) * std::cos( sunAltitude ) *
                                       std::cos( toRadians( moonPosition.azimuth - sunPosition.azimuth ) ) );
    // Crescent width in arcminutes.
    const double width = Astronomy::moonSemiDiameter( bestTime ) * 60.0 * ( 1 - std::cos( arcl ) );
    result.v = arcv - ( 7.1651 - 6.3226 * width + 0.7319 * width * width - 0.1018 * width * width * width );
    result.bestTime = bestTime;
    if ( result.v >= 5.65 ) {
        result.zone = ZoneA;
    } else if ( result.v >= 2.0 ) {
        result.zone = ZoneB;
    } else if ( result.v >= -0.96 ) {
        result.zone = ZoneC;
    } else {
        result.zone = ZoneD;
    }
    return result;
}

bool Hijri::diyanetEvening( int day, int64_t conjunction )
{
    // The evening of date `day` at each place, by its local mean time. Diyanet does not publish
    // a time limit; sunsets up to 04:00 UT of the next day (07:00 in Turkey) reproduce 158 of
    // the 161 month starts it published for 2023-2035 (tests/diyanet_month_starts.csv). Without
    // a limit, Alaskan evenings started some months a day early; with UTC days only, American
    // evenings started others a day late. The three left lie within 0.1 degree of a limit.
    const int64_t latest = static_cast<int64_t>( day + 1 ) * s_dayS + s_diyanetLastSunsetS;
    for ( int row = 0; row < s_landRows; ++row ) {
        for ( int column = 0; column < s_landColumns; ++column ) {
            const int bit = row * s_landColumns + column;
            if ( ( s_landMask[bit / 8] & ( 1 << ( bit % 8 ) ) ) == 0 ) {
                continue;
            }
            Astronomy::Observer place = {};
            place.latitude = s_landSouth + row * s_landCellDegrees + s_landCellDegrees / 2.0;
            place.longitude = -180.0 + column * s_landCellDegrees + s_landCellDegrees / 2.0;
            const int64_t noon = static_cast<int64_t>( day ) * s_dayS + s_dayS / 2 -
                                 static_cast<int64_t>( std::lround( place.longitude / 15.0 * 3600.0 ) );
            int64_t sunset = 0;
            if ( !Astronomy::sunsetNear( noon, place, sunset ) || sunset < conjunction || sunset > latest ) {
                continue;
            }
            // Geocentric separation and topocentric, airless altitude: the published wording.
            if ( Astronomy::moonSunSeparation( sunset ) >= 8.0 &&
                 Astronomy::moonPosition( sunset, place ).altitude >= 5.0 ) {
                return true;
            }
        }
    }
    return false;
}

// Local day number on which the month that follows `conjunction` begins.
static int monthStart( int64_t conjunction, int utcOffsetMinutes, const Astronomy::Observer &observer,
                       Hijri::Method method )
{
    // Diyanet's evenings are dated by each place's own date, the first one by UTC.
    const int first = dayOf( conjunction, method == Hijri::Diyanet ? 0 : utcOffsetMinutes );
    for ( int i = 0; i < s_maxEvenings; ++i ) {
        const int evening = first + i;
        bool seen = false;
        if ( method == Hijri::Diyanet ) {
            seen = Hijri::diyanetEvening( evening, conjunction );
        } else {
            seen = Hijri::crescent( evening, utcOffsetMinutes, observer ).zone == Hijri::ZoneA;
        }
        if ( seen ) {
            return evening + 1;
        }
    }
    return first + s_maxEvenings;
}

Hijri::Date Hijri::date( int day, int utcOffsetMinutes, const Astronomy::Observer &observer, Method method )
{
    int64_t conjunction = Astronomy::previousNewMoon( localMidnight( day + 1, utcOffsetMinutes ) );
    int start = monthStart( conjunction, utcOffsetMinutes, observer, method );
    if ( start > day ) {
        conjunction = Astronomy::previousNewMoon( conjunction - s_dayS );
        start = monthStart( conjunction, utcOffsetMinutes, observer, method );
    }
    // The tabular calendar names the month: its middle is at most two days from the observed one.
    Date result = tabular( start + 14 );
    result.day = day - start + 1;
    return result;
}

// Julian day of a tabular Islamic date (Fourmilab's calendar converter).
static double tabularToJulianDay( int year, int month, int day )
{
    return day + std::ceil( 29.5 * ( month - 1 ) ) + ( year - 1 ) * 354.0 + std::floor( ( 3 + 11.0 * year ) / 30.0 ) +
           1948439.5 - 1;
}

Hijri::Date Hijri::tabular( int day )
{
    const double julianDay = day + 2440587.5;
    Date result = {};
    result.year = static_cast<int>( std::floor( ( 30 * ( julianDay - 1948439.5 ) + 10646 ) / 10631 ) );
    const double month = std::ceil( ( julianDay - ( 29 + tabularToJulianDay( result.year, 1, 1 ) ) ) / 29.5 ) + 1;
    result.month = month < 12 ? static_cast<int>( month ) : 12;
    result.day = static_cast<int>( julianDay - tabularToJulianDay( result.year, result.month, 1 ) + 1 );
    return result;
}
