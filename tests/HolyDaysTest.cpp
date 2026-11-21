// Host test of main/HolyDays.cpp: the religious days on Diyanet's month starts
// (tests/diyanet_month_starts.csv) against the days of Diyanet's calendar for 2026 and early
// 2027, and the search for the next one with the watch's Diyanet method.
// Build and run: sh tests/run_host_tests.sh

#include <cstdio>

#include "Dates.h"
#include "HolyDays.h"

static int failures = 0;

static void check( const char *what, int value, int expected )
{
    if ( value != expected ) {
        printf( "FAIL %s: %d, expected %d\n", what, value, expected );
        ++failures;
    }
}

struct Listed
{
    int year;
    int month;
    int day;
    HolyDays::Day holy;
};

// Diyanet's religious days 2026 (every one of the year) and the first ones of 2027, as its
// calendar dates them (the kandils on their evenings). Read from Diyanet's 2026 list as
// newspapers and calendar sites reprint it (2026-11-21; e.g. takvimi.org/dini-gunler-2026):
// Miraç, Berat, Ramazan, Kadir, Mevlid and Regaip of 2026 and Miraç, Berat, Ramazan, Kadir and
// Mevlid of 2027 were checked there; the bayrams, the Hijri new year and Aşure follow from the
// month starts of the same calendar.
static const Listed s_listed[] = {
    { 2026, 1, 15, HolyDays::Mirac },          { 2026, 2, 2, HolyDays::Berat },
    { 2026, 2, 19, HolyDays::RamadanStart },   { 2026, 3, 16, HolyDays::Kadir },
    { 2026, 3, 20, HolyDays::RamazanBayrami }, { 2026, 3, 21, HolyDays::RamazanBayrami },
    { 2026, 3, 22, HolyDays::RamazanBayrami }, { 2026, 5, 27, HolyDays::KurbanBayrami },
    { 2026, 5, 28, HolyDays::KurbanBayrami },  { 2026, 5, 29, HolyDays::KurbanBayrami },
    { 2026, 5, 30, HolyDays::KurbanBayrami },  { 2026, 6, 16, HolyDays::NewYear },
    { 2026, 6, 25, HolyDays::Ashura },         { 2026, 8, 24, HolyDays::Mevlid },
    { 2026, 12, 10, HolyDays::Regaip },        { 2027, 1, 4, HolyDays::Mirac },
    { 2027, 1, 22, HolyDays::Berat },          { 2027, 2, 8, HolyDays::RamadanStart },
    { 2027, 3, 5, HolyDays::Kadir },           { 2027, 3, 9, HolyDays::RamazanBayrami },
    { 2027, 5, 16, HolyDays::KurbanBayrami },  { 2027, 8, 13, HolyDays::Mevlid },
};
static const int s_listedCount = sizeof( s_listed ) / sizeof( s_listed[0] );

static const int s_maxMonths = 200;

