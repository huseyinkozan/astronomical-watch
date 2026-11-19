#pragma once

#include <cstdint>

#include "Astronomy.h"
#include "Zones.h"

// The alarms' rules (docs/superpowers/specs/2026-11-19-alarms-design.md): a definition (a clock
// time or a sky event with an offset, the weekdays) gives its next due time from the place and
// the zone. Plain C++ for the host tests (tests/AlarmsTest.cpp); the event times come from
// Astronomy and Sky.
class Alarms
{
public:
    static const int MaxAlarms = 4;
    // A due time found this late still rings; later it is skipped.
    static const int LateSeconds = 600;
    // Days searched for an event; none within them: no due time (searched again the next day).
    static const int SearchDays = 60;
    static const int MaxOffsetMinutes = 120;

    enum Kind : uint8_t
    {
        ClockTime,
        SkyEvent
    };

    enum Event : uint8_t
    {
        Sunrise,
        Sunset,
        CivilDawn,
        CivilDusk,
        NauticalDawn,
        NauticalDusk,
        AstronomicalDawn,
        AstronomicalDusk,
        SolarNoon,
        Moonrise,
        Moonset,
        Imsak,
        PrayerSunrise,
        Dhuhr,
        Asr,
        Maghrib,
        Isha,
        EventCount
    };

    struct Definition
    {
        uint8_t kind;           // Kind
        uint8_t hour;           // ClockTime
        uint8_t minute;
        uint8_t event;          // SkyEvent: Event
        int16_t offsetMinutes;  // SkyEvent: -120 to +120
        uint8_t days;           // bit 0 Monday ... bit 6 Sunday: the days it rings on; 0: once
        bool on;
    };

    struct Place
    {
        Astronomy::Observer observer;
        Zone zone;
        int ishaAngle;
    };

    // The first due time (Unix seconds, a whole minute) after `after`; 0 when the alarm is off
    // or nothing is due within SearchDays.
    static int64_t next( const Definition &alarm, int64_t after, const Place &place );

    enum Due
    {
        Wait,
        Ring,
        Skip
    };

    // What to do with `due` at `now` (0: nothing due, Wait).
    static Due check( int64_t due, int64_t now );

    // Where a plan searches from (next()'s `after`): LateSeconds back, so that a due time missed by
    // a reset still rings, but not before `handled`, the last due time rung or skipped, or the
    // moment the alarm was switched on or saved (so that an alarm switched on just after its
    // time waits for the next one). A handled time more than a day ahead (the clock was
    // corrected back) no longer counts.
    static int64_t planFrom( int64_t now, int64_t handled );

    // Seconds of deep sleep before `due`: early by 2 s and 0.2 % of the sleep (the RC timer
    // drifts seconds a day; the PCF85063 gives the time at the wake), at most an hour (the
    // battery check), 3600 without a due time; 0 only when it is due within 3 s (active mode then
    // rings it), never otherwise.
    static int64_t sleepBefore( int64_t due, int64_t now );

    // A checksum of the place: a cached due time holds only for the key it was computed with.
    static uint32_t placeKey( const Place &place );

    // The setup portal's text: "kind|hour|minute|event|offset|days|on". decode() rejects a text
    // with a value out of range and leaves `alarm` as it was.
    static bool encode( const Definition &alarm, char *text, int size );
    static bool decode( const char *text, Definition &alarm );
};
