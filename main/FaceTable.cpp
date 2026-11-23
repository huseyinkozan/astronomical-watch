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
    eventsDay = today.day;
    localDay = today.day;
    validUntil = today.localMidnight + 2 * s_dayS;
    hijriDay = today.hijri.day;
    hijriMonth = today.hijri.month;
    hijriYear = today.hijri.year;
    addDay( *this, today.sun.riseSet, today.prayers );
    const Astronomy::SunDay tomorrow = Astronomy::sunDay( today.localMidnight + s_dayS, observer );
    addDay( *this, tomorrow.riseSet, Sky::prayerTimes( today.day + 1, observer, ishaAngle ) );
}

void FaceTable::follow( int day, int utcOffsetMinutes, const Astronomy::Observer &observer, int ishaAngle )
{
    if ( day == eventsDay ) {
        return;
    }
    count = 0;
    eventsDay = day;
    const int64_t localMidnight = static_cast<int64_t>( day ) * s_dayS - static_cast<int64_t>( utcOffsetMinutes ) * 60;
    validUntil = localMidnight + 2 * s_dayS;
    for ( int i = 0; i < 2; ++i ) {
        const Astronomy::SunDay sun = Astronomy::sunDay( localMidnight + i * s_dayS, observer );
        addDay( *this, sun.riseSet, Sky::prayerTimes( day + i, observer, ishaAngle ) );
    }
    // Every Hijri month has at least 29 days.
    const int days = day - localDay;
    if ( hijriDay > 0 && days > 0 && hijriDay + days <= 29 ) {
        hijriDay += days;
    } else {
        hijriDay = 0;
    }
    localDay = day;
}

void FaceTable::setCounter( int index, const char *name, int day, int savedDay )
{
    if ( index < 0 || index >= CounterCount ) {
        return;
    }
    snprintf( counterNames[index], NameLength, "%s", name );
    counterDays[index] = day;
    counterSavedDays[index] = savedDay;
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

Tide::State FaceTable::tide( int64_t now ) const
{
    return Tide::at( moonTransits, tideInterval, moonAge, now );
}
