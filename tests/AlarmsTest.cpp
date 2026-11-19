// Host test of main/Alarms.cpp: the rules that turn an alarm's definition into its due time.
// The event times themselves are tested against Skyfield in AstronomyTest and PrayerTest; here
// the expected values come from the same functions. Build and run: sh tests/run_host_tests.sh

#include <cstdio>
#include <cstring>

#include "AlarmIcons.h"
#include "Alarms.h"
#include "Dates.h"
#include "Sky.h"

static int s_checks = 0;
static int s_failures = 0;

static void check( const char *name, bool condition )
{
    ++s_checks;
    if ( !condition ) {
        printf( "FAIL %s\n", name );
        ++s_failures;
    }
}

static const int64_t s_day = 86400;

// Unix time of a local date and time at a fixed offset (minutes).
static int64_t localTime( int year, int month, int day, int hour, int minute, int offset )
{
    return static_cast<int64_t>( Dates::dayNumber( year, month, day ) ) * s_day + hour * 3600 + minute * 60 - offset * 60;
}

static int64_t roundMinute( int64_t time )
{
    return ( time + 30 ) / 60 * 60;
}

static Alarms::Definition clockAlarm( int hour, int minute, int days )
{
    Alarms::Definition alarm = {};
    alarm.kind = Alarms::ClockTime;
    alarm.hour = static_cast<uint8_t>( hour );
    alarm.minute = static_cast<uint8_t>( minute );
    alarm.days = static_cast<uint8_t>( days );
    alarm.on = true;
    return alarm;
}

static Alarms::Definition eventAlarm( Alarms::Event event, int offset, int days )
{
    Alarms::Definition alarm = {};
    alarm.kind = Alarms::SkyEvent;
    alarm.event = static_cast<uint8_t>( event );
    alarm.offsetMinutes = static_cast<int16_t>( offset );
    alarm.days = static_cast<uint8_t>( days );
    alarm.on = true;
    return alarm;
}

