#pragma once

#include <cstdint>

#include "Astronomy.h"
#include "Sky.h"
#include "Zones.h"

// What the face slots need beyond the clock, taken when the sky day is computed: today's and
// tomorrow's sunrise, sunset and prayer times, today's Hijri date, the day counters and the
// world clock cities. The always-on face keeps a copy in RTC memory (no pointers), so that the
// minute wakes show the next event without computing the sky day (about 0.9 s).
class FaceTable
{
public:
    // Kind of an event; a prayer is PrayerEvent + Sky::Prayer.
    enum Kind : uint8_t
    {
        SunriseEvent,
        SunsetEvent,
        PrayerEvent
    };

    struct Event
    {
        int64_t time;  // Unix time
        uint8_t kind;
    };

    static const int MaxEvents = 16;
    static const int CounterCount = 4;
    static const int CityCount = 3;
    static const int NameLength = 24;

    Event events[MaxEvents];
    int count;
    int64_t validUntil;  // the end of tomorrow: later events are not in the table
    int localDay;        // the day the table was built for (days since 1970-01-01, local)
    int hijriDay;
    int hijriMonth;
    int hijriYear;
    int counterDays[CounterCount];
    char counterNames[CounterCount][NameLength];  // empty: no counter
    Zone cityZones[CityCount];
    char cityNames[CityCount][NameLength];        // empty: no city

    // Tomorrow's sun and prayer times are computed here (Astronomy::sunDay, Sky::prayerTimes),
    // not with a whole Sky::computeDay.
    void build( const Sky::Day &today, int utcOffsetMinutes, const Astronomy::Observer &observer, int ishaAngle );

    void setCounter( int index, const char *name, int day );
    void setCity( int index, const char *name, const Zone &zone );

    // The first sunrise or sunset (`prayer` false), or prayer time (true), after `now`; false when
    // there is none in the table or `now` is past its end.
    bool next( int64_t now, bool prayer, Event &event ) const;
};
