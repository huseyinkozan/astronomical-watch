// Host test of main/Corners.cpp: the corner gauges at two moments in Istanbul (UTC+3), against
// rise and set times from Skyfield (DE421; the watch's own times differ by up to a minute or two).
// Build and run: sh tests/run_host_tests.sh

#include <cstdio>
#include <cstdlib>

#include "Corners.h"
#include "FaceConfig.h"

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

// The default four gauges at `now`: the day or night, the battery, the Moon's pass, its phase.
static void defaultGauges( const Corners::Input &input, int64_t now, int batteryPercent, bool charging,
                           Corners::Gauge gauges[Corners::CornerCount] )
{
    Corners::Context context = {};
    context.now = now;
    context.batteryPercent = batteryPercent;
    context.charging = charging;
    const uint8_t items[Corners::CornerCount] = { FaceConfig::DayNightGauge, FaceConfig::BatteryGauge,
                                                  FaceConfig::MoonPassGauge, FaceConfig::MoonPhaseGauge };
    const uint8_t parameters[Corners::CornerCount] = {};
    Corners::compute( input, context, items, parameters, gauges );
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
    defaultGauges( Corners::fromSky( sky ), now, 92, false, gauges );
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
    defaultGauges( Corners::fromSky( sky ), now, 8, true, gauges );
    check( "night 03:00", gauges[Corners::TopLeft], Corners::Night, 0, 712 );
    check( "battery 8 %", gauges[Corners::TopRight], Corners::BatteryLow, 0, 80 );
    check( "Moon down 03:00", gauges[Corners::BottomLeft], Corners::MoonDown, 0, 167 );
    // Charging marks only the battery gauge.
    ++s_checks;
    if ( !gauges[Corners::TopRight].charging || gauges[Corners::TopLeft].charging ||
         gauges[Corners::BottomLeft].charging || gauges[Corners::BottomRight].charging ) {
        ++s_failures;
        printf( "FAIL charging: only the battery gauge should be marked\n" );
    }

    // A waning Moon: the lit part ends at the upper end.
    Corners::Input input = {};
    input.valid = true;
    input.moonPercent = 30;
    input.waxing = false;
    defaultGauges( input, now, -1, true, gauges );
    check( "waning phase", gauges[Corners::BottomRight], Corners::MoonLit, 700, 1000 );
    check( "battery unknown", gauges[Corners::TopRight], Corners::Empty, 0, 0 );
    // An unknown battery is not drawn, so it does not charge either.
    ++s_checks;
    if ( gauges[Corners::TopRight].charging ) {
        ++s_failures;
        printf( "FAIL charging with the battery unknown\n" );
    }
    check( "no events", gauges[Corners::TopLeft], Corners::Empty, 0, 0 );

    // The arc templates' places: a pass from 06:00 to 18:00, down until 06:00 the next day.
    Corners::Events pass = {};
    pass.times[0] = 6 * 3600;
    pass.rising[0] = true;
    pass.times[1] = 18 * 3600;
    pass.rising[1] = false;
    pass.times[2] = 30 * 3600;
    pass.rising[2] = true;
    pass.count = 3;
    auto near = []( float a, float b ) { return a - b < 0.01f && b - a < 0.01f; };
    Corners::ArcPlace place = Corners::arcPlace( pass, 12 * 3600, true, false );
    ++s_checks;
    if ( !( place.known && place.up && near( place.angle, 270 ) && near( place.rise, 180 ) && near( place.set, 360 ) ) ) {
        ++s_failures;
        printf( "FAIL orbit at noon: angle %.1f rise %.1f set %.1f\n", place.angle, place.rise, place.set );
    }
    place = Corners::arcPlace( pass, 24 * 3600, true, false );
    ++s_checks;
    if ( !( place.known && !place.up && near( place.angle, 90 ) ) ) {
        ++s_failures;
        printf( "FAIL orbit at midnight: angle %.1f\n", place.angle );
    }
    place = Corners::arcPlace( pass, 9 * 3600, false, false );
    ++s_checks;
    if ( !( place.known && place.up && near( place.angle, 225 ) ) ) {
        ++s_failures;
        printf( "FAIL path at 09:00, north: angle %.1f\n", place.angle );
    }
    place = Corners::arcPlace( pass, 9 * 3600, false, true );
    ++s_checks;
    if ( !( place.known && near( place.angle, 315 ) ) ) {
        ++s_failures;
        printf( "FAIL path at 09:00, south: angle %.1f\n", place.angle );
    }
    // The polar day: no rise or set, never a division by zero.
    Corners::Events polar = {};
    polar.alwaysUp = true;
    place = Corners::arcPlace( polar, 12 * 3600, true, false );
    ++s_checks;
    if ( place.known || !place.alwaysUp ) {
        ++s_failures;
        printf( "FAIL polar day: known %d alwaysUp %d\n", place.known, place.alwaysUp );
    }

    // The next-sunset gauge around sunset on days whose sunsets differ from the day before
    // (yesterday's sunset a day later must not stand for today's).
    {
        const int sunsetDays[2] = { dayNumber( 2026, 11, 18 ), dayNumber( 2026, 3, 20 ) };
        for ( int d = 0; d < 2; ++d ) {
            const Sky::Day sunsetSky = Sky::computeDay( sunsetDays[d], offsetMinutes, istanbul, Hijri::Diyanet );
            const int64_t sunset = sunsetSky.sun.riseSet.setting.time;
            const uint8_t items[4] = { FaceConfig::NextSunsetGauge, FaceConfig::NextSunriseGauge, 0, 0 };
            const uint8_t parameters[4] = {};
            Corners::Context context = {};
            context.utcOffsetMinutes = offsetMinutes;
            Corners::Gauge g[Corners::CornerCount] = {};
            context.now = sunset - 60;
            Corners::compute( Corners::fromSky( sunsetSky ), context, items, parameters, g );
            check( d == 0 ? "next sunset a minute before it, 18 Nov" : "next sunset a minute before it, 20 Mar", g[0],
                   Corners::SunEvent, 0, 999 );
            context.now = sunset + 20;
            Corners::compute( Corners::fromSky( sunsetSky ), context, items, parameters, g );
            check( d == 0 ? "next sunset just after it, 18 Nov" : "next sunset just after it, 20 Mar", g[0],
                   Corners::SunEvent, 0, 0 );
            context.now = sunsetSky.sun.riseSet.rising.time - 60;
            Corners::compute( Corners::fromSky( sunsetSky ), context, items, parameters, g );
            check( d == 0 ? "next sunrise a minute before it, 18 Nov" : "next sunrise a minute before it, 20 Mar", g[1],
                   Corners::SunEvent, 0, 999 );
        }
    }

    // The chosen gauges (FaceConfig::GaugeItem) on 21 June 2026 in Istanbul (UTC+3).
    {
        const int today = dayNumber( 2026, 6, 21 );  // a Sunday
        auto at = [&]( int dayOffset, int hour, int minute ) {
            return static_cast<int64_t>( today + dayOffset ) * 86400 + ( hour * 60 + minute - offsetMinutes ) * 60;
        };
        Corners::Input sky = {};
        sky.valid = true;
        const int prayerMinutes[6] = { 3 * 60 + 30, 5 * 60 + 20, 13 * 60 + 10, 17 * 60, 20 * 60 + 40, 22 * 60 + 20 };
        for ( int i = 0; i < 6; ++i ) {
            sky.prayers[i] = at( 0, 0, prayerMinutes[i] );
            sky.prayerValid[i] = true;
        }
        const int sunTimes[4][2] = { { -1, 20 * 60 + 40 }, { 0, 5 * 60 + 32 }, { 0, 20 * 60 + 40 }, { 1, 5 * 60 + 32 } };
        for ( int i = 0; i < 4; ++i ) {
            sky.sun.times[i] = at( sunTimes[i][0], 0, sunTimes[i][1] );
            sky.sun.rising[i] = i % 2 == 1;
        }
        sky.sun.count = 4;
        const int moonTimes[4][2] = { { 0, 29 }, { 0, 12 * 60 + 40 }, { 1, 51 }, { 1, 13 * 60 + 45 } };
        for ( int i = 0; i < 4; ++i ) {
            sky.moon.times[i] = at( moonTimes[i][0], 0, moonTimes[i][1] );
            sky.moon.rising[i] = i % 2 == 1;
        }
        sky.moon.count = 4;
        Corners::Context context = {};
        context.utcOffsetMinutes = offsetMinutes;
        context.batteryPercent = 68;
        context.sunAltitude = 45;
        context.sunAltitudeValid = true;
        context.hijriDay = 16;
        context.counterDays[0] = today + 10;
        context.counterSavedDays[0] = today - 10;
        context.counterValid[0] = true;
        context.counterDays[1] = dayNumber( 2027, 2, 8 );
        context.counterValid[1] = true;  // saved by an older version: no saved day
        context.counterDays[2] = today - 1;
        context.counterSavedDays[2] = today - 40;
        context.counterValid[2] = true;
        context.counterDays[3] = dayNumber( 2024, 2, 29 );
        context.counterSavedDays[3] = today - 400;
        context.counterValid[3] = true;
        Corners::Gauge g[Corners::CornerCount] = {};
        auto one = [&]( int item, int parameter, int64_t now, bool alwaysOn = false ) {
            const uint8_t items[4] = { static_cast<uint8_t>( item ), 0, 0, 0 };
            const uint8_t parameters[4] = { static_cast<uint8_t>( parameter ), 0, 0, 0 };
            context.now = now;
            context.alwaysOn = alwaysOn;
            Corners::compute( sky, context, items, parameters, g );
            return g[0];
        };
        check( "prayer gauge after the night prayer", one( FaceConfig::NextPrayerGauge, 0, at( 0, 23, 20 ) ), Corners::PrayerEvent, 0, 193 );
        check( "prayer gauge before the first prayer", one( FaceConfig::NextPrayerGauge, 0, at( 0, 2, 0 ) ), Corners::PrayerEvent, 0, 709 );
        check( "week, Sunday noon", one( FaceConfig::WeekGauge, 0, at( 0, 12, 0 ) ), Corners::Calendar, 0, 928 );
        check( "month, 21 June noon", one( FaceConfig::MonthGauge, 0, at( 0, 12, 0 ) ), Corners::Calendar, 0, 683 );
        check( "Hijri month, day 16 noon", one( FaceConfig::HijriMonthGauge, 0, at( 0, 12, 0 ) ), Corners::HijriCalendar, 0, 516 );
        check( "year, 21 June noon", one( FaceConfig::YearGauge, 0, at( 0, 12, 0 ) ), Corners::Calendar, 0, 469 );
        check( "hour, 24 h at noon", one( FaceConfig::HourGauge, 0, at( 0, 12, 0 ) ), Corners::Clock, 0, 500 );
        check( "hour, 12 h at noon", one( FaceConfig::HourGauge, 1, at( 0, 12, 0 ) ), Corners::Clock, 0, 0 );
        check( "hour, 12 h at 18:00", one( FaceConfig::HourGauge, 1, at( 0, 18, 0 ) ), Corners::Clock, 0, 500 );
        check( "minute at 19:15", one( FaceConfig::MinuteGauge, 0, at( 0, 19, 15 ) ), Corners::Clock, 0, 250 );
        check( "second on the always-on face: empty", one( FaceConfig::SecondGauge, 0, at( 0, 19, 15 ) + 30, true ), Corners::Empty, 0, 0 );
        check( "second on the active face", one( FaceConfig::SecondGauge, 0, at( 0, 19, 15 ) + 30 ), Corners::Clock, 0, 500 );
        context.timeoutLeft = 600;
        check( "display timeout: the share left", one( FaceConfig::TimeoutGauge, 0, at( 0, 12, 0 ) ), Corners::Timeout, 0, 600 );
        check( "always-on: the power-off's share left", one( FaceConfig::TimeoutGauge, 0, at( 0, 12, 0 ), true ), Corners::Timeout, 0, 600 );
        context.timeoutLeft = 1400;
        check( "display timeout: at most full", one( FaceConfig::TimeoutGauge, 0, at( 0, 12, 0 ) ), Corners::Timeout, 0, 1000 );
        context.timeoutLeft = 401;
        check( "display timeout: above 40 % white", one( FaceConfig::TimeoutGauge, 0, at( 0, 12, 0 ) ), Corners::Timeout, 0, 401 );
        context.timeoutLeft = 400;
        check( "display timeout: 40 % orange", one( FaceConfig::TimeoutGauge, 0, at( 0, 12, 0 ) ), Corners::TimeoutMiddle, 0, 400 );
        context.timeoutLeft = 201;
        check( "display timeout: above 20 % orange", one( FaceConfig::TimeoutGauge, 0, at( 0, 12, 0 ) ), Corners::TimeoutMiddle, 0, 201 );
        context.timeoutLeft = 200;
        check( "display timeout: 20 % red", one( FaceConfig::TimeoutGauge, 0, at( 0, 12, 0 ) ), Corners::TimeoutLow, 0, 200 );
        context.timeoutLeft = -1;
        check( "always-on without a power-off: empty", one( FaceConfig::TimeoutGauge, 0, at( 0, 12, 0 ), true ), Corners::Empty, 0, 0 );
        const Corners::Gauge below = Corners::timeoutGauge( -20 );
        check( "timeout gauge: at least empty", below, Corners::TimeoutLow, 0, 0 );
        check( "sun altitude 45", one( FaceConfig::SunAltitudeGauge, 0, at( 0, 12, 0 ) ), Corners::Altitude, 0, 500 );
        context.sunAltitude = -5;
        check( "sun altitude at night: empty", one( FaceConfig::SunAltitudeGauge, 0, at( 0, 23, 0 ) ), Corners::Empty, 0, 0 );
        check( "counter since saved", one( FaceConfig::DayCounterGauge, 0 | ( FaceConfig::SinceSaved << 2 ), at( 0, 12, 0 ) ), Corners::Counter, 0, 525 );
        check( "counter, saved day unknown: empty", one( FaceConfig::DayCounterGauge, 1 | ( FaceConfig::SinceSaved << 2 ), at( 0, 12, 0 ) ), Corners::Empty, 0, 0 );
        // 8 February 2026 to 8 February 2027: 133.5 of 365 days.
        check( "counter yearly", one( FaceConfig::DayCounterGauge, 1 | ( FaceConfig::Yearly << 2 ), at( 0, 12, 0 ) ), Corners::Counter, 0, 365 );
        check( "counter, last 30 days", one( FaceConfig::DayCounterGauge, 0 | ( FaceConfig::Last30Days << 2 ), at( 0, 12, 0 ) ), Corners::Counter, 0, 683 );
        check( "counter, date passed: full", one( FaceConfig::DayCounterGauge, 2 | ( FaceConfig::Last100Days << 2 ), at( 0, 12, 0 ) ), Corners::Counter, 0, 1000 );
        const Corners::Gauge leap = one( FaceConfig::DayCounterGauge, 3 | ( FaceConfig::Yearly << 2 ), at( 0, 12, 0 ) );
        ++s_checks;
        if ( leap.kind != Corners::Counter || leap.end < 0 || leap.end > 1000 ) {
            ++s_failures;
            printf( "FAIL counter yearly on 29 February: kind %d end %d\n", leap.kind, leap.end );
        }
        // 05:32 today to 05:32 tomorrow: 6 h 28 min of 24 h at noon.
        check( "next sunrise at noon", one( FaceConfig::NextSunriseGauge, 0, at( 0, 12, 0 ) ), Corners::SunEvent, 0, 269 );
        check( "next sunset at noon", one( FaceConfig::NextSunsetGauge, 0, at( 0, 12, 0 ) ), Corners::SunEvent, 0, 638 );
        // 12:40 today to 13:45 tomorrow (25 h 5 min) at 19:00.
        check( "next moonrise at 19:00", one( FaceConfig::NextMoonriseGauge, 0, at( 0, 19, 0 ) ), Corners::MoonEvent, 0, 252 );
        Corners::Input polar = sky;
        polar.sun.count = 0;
        polar.sun.alwaysUp = true;
        const uint8_t items[4] = { FaceConfig::NextSunriseGauge, FaceConfig::DayNightGauge, FaceConfig::BatteryGauge, 0 };
        const uint8_t parameters[4] = {};
        context.now = at( 0, 12, 0 );
        Corners::compute( polar, context, items, parameters, g );
        check( "next sunrise in the polar day: empty", g[0], Corners::Empty, 0, 0 );
        check( "day gauge in the polar day: full", g[1], Corners::Day, 0, 1000 );
        check( "battery in any corner", g[2], Corners::BatteryHigh, 0, 680 );
        // Day or night for the always-on brightness, whatever the corners show.
        ++s_checks;
        if ( Corners::night( sky, at( 0, 12, 0 ) ) || !Corners::night( sky, at( 0, 23, 0 ) ) || Corners::night( polar, at( 0, 23, 0 ) ) ) {
            ++s_failures;
            printf( "FAIL night: noon %d, 23:00 %d, polar day %d\n", Corners::night( sky, at( 0, 12, 0 ) ),
                    Corners::night( sky, at( 0, 23, 0 ) ), Corners::night( polar, at( 0, 23, 0 ) ) );
        }
    }

    // Dark enough for the red night mode: the Sun below -6 degrees (civil twilight over). Istanbul
    // on 21 June 2026: sunset 20:38, civil dusk about 21:12, civil dawn about 05:00 (Skyfield).
    {
        const int today = dayNumber( 2026, 6, 21 );
        const Corners::Input sky = Corners::fromSky( Sky::computeDay( today, offsetMinutes, istanbul, Hijri::Diyanet ) );
        auto at = [&]( int dayOffset, int hour, int minute ) {
            return static_cast<int64_t>( today + dayOffset ) * 86400 + ( hour * 60 + minute - offsetMinutes ) * 60;
        };
        const struct
        {
            const char *name;
            int64_t time;
            bool dark;
        } cases[] = {
            { "dark: noon", at( 0, 12, 0 ), false },
            { "dark: after sunset, in civil twilight", at( 0, 20, 50 ), false },
            { "dark: after civil dusk", at( 0, 21, 20 ), true },
            { "dark: midnight", at( 1, 0, 30 ), true },
            { "dark: before civil dawn", at( 1, 4, 50 ), true },
            { "dark: after civil dawn", at( 1, 5, 10 ), false },
            { "dark: yesterday evening", at( -1, 21, 30 ), true },
        };
        for ( const auto &c : cases ) {
            ++s_checks;
            if ( Corners::dark( sky, c.time ) != c.dark ) {
                ++s_failures;
                printf( "FAIL %s: %d\n", c.name, Corners::dark( sky, c.time ) );
            }
        }
        // Light nights: the Sun stays above -6 degrees all night (polar summer), never dark.
        Corners::Input white = sky;
        white.civil.count = 0;
        white.civil.alwaysUp = true;
        ++s_checks;
        if ( Corners::dark( white, at( 1, 0, 30 ) ) ) {
            ++s_failures;
            printf( "FAIL dark: light night\n" );
        }
        ++s_checks;
        if ( Corners::dark( Corners::Input {}, at( 1, 0, 30 ) ) ) {
            ++s_failures;
            printf( "FAIL dark: without the sky\n" );
        }
    }

    printf( "Corners: %d checks, %d failures\n", s_checks, s_failures );
    return s_failures == 0 ? 0 : 1;
}
