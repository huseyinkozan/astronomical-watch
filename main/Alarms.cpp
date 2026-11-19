#include "Alarms.h"

#include <cstdio>

#include "Dates.h"
#include "Sky.h"

static const int64_t s_day = 86400;

// Floor division: local days before 1970 stay correct.
static int localDay( int64_t utc, int offsetMinutes )
{
    const int64_t local = utc + static_cast<int64_t>( offsetMinutes ) * 60;
    return static_cast<int>( local >= 0 ? local / s_day : ( local - s_day + 1 ) / s_day );
}

// The event of local day `day` (Unix seconds), false when it does not occur that day.
static bool eventTime( Alarms::Event event, int day, const Alarms::Place &place, int64_t &time )
{
    // The day's midnight at the offset of that moment (a DST change happens after midnight).
    const int64_t guess = static_cast<int64_t>( day ) * s_day - static_cast<int64_t>( place.zone.offset ) * 60;
    const int64_t midnight = static_cast<int64_t>( day ) * s_day - static_cast<int64_t>( Zones::offsetAt( place.zone, guess ) ) * 60;
    Astronomy::Event found = {};
    if ( event <= Alarms::SolarNoon ) {
        const Astronomy::SunDay sun = Astronomy::sunDay( midnight, place.observer );
        const Astronomy::Crossings *crossings[4] = { &sun.riseSet, &sun.civil, &sun.nautical, &sun.astronomical };
        if ( event == Alarms::SolarNoon ) {
            found = sun.noon;
        } else {
            const Astronomy::Crossings &c = *crossings[event / 2];
            found = event % 2 == 0 ? c.rising : c.setting;
        }
    } else if ( event <= Alarms::Moonset ) {
        const Astronomy::Crossings moon = Astronomy::moonDay( midnight, place.observer );
        found = event == Alarms::Moonrise ? moon.rising : moon.setting;
    } else {
        const Sky::PrayerTimes prayers = Sky::prayerTimes( day, place.observer, place.ishaAngle );
        const int prayer = event - Alarms::Imsak;
        found.occurs = prayers.valid[prayer];
        found.time = prayers.times[prayer];
    }
    time = ( found.time + 30 ) / 60 * 60;
    return found.occurs;
}

// The clock time of local day `day` at the zone's offset of that moment.
static int64_t clockTime( const Alarms::Definition &alarm, int day, const Zone &zone )
{
    const int64_t local = static_cast<int64_t>( day ) * s_day + alarm.hour * 3600 + alarm.minute * 60;
    const int64_t guess = local - static_cast<int64_t>( Zones::offsetAt( zone, local - zone.offset * 60 ) ) * 60;
    return local - static_cast<int64_t>( Zones::offsetAt( zone, guess ) ) * 60;
}

int64_t Alarms::next( const Definition &alarm, int64_t after, const Place &place )
{
    if ( !alarm.on || alarm.kind > SkyEvent || ( alarm.kind == SkyEvent && alarm.event >= EventCount ) ) {
        return 0;
    }
    const int first = localDay( after, Zones::offsetAt( place.zone, after ) );
    // From the day before: a negative offset can bring that day's event after `after`.
    for ( int day = first - 1; day <= first + SearchDays; ++day ) {
        int64_t due = 0;
        if ( alarm.kind == ClockTime ) {
            due = clockTime( alarm, day, place.zone );
        } else {
            if ( !eventTime( static_cast<Event>( alarm.event ), day, place, due ) ) {
                continue;
            }
            due += static_cast<int64_t>( alarm.offsetMinutes ) * 60;
        }
        if ( due <= after ) {
            continue;
        }
        // The weekday of the ringing moment, not of the event.
        const int weekday = Dates::weekday( localDay( due, Zones::offsetAt( place.zone, due ) ) );
        if ( alarm.days == 0 || ( alarm.days & ( 1 << weekday ) ) != 0 ) {
            return due;
        }
    }
    return 0;
}

int64_t Alarms::planFrom( int64_t now, int64_t handled )
{
    const int64_t lookBack = now - LateSeconds - 1;
    if ( handled > lookBack && handled <= now + s_day ) {
        return handled;
    }
    return lookBack;
}

int64_t Alarms::sleepBefore( int64_t due, int64_t now )
{
    if ( due == 0 ) {
        return 3600;
    }
    const int64_t left = due - now;
    if ( left <= 3 ) {
        return 0;
    }
    const int64_t sleep = left - 2 - left / 500;
    if ( sleep < 1 ) {
        return 1;
    }
    return sleep < 3600 ? sleep : 3600;
}

Alarms::Due Alarms::check( int64_t due, int64_t now )
{
    if ( due == 0 || now < due ) {
        return Wait;
    }
    return now - due <= LateSeconds ? Ring : Skip;
}

// FNV-1a over the bytes of the inputs.
static uint32_t mix( uint32_t hash, const void *data, int size )
{
    const uint8_t *bytes = static_cast<const uint8_t *>( data );
    for ( int i = 0; i < size; ++i ) {
        hash = ( hash ^ bytes[i] ) * 16777619u;
    }
    return hash;
}

uint32_t Alarms::placeKey( const Place &place )
{
    uint32_t hash = 2166136261u;
    hash = mix( hash, &place.observer.latitude, sizeof( place.observer.latitude ) );
    hash = mix( hash, &place.observer.longitude, sizeof( place.observer.longitude ) );
    hash = mix( hash, &place.zone.offset, sizeof( place.zone.offset ) );
    hash = mix( hash, place.zone.changeAt, sizeof( place.zone.changeAt ) );
    hash = mix( hash, place.zone.offsetAfter, sizeof( place.zone.offsetAfter ) );
    hash = mix( hash, &place.ishaAngle, sizeof( place.ishaAngle ) );
    return hash;
}

bool Alarms::encode( const Definition &alarm, char *text, int size )
{
    const int written = snprintf( text, size, "%d|%d|%d|%d|%d|%d|%d", alarm.kind, alarm.hour, alarm.minute, alarm.event,
                                  alarm.offsetMinutes, alarm.days, alarm.on ? 1 : 0 );
    return written > 0 && written < size;
}

bool Alarms::decode( const char *text, Definition &alarm )
{
    int v[7] = {};
    char end = 0;
    if ( sscanf( text, "%d|%d|%d|%d|%d|%d|%d%c", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &end ) != 7 ) {
        return false;
    }
    if ( v[0] < 0 || v[0] > SkyEvent || v[1] < 0 || v[1] > 23 || v[2] < 0 || v[2] > 59 || v[3] < 0 ||
         v[3] >= EventCount || v[4] < -MaxOffsetMinutes || v[4] > MaxOffsetMinutes || v[5] < 0 || v[5] > 0x7F ||
         v[6] < 0 || v[6] > 1 ) {
        return false;
    }
    alarm.kind = static_cast<uint8_t>( v[0] );
    alarm.hour = static_cast<uint8_t>( v[1] );
    alarm.minute = static_cast<uint8_t>( v[2] );
    alarm.event = static_cast<uint8_t>( v[3] );
    alarm.offsetMinutes = static_cast<int16_t>( v[4] );
    alarm.days = static_cast<uint8_t>( v[5] );
    alarm.on = v[6] != 0;
    return true;
}
