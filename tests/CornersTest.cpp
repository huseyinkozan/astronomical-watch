// Host test of main/Corners.cpp: the corner gauges at two moments in Istanbul (UTC+3), against
// rise and set times from Skyfield (DE421; the watch's own times differ by up to a minute or two).
// Build and run: sh tests/run_host_tests.sh

#include <cstdio>
#include <cstdlib>

#include "Corners.h"

// Days since 1970-01-01 of a Gregorian date (H. Hinnant's days_from_civil).
static int dayNumber( int year, int month, int day )
{
    year -= month <= 2;
    const int era = ( year >= 0 ? year : year - 399 ) / 400;
    const int yearOfEra = year - era * 400;
    const int dayOfYear = ( 153 * ( month + ( month > 2 ? -3 : 9 ) ) + 2 ) / 5 + day - 1;
    const int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

static int s_failures = 0;
static int s_checks = 0;

// `expected` in thousandths; 5 thousandths is about one pixel of the gauge.
static void check( const char *name, const Corners::Gauge &gauge, Corners::Kind kind, int start, int end )
{
    ++s_checks;
    const int tolerance = 5;
    if ( gauge.kind != kind || std::abs( gauge.start - start ) > tolerance || std::abs( gauge.end - end ) > tolerance ) {
        ++s_failures;
        printf( "FAIL %s: kind %d, %d to %d; expected kind %d, %d to %d\n", name, gauge.kind, gauge.start, gauge.end,
                kind, start, end );
    }
}

int main()
{
    const int offsetMinutes = 180;
    const Astronomy::Observer istanbul = { 41.0082, 28.9784 };

    // Sunday 2026-06-21 19:19: Sun 05:33 to 20:38 (Skyfield), so 0.912 of the day has passed; Moon
    // 12:40 to 00:51 the next night, 0.546 of its pass; 45 % lit at local noon, waxing.
    int day = dayNumber( 2026, 6, 21 );
    Sky::Day sky = Sky::computeDay( day, offsetMinutes, istanbul, Hijri::Diyanet );
    int64_t now = static_cast<int64_t>( day ) * 86400 + ( 19 * 60 + 19 - offsetMinutes ) * 60;
    Corners::Gauge gauges[Corners::CornerCount] = {};
    Corners::compute( Corners::fromSky( sky ), now, 92, gauges );
    check( "day 19:19", gauges[Corners::TopLeft], Corners::Day, 0, 912 );
    check( "battery 92 %", gauges[Corners::TopRight], Corners::BatteryHigh, 0, 920 );
    check( "Moon up 19:19", gauges[Corners::BottomLeft], Corners::MoonUp, 0, 546 );
    const int lit = static_cast<int>( sky.phase.illumination * 100 + 0.5 ) * 10;
    check( "phase 21 June", gauges[Corners::BottomRight], Corners::MoonLit, 0, lit );

    // Monday 2026-06-22 03:00: night from 20:38 to 05:33, 0.712 passed; the Moon set at 00:51 and
    // rises at 13:45, 0.167 of its time below the horizon.
    day = dayNumber( 2026, 6, 22 );
    sky = Sky::computeDay( day, offsetMinutes, istanbul, Hijri::Diyanet );
    now = static_cast<int64_t>( day ) * 86400 + ( 3 * 60 - offsetMinutes ) * 60;
    Corners::compute( Corners::fromSky( sky ), now, 8, gauges );
    check( "night 03:00", gauges[Corners::TopLeft], Corners::Night, 0, 712 );
    check( "battery 8 %", gauges[Corners::TopRight], Corners::BatteryLow, 0, 80 );
    check( "Moon down 03:00", gauges[Corners::BottomLeft], Corners::MoonDown, 0, 167 );

    // A waning Moon: the lit part ends at the upper end.
    Corners::Input input = {};
    input.valid = true;
    input.moonPercent = 30;
    input.waxing = false;
    Corners::compute( input, now, -1, gauges );
    check( "waning phase", gauges[Corners::BottomRight], Corners::MoonLit, 700, 1000 );
    check( "battery unknown", gauges[Corners::TopRight], Corners::Empty, 0, 0 );
    check( "no events", gauges[Corners::TopLeft], Corners::Empty, 0, 0 );

    printf( "Corners: %d checks, %d failures\n", s_checks, s_failures );
    return s_failures == 0 ? 0 : 1;
}