int main()
{
    Alarms::Place istanbul = {};
    istanbul.observer = { 41.0082, 28.9784 };
    istanbul.zone = { 180, { 0, 0 }, { 0, 0 } };
    istanbul.ishaAngle = 17;
    const int tr = 180;

    // Clock time on weekdays: Friday 2026-11-20 08:00 -> Monday 07:00.
    const Alarms::Definition weekdays = clockAlarm( 7, 0, 0x1F );
    check( "clockWeekdays", Alarms::next( weekdays, localTime( 2026, 11, 20, 8, 0, tr ), istanbul ) ==
                                localTime( 2026, 11, 23, 7, 0, tr ) );
    check( "clockWeekdays same day", Alarms::next( weekdays, localTime( 2026, 11, 20, 6, 0, tr ), istanbul ) ==
                                         localTime( 2026, 11, 20, 7, 0, tr ) );
    // Once: today if still to come, else tomorrow.
    const Alarms::Definition once = clockAlarm( 7, 0, 0 );
    check( "clockOnce today", Alarms::next( once, localTime( 2026, 11, 22, 6, 0, tr ), istanbul ) ==
                                  localTime( 2026, 11, 22, 7, 0, tr ) );
    check( "clockOnce tomorrow", Alarms::next( once, localTime( 2026, 11, 22, 8, 0, tr ), istanbul ) ==
                                     localTime( 2026, 11, 23, 7, 0, tr ) );
    Alarms::Definition off = weekdays;
    off.on = false;
    check( "off", Alarms::next( off, localTime( 2026, 11, 20, 6, 0, tr ), istanbul ) == 0 );

    // Events equal the astronomy and prayer functions, rounded to the minute.
    const int day = Dates::dayNumber( 2026, 11, 20 );
    const int64_t midnight = static_cast<int64_t>( day ) * s_day - tr * 60;
    const Astronomy::SunDay sun = Astronomy::sunDay( midnight, istanbul.observer );
    const int64_t morning = localTime( 2026, 11, 20, 0, 1, tr );
    check( "sunset", Alarms::next( eventAlarm( Alarms::Sunset, 0, 0x7F ), morning, istanbul ) ==
                         roundMinute( sun.riseSet.setting.time ) );
    check( "civil dawn -15", Alarms::next( eventAlarm( Alarms::CivilDawn, -15, 0x7F ), morning, istanbul ) ==
                                 roundMinute( sun.civil.rising.time ) - 15 * 60 );
    check( "solar noon", Alarms::next( eventAlarm( Alarms::SolarNoon, 0, 0x7F ), morning, istanbul ) ==
                             roundMinute( sun.noon.time ) );
    const Sky::PrayerTimes prayers = Sky::prayerTimes( day, istanbul.observer, 17 );
    check( "maghrib", Alarms::next( eventAlarm( Alarms::Maghrib, 0, 0x7F ), morning, istanbul ) ==
                          roundMinute( prayers.times[Sky::Maghrib] ) );
    check( "isha +10", Alarms::next( eventAlarm( Alarms::Isha, 10, 0x7F ), morning, istanbul ) ==
                           roundMinute( prayers.times[Sky::Isha] ) + 600 );

    // The weekday is the ringing day: find a Tuesday whose moonrise is between 00:00 and 00:59;
    // "moonrise -60, Monday only" rings on the Monday before, at 23:xx.
    bool found = false;
    for ( int d = Dates::dayNumber( 2026, 11, 1 ); d < Dates::dayNumber( 2028, 1, 1 ) && !found; ++d ) {
        if ( Dates::weekday( d ) != 1 ) {
            continue;
        }
        const Astronomy::Crossings moon = Astronomy::moonDay( static_cast<int64_t>( d ) * s_day - tr * 60, istanbul.observer );
        const int64_t local = moon.rising.time + tr * 60 - static_cast<int64_t>( d ) * s_day;
        if ( !moon.rising.occurs || local < 0 || local >= 3600 - 30 ) {
            continue;
        }
        found = true;
        const int64_t mondayMorning = static_cast<int64_t>( d - 1 ) * s_day - tr * 60;
        const int64_t due = Alarms::next( eventAlarm( Alarms::Moonrise, -60, 0x01 ), mondayMorning, istanbul );
        check( "ringingDayRule time", due == roundMinute( moon.rising.time ) - 3600 );
        check( "ringingDayRule Monday", Dates::weekday( static_cast<int>( ( due + tr * 60 ) / s_day ) ) == 0 );
        const int64_t sunday = Alarms::next( eventAlarm( Alarms::Moonrise, -60, 0x40 ), mondayMorning - s_day, istanbul );
        check( "ringingDayRule Sunday only rings on a Sunday", sunday != 0 && Dates::weekday( static_cast<int>( ( sunday + tr * 60 ) / s_day ) ) == 6 );
    }
    check( "ringingDayRule case found", found );

    // A day without a moonrise moves to the next day that has one.
    found = false;
    for ( int d = Dates::dayNumber( 2026, 11, 1 ); d < Dates::dayNumber( 2026, 12, 31 ) && !found; ++d ) {
        const int64_t dayStart = static_cast<int64_t>( d ) * s_day - tr * 60;
        if ( Astronomy::moonDay( dayStart, istanbul.observer ).rising.occurs ) {
            continue;
        }
        found = true;
        const Astronomy::Crossings nextMoon = Astronomy::moonDay( dayStart + s_day, istanbul.observer );
        check( "missingEvent", nextMoon.rising.occurs &&
                                   Alarms::next( eventAlarm( Alarms::Moonrise, 0, 0x7F ), dayStart, istanbul ) ==
                                       roundMinute( nextMoon.rising.time ) );
    }
    check( "missingEvent case found", found );

    // Polar: Tromso's sunrise after the polar night (within the 60 days searched), and no
    // astronomical dusk at all in its summer.
    Alarms::Place tromso = {};
    tromso.observer = { 69.65, 18.96 };
    tromso.zone = { 60, { 0, 0 }, { 0, 0 } };
    tromso.ishaAngle = 17;
    const int64_t polar = Alarms::next( eventAlarm( Alarms::Sunrise, 0, 0x7F ), localTime( 2026, 12, 15, 0, 0, 60 ), tromso );
    const int polarDay = static_cast<int>( ( polar + 60 * 60 ) / s_day );
    check( "polar sunrise in January", polar != 0 && polarDay >= Dates::dayNumber( 2027, 1, 5 ) &&
                                           polarDay <= Dates::dayNumber( 2027, 1, 25 ) );
    check( "polar sunrise is that day's", polar == roundMinute( Astronomy::sunDay( static_cast<int64_t>( polarDay ) * s_day - 3600,
                                                                                    tromso.observer ).riseSet.rising.time ) );
    check( "polar no dusk", Alarms::next( eventAlarm( Alarms::AstronomicalDusk, 0, 0x7F ), localTime( 2026, 5, 15, 0, 0, 60 ),
                                          tromso ) == 0 );

    // DST: a zone going from +60 to +120 at 2027-03-28 01:00 UTC; a daily 03:00 alarm rings at
    // 03:00 of the new offset (01:00 UTC), once.
    Alarms::Place europe = istanbul;
    const int spring = 1806195600 / 60;
    europe.zone = { 60, { spring, 0 }, { 120, 0 } };
    const int64_t dst = Alarms::next( clockAlarm( 3, 0, 0x7F ), localTime( 2027, 3, 27, 12, 0, 60 ), europe );
    check( "dstChange", dst == 1806195600 );
    check( "dstChange next day", Alarms::next( clockAlarm( 3, 0, 0x7F ), dst, europe ) == dst + s_day );

    // Late due times and a rung alarm.
    const int64_t due = localTime( 2026, 11, 23, 7, 0, tr );
    check( "wait", Alarms::check( due, due - 1 ) == Alarms::Wait );
    check( "ring", Alarms::check( due, due ) == Alarms::Ring );
    check( "ring late", Alarms::check( due, due + Alarms::LateSeconds ) == Alarms::Ring );
    check( "skip", Alarms::check( due, due + Alarms::LateSeconds + 1 ) == Alarms::Skip );
    check( "no due waits", Alarms::check( 0, due ) == Alarms::Wait );
    // A due time already handled (rung or skipped) is never due again, whatever the clock says
    // (a display that failed must not bring the same alarm back at every wake).
    check( "handled waits", Alarms::check( due, due + 5, due ) == Alarms::Wait );
    check( "handled late waits", Alarms::check( due, due + Alarms::LateSeconds + 60, due ) == Alarms::Wait );
    check( "other handled rings", Alarms::check( due, due + 5, due - 86400 ) == Alarms::Ring );
    check( "rung not found again", Alarms::next( weekdays, due, istanbul ) > due );

    // Where a plan searches from: 10 min back after a boot, but not before the last handled time;
    // a handled time more than a day ahead (a clock corrected back) no longer counts.
    const int64_t now = due + 3600;
    check( "planFrom look-back", Alarms::planFrom( now, 0 ) == now - Alarms::LateSeconds - 1 );
    check( "planFrom handled", Alarms::planFrom( now, now - 60 ) == now - 60 );
    check( "planFrom clock corrected back", Alarms::planFrom( now, now + 2 * 86400 ) == now - Alarms::LateSeconds - 1 );
    // An alarm switched on (or saved) at 07:05 does not ring for 07:00: the switch counts as
    // handled at that moment.
    check( "switched on after its time", Alarms::next( weekdays, Alarms::planFrom( due + 300, due + 300 ), istanbul ) > due + 300 );
    // The sleep before a due time: never 0 unless it is due within 3 s, at most an hour.
    check( "sleepBefore none", Alarms::sleepBefore( 0, now ) == 3600 );
    check( "sleepBefore due now", Alarms::sleepBefore( now + 3, now ) == 0 );
    check( "sleepBefore past", Alarms::sleepBefore( now - 100, now ) == 0 );
    check( "sleepBefore early", Alarms::sleepBefore( now + 1000, now ) == 1000 - 2 - 2 );
    check( "sleepBefore at least 1", Alarms::sleepBefore( now + 4, now ) >= 1 );
    check( "sleepBefore hour", Alarms::sleepBefore( now + 86400, now ) == 3600 );

    // The cache key follows every input.
    const uint32_t key = Alarms::placeKey( istanbul );
    Alarms::Place changed = istanbul;
    changed.observer.latitude += 0.0001;
    check( "key latitude", Alarms::placeKey( changed ) != key );
    changed = istanbul;
    changed.observer.longitude -= 0.0001;
    check( "key longitude", Alarms::placeKey( changed ) != key );
    changed = istanbul;
    changed.zone.offset = 120;
    check( "key offset", Alarms::placeKey( changed ) != key );
    changed = istanbul;
    changed.zone.changeAt[0] = spring;
    check( "key zone change", Alarms::placeKey( changed ) != key );
    changed = istanbul;
    changed.ishaAngle = 16;
    check( "key isha", Alarms::placeKey( changed ) != key );

    // The portal's text.
    char text[48];
    Alarms::Definition back = {};
    const Alarms::Definition sample = eventAlarm( Alarms::Isha, -120, 0x55 );
    check( "encode", Alarms::encode( sample, text, sizeof( text ) ) );
    check( "decode", Alarms::decode( text, back ) && back.kind == sample.kind && back.event == sample.event &&
                         back.offsetMinutes == -120 && back.days == 0x55 && back.on );
    check( "decode clock", Alarms::decode( "0|7|30|0|0|31|1", back ) && back.hour == 7 && back.minute == 30 &&
                               back.days == 31 && back.on );
    // The texts the setup page sends (tools/portal_preview.py's state, the page's v.join('|')).
    check( "page clock", Alarms::decode( "0|7|0|0|0|31|1", back ) && back.kind == Alarms::ClockTime && back.days == 31 );
    check( "page event", Alarms::decode( "1|0|0|1|-10|127|1", back ) && back.event == Alarms::Sunset &&
                             back.offsetMinutes == -10 && back.days == 127 );
    check( "reject hour", !Alarms::decode( "0|24|0|0|0|31|1", back ) );
    check( "reject offset", !Alarms::decode( "1|0|0|3|121|31|1", back ) );
    check( "reject event", !Alarms::decode( "1|0|0|17|0|31|1", back ) );
    check( "reject fields", !Alarms::decode( "0|7|30|0|0|31", back ) );

    // The corner icons are mirror images left to right.
    for ( int icon = 0; icon < AlarmIcons::IconCount; ++icon ) {
        const AlarmIcons::Icon i = static_cast<AlarmIcons::Icon>( icon );
        bool symmetric = AlarmIcons::width( i ) > 0 && AlarmIcons::height( i ) > 0;
        for ( int y = 0; y < AlarmIcons::height( i ); ++y ) {
            for ( int x = 0; x < AlarmIcons::width( i ); ++x ) {
                symmetric = symmetric && AlarmIcons::pixel( i, x, y ) == AlarmIcons::pixel( i, AlarmIcons::width( i ) - 1 - x, y );
            }
        }
        check( "icon symmetric", symmetric );
    }
    check( "hourglass has sand", AlarmIcons::pixel( AlarmIcons::Hourglass, 14, 21 ) == AlarmIcons::Sand );

    printf( "Alarms: %d checks, %d failures\n", s_checks, s_failures );
    return s_failures == 0 ? 0 : 1;
}
