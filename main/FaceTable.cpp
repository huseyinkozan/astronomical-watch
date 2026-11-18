#include "FaceTable.h"

#include <cstdio>

static const int64_t s_dayS = 86400;

static void add( FaceTable &table, int64_t time, uint8_t kind )
{
    if ( table.count >= FaceTable::MaxEvents ) {
        return;
    }
    // In time order: few events, insertion is enough.
    int at = table.count;
    while ( at > 0 && table.events[at - 1].time > time ) {
        table.events[at] = table.events[at - 1];
        --at;
    }
    table.events[at] = { time, kind };
    ++table.count;
}

static void addDay( FaceTable &table, const Astronomy::Crossings &riseSet, const Sky::PrayerTimes &prayers )
{
    if ( riseSet.rising.occurs ) {
        add( table, riseSet.rising.time, FaceTable::SunriseEvent );
    }
    if ( riseSet.setting.occurs ) {
        add( table, riseSet.setting.time, FaceTable::SunsetEvent );
    }
    for ( int i = 0; i < Sky::PrayerCount; ++i ) {
        if ( prayers.valid[i] ) {
            add( table, prayers.times[i], static_cast<uint8_t>( FaceTable::PrayerEvent + i ) );
        }
    }
}

void FaceTable::build( const Sky::Day &today, int utcOffsetMinutes, const Astronomy::Observer &observer, int ishaAngle )
{
    ( void ) utcOffsetMinutes;
    count = 0;
    localDay = today.day;
    validUntil = today.localMidnight + 2 * s_dayS;
    hijriDay = today.hijri.day;
    hijriMonth = today.hijri.month;
    hijriYear = today.hijri.year;
    addDay( *this, today.sun.riseSet, today.prayers );
    const Astronomy::SunDay tomorrow = Astronomy::sunDay( today.localMidnight + s_dayS, observer );
    addDay( *this, tomorrow.riseSet, Sky::prayerTimes( today.day + 1, observer, ishaAngle ) );
}

void FaceTable::setCounter( int index, const char *name, int day )
{
    if ( index < 0 || index >= CounterCount ) {
        return;
    }
    snprintf( counterNames[index], NameLength, "%s", name );
    counterDays[index] = day;
}

void FaceTable::setCity( int index, const char *name, const Zone &zone )
{
    if ( index < 0 || index >= CityCount ) {
        return;
    }
    snprintf( cityNames[index], NameLength, "%s", name );
    cityZones[index] = zone;
}

bool FaceTable::next( int64_t now, bool prayer, Event &event ) const
{
    if ( now >= validUntil ) {
        return false;
    }
    for ( int i = 0; i < count; ++i ) {
        const bool isPrayer = events[i].kind >= PrayerEvent;
        if ( events[i].time > now && isPrayer == prayer ) {
            event = events[i];
            return true;
        }
    }
    return false;
}
