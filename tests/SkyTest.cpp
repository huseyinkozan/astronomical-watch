// Host test of the time page values in main/Sky.cpp (equation of time, qibla, ezani) and of
// main/Dates.cpp. Build and run: sh tests/run_host_tests.sh

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Almanac.h"
#include "Dates.h"
#include "Sky.h"
#include "Tide.h"

static int failures = 0;
static int checks = 0;

static void check( const char *name, double value, double expected, double tolerance )
{
    ++checks;
    if ( std::fabs( value - expected ) > tolerance ) {
        printf( "FAIL %s: %.2f, expected %.2f (±%.2f)\n", name, value, expected, tolerance );
        ++failures;
    }
}

int main()
{
    const Astronomy::Observer istanbul = { 41.0082, 28.9784 };
    const int turkey = 180;
    // Equation of time (apparent minus mean solar time) near its extremes, from the usual tables:
    // about -14 min 13 s on 11 February and +16 min 26 s on 3 November.
    const Sky::Day february = Sky::computeDay( Dates::dayNumber( 2026, 2, 11 ), turkey, istanbul, Hijri::Diyanet );
    check( "equation of time 11 Feb (s)", february.equationOfTime, -853, 30 );
    const Sky::Day november = Sky::computeDay( Dates::dayNumber( 2026, 11, 3 ), turkey, istanbul, Hijri::Diyanet );
    check( "equation of time 3 Nov (s)", november.equationOfTime, 986, 30 );
    // Qibla from Istanbul: about 151.6 degrees.
    check( "qibla bearing Istanbul", february.qiblaBearing, 151.6, 0.5 );

    // The fixed scene: 21 June 2026, 19:19 local (16:19 UTC); yesterday's sunset about 20:39.
    const Sky::Day june = Sky::computeDay( Dates::dayNumber( 2026, 6, 21 ), turkey, istanbul, Hijri::Diyanet );
    const int64_t scene = static_cast<int64_t>( Dates::dayNumber( 2026, 6, 21 ) ) * 86400 + ( 16 * 60 + 19 ) * 60;
    const Sky::Now now = Sky::computeNow( scene, june, istanbul );
    check( "ezani 19:19 (min)", now.ezaniSeconds / 60.0, 10 * 60 + 39, 3 );
    // Solar time: local mean time (UTC + 1 h 55.9 min) plus the equation of time (-1.6 min).
    check( "solar time 19:19 (min)", now.solarSeconds / 60.0, 16 * 60 + 19 + 115.9 - 1.6, 1.5 );
    // The Sun at the qibla's azimuth in the morning, before noon.
    // Turnaround: half of the 1 h 21 min left until the sunset at 20:40, so about 19:59.
    const int64_t turnaround = Sky::turnaround( now );
    check( "turnaround 19:19 (min after 19:19)", ( turnaround - scene ) / 60.0, 40.5, 1 );
    Sky::Now night = now;
    night.sunUp = false;
    check( "no turnaround at night", static_cast<double>( Sky::turnaround( night ) ), 0, 0 );
    check( "qibla time before noon", june.qibla.occurs && june.qibla.time < june.sun.noon.time ? 1 : 0, 1, 0 );

    // Dates.
    int year = 0;
    int month = 0;
    int day = 0;
    Dates::civil( Dates::dayNumber( 2026, 6, 21 ), year, month, day );
    check( "civil round trip", year * 10000 + month * 100 + day, 20260621, 0 );
    check( "ISO week 2026-06-21", Dates::isoWeek( Dates::dayNumber( 2026, 6, 21 ) ), 25, 0 );
    check( "ISO week 2021-01-03", Dates::isoWeek( Dates::dayNumber( 2021, 1, 3 ) ), 53, 0 );
    check( "ISO week 2026-01-01", Dates::isoWeek( Dates::dayNumber( 2026, 1, 1 ) ), 1, 0 );
    check( "day of year 2026-06-21", Dates::dayOfYear( Dates::dayNumber( 2026, 6, 21 ) ), 172, 0 );
    check( "weekday 2026-06-21 (Sunday)", Dates::weekday( Dates::dayNumber( 2026, 6, 21 ) ), 6, 0 );
    // The Moon's upper transits for the tide: yesterday to the day after tomorrow, as the almanac
    // gives them, a lunar day (24 h 20 min to 25 h 30 min) apart.
    for ( int i = 0; i < Tide::TransitCount; ++i ) {
        int64_t expected = 0;
        const bool has = Almanac::meridianPassage( Almanac::Moon, june.localMidnight + ( i - 1 ) * 86400LL,
                                                   istanbul.longitude, expected );
        check( "moon transit as the almanac's", static_cast<double>( june.moonTransits[i] ),
               has ? static_cast<double>( expected ) : 0.0, 0 );
        if ( i > 0 && june.moonTransits[i] != 0 && june.moonTransits[i - 1] != 0 ) {
            const double gap = static_cast<double>( june.moonTransits[i] - june.moonTransits[i - 1] );
            check( "moon transits a lunar day apart", gap, ( 87600.0 + 91800.0 ) / 2, ( 91800.0 - 87600.0 ) / 2 );
        }
    }
    printf( "Sky time page: %d checks, %d failures\n", checks, failures );
    return failures == 0 ? 0 : 1;
}
