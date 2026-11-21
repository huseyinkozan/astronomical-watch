#pragma once

#include "Hijri.h"

// The religious days and kandils of Diyanet's list on the Hijri calendar (the clock page's
// calendar). A kandil is the night before its Hijri date: it is given on the civil day of that
// evening, as Diyanet's calendar dates it (Berat Kandili 1447: 2 February 2026, the evening
// before 15 Shaban). Plain C++ for the host tests; days are local day numbers.
class HolyDays
{
public:
    enum Day
    {
        None,
        Regaip,          // the evening before the first Friday of Rajab
        Mirac,           // before 27 Rajab
        Berat,           // before 15 Shaban
        RamadanStart,    // 1 Ramadan
        Kadir,           // before 27 Ramadan
        RamazanBayrami,  // 1 to 3 Shawwal
        KurbanBayrami,   // 10 to 13 Dhu al-Hijjah
        NewYear,         // 1 Muharram
        Ashura,          // 10 Muharram
        Mevlid,          // before 12 Rabi al-Awwal
        DayCount
    };

    // The religious day on day `day` (1 to `length`) of Hijri month `month`, `weekday` 0 Monday
    // (Dates::weekday). The last day of Jumada al-Akhirah is Regaip when it is a Thursday:
    // Rajab then begins on the first Friday.
    static Day on( int month, int day, int length, int weekday );

    // A bit per day of the month (bit 0: day 1) that is a religious day.
    static uint32_t marks( const Hijri::Month &month );

    // The first religious day of `month` on or after local day `from`; its local day in `found`.
    static Day firstFrom( const Hijri::Month &month, int from, int &found );

    // What the calendar says about today: the next religious day (today's, if it is one), and
    // the month's own counts.
    struct Next
    {
        Day day;         // None: not found within the months searched
        int dayNumber;   // its local day
        int daysLeft;    // 0: today
        int ramadanIn;   // in Shaban: days until 1 Ramadan, else 0
        int ramadanDay;  // in Ramadan: today's day of it, else 0
    };

    // `months`: the Hijri months from the one that holds `today` on, `count` of them in order.
    static Next next( int today, const Hijri::Month *months, int count );

    // Months to look ahead at most: from Mevlid (12 Rabi al-Awwal) to Regaip (early Rajab) the
    // search crosses four month starts.
    static const int MaxMonths = 6;
};