int main( int argc, char **argv )
{
    const char *path = argc > 1 ? argv[1] : "tests/diyanet_month_starts.csv";
    FILE *file = fopen( path, "r" );
    if ( file == nullptr ) {
        printf( "Cannot open %s\n", path );
        return 1;
    }
    // Diyanet's months: each lasts until the next one's start.
    Hijri::Month months[s_maxMonths] = {};
    int count = 0;
    char line[128];
    while ( fgets( line, sizeof( line ), file ) != nullptr && count < s_maxMonths ) {
        int year = 0;
        int month = 0;
        int day = 0;
        int hijriYear = 0;
        int hijriMonth = 0;
        if ( line[0] == '#' || sscanf( line, "%d,%d,%d,%d,%d", &year, &month, &day, &hijriYear, &hijriMonth ) != 5 ) {
            continue;
        }
        months[count] = { hijriYear, hijriMonth, Dates::dayNumber( year, month, day ), 0 };
        if ( count > 0 ) {
            months[count - 1].length = months[count].firstDay - months[count - 1].firstDay;
        }
        ++count;
    }
    fclose( file );

    // Every day of 2026: a religious day exactly where the list has one; then the listed days.
    const int from = Dates::dayNumber( 2026, 1, 1 );
    const int to = Dates::dayNumber( 2027, 8, 31 );
    int found = 0;
    for ( int i = 0; i + 1 < count; ++i ) {
        const Hijri::Month &month = months[i];
        const uint32_t marks = HolyDays::marks( month );
        for ( int day = 1; day <= month.length; ++day ) {
            const int number = month.firstDay + day - 1;
            if ( number < from || number > to ) {
                continue;
            }
            const HolyDays::Day holy = HolyDays::on( month.month, day, month.length, Dates::weekday( number ) );
            HolyDays::Day expected = HolyDays::None;
            for ( int j = 0; j < s_listedCount; ++j ) {
                if ( Dates::dayNumber( s_listed[j].year, s_listed[j].month, s_listed[j].day ) == number ) {
                    expected = s_listed[j].holy;
                }
            }
            // The 2027 part of the list is not complete: only its days are checked.
            if ( number > Dates::dayNumber( 2026, 12, 31 ) && expected == HolyDays::None ) {
                continue;
            }
            int year = 0;
            int monthNumber = 0;
            int date = 0;
            Dates::civil( number, year, monthNumber, date );
            char what[64];
            snprintf( what, sizeof( what ), "religious day %04d-%02d-%02d", year, monthNumber, date );
            check( what, holy, expected );
            snprintf( what, sizeof( what ), "mark %04d-%02d-%02d", year, monthNumber, date );
            check( what, ( marks >> ( day - 1 ) ) & 1, holy != HolyDays::None ? 1 : 0 );
            found += expected != HolyDays::None && holy == expected;
        }
    }
    check( "listed days found", found, s_listedCount );

    // Rajab starting on a Friday: Regaip is the Thursday before, the last day of the month before.
    check( "Regaip before a Friday 1 Rajab", HolyDays::on( 6, 29, 29, 3 ), HolyDays::Regaip );
    check( "no Regaip on a Thursday 7 Rajab", HolyDays::on( 7, 7, 30, 3 ), HolyDays::None );
    check( "Regaip on a Thursday 6 Rajab", HolyDays::on( 7, 6, 30, 3 ), HolyDays::Regaip );

    // The next one with the watch's Diyanet method, as main.cpp searches it.
    const Astronomy::Observer ankara = { 39.93, 32.86 };
    struct Search
    {
        int year, month, day;
        HolyDays::Day holy;
        int daysLeft, ramadanIn, ramadanDay;
    };
    static const Search s_searches[] = {
        { 2026, 11, 21, HolyDays::Regaip, 19, 0, 0 },        // the end of Jumada al-Ula
        { 2026, 8, 25, HolyDays::Regaip, 107, 0, 0 },        // the day after Mevlid: the longest search
        { 2027, 1, 25, HolyDays::RamadanStart, 14, 14, 0 },  // Shaban, after Berat
        { 2027, 1, 12, HolyDays::Berat, 10, 27, 0 },         // Shaban, before Berat
        { 2026, 3, 1, HolyDays::Kadir, 15, 0, 11 },          // Ramadan
        { 2026, 3, 21, HolyDays::RamazanBayrami, 0, 0, 0 },  // the bayram's second day
    };
    for ( size_t i = 0; i < sizeof( s_searches ) / sizeof( s_searches[0] ); ++i ) {
        const Search &search = s_searches[i];
        const int today = Dates::dayNumber( search.year, search.month, search.day );
        Hijri::Month ahead[HolyDays::MaxMonths] = {};
        ahead[0] = Hijri::month( today, 180, ankara, Hijri::Diyanet );
        for ( int j = 1; j < HolyDays::MaxMonths; ++j ) {
            ahead[j] = Hijri::month( ahead[j - 1].firstDay + ahead[j - 1].length, 180, ankara, Hijri::Diyanet );
        }
        const HolyDays::Next next = HolyDays::next( today, ahead, HolyDays::MaxMonths );
        char what[64];
        snprintf( what, sizeof( what ), "next from %04d-%02d-%02d", search.year, search.month, search.day );
        check( what, next.day, search.holy );
        check( what, next.daysLeft, search.daysLeft );
        check( what, next.dayNumber, today + search.daysLeft );
        check( what, next.ramadanIn, search.ramadanIn );
        check( what, next.ramadanDay, search.ramadanDay );
    }

    printf( "Religious days: %d listed found, %d failures\n", found, failures );
    return failures == 0 ? 0 : 1;
}
