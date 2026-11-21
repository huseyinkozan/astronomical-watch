// Host test of the prayer times in main/Sky.cpp against Diyanet's published tables for 2026
// (tests/diyanet_prayer_times.csv). Build and run: sh tests/run_host_tests.sh

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Dates.h"
#include "Sky.h"

struct City
{
    const char *name;
    Astronomy::Observer observer;
    bool european;  // EET/EEST instead of Turkey's fixed UTC+3
    int ishaAngle;  // 16 where Diyanet uses it (Romania)
};

// Istanbul and Ankara as Diyanet gives them; the others are ordinary city centres.
static const City s_cities[] = { { "istanbul", { 41.012, 28.974 }, false, 17 },  { "ankara", { 39.938, 32.848 }, false, 17 },
                                 { "diyarbakir", { 37.914, 40.231 }, false, 17 }, { "antalya", { 36.897, 30.713 }, false, 17 },
                                 { "sofia", { 42.698, 23.322 }, true, 17 },       { "bucharest", { 44.427, 26.103 }, true, 16 } };
static const int s_cityCount = sizeof( s_cities ) / sizeof( s_cities[0] );

// Eastern European time: UTC+3 from the last Sunday of March to the last Sunday of October
// (01:00 UTC each), UTC+2 otherwise.
static int europeanOffset( int day )
{
    int year = 0;
    int month = 0;
    int date = 0;
    Dates::civil( day, year, month, date );
    const int march = Dates::dayNumber( year, 3, 31 );
    const int october = Dates::dayNumber( year, 10, 31 );
    const int start = march - ( Dates::weekday( march ) + 1 ) % 7;
    const int end = october - ( Dates::weekday( october ) + 1 ) % 7;
    return day >= start && day < end ? 180 : 120;
}

// The kerahat intervals on Istanbul's 21 June 2026 (Diyanet: sunrise 05:25, dhuhr 13:11,
// maghrib 20:47): 05:25-06:10, 13:01-13:11 and 20:02-20:47, then tomorrow's morning.
static bool checkKerahat()
{
    const int day = Dates::dayNumber( 2026, 6, 21 );
    const int64_t midnight = static_cast<int64_t>( day ) * 86400 - 180 * 60;
    const Sky::PrayerTimes prayers = Sky::prayerTimes( day, s_cities[0].observer, 17 );
    // Local minute of the day, the interval expected (start and end minute), inside it or not.
    struct Case
    {
        int minute;
        int start;
        int end;
        bool now;
    };
    static const Case s_cases[] = { { 4 * 60, 5 * 60 + 25, 6 * 60 + 10, false },
                                    { 5 * 60 + 40, 5 * 60 + 25, 6 * 60 + 10, true },
                                    { 6 * 60 + 10, 13 * 60 + 1, 13 * 60 + 11, false },
                                    { 13 * 60 + 5, 13 * 60 + 1, 13 * 60 + 11, true },
                                    { 17 * 60, 20 * 60 + 2, 20 * 60 + 47, false },
                                    { 20 * 60 + 46, 20 * 60 + 2, 20 * 60 + 47, true },
                                    { 21 * 60, 24 * 60 + 5 * 60 + 25, 24 * 60 + 6 * 60 + 10, false } };
    bool ok = true;
    for ( size_t i = 0; i < sizeof( s_cases ) / sizeof( s_cases[0] ); ++i ) {
        const Case &c = s_cases[i];
        const Sky::Kerahat kerahat = Sky::kerahat( prayers, midnight + c.minute * 60 );
        const int start = static_cast<int>( ( kerahat.start - midnight ) / 60 );
        const int end = static_cast<int>( ( kerahat.end - midnight ) / 60 );
        if ( !kerahat.valid || start != c.start || end != c.end || kerahat.now != c.now ) {
            printf( "FAIL kerahat at %02d:%02d: %d-%d (now %d), expected %d-%d (now %d)\n", c.minute / 60, c.minute % 60,
                    start, end, kerahat.now, c.start, c.end, c.now );
            ok = false;
        }
    }
    printf( "Kerahat: %s\n", ok ? "ok" : "FAIL" );
    return ok;
}

int main( int argc, char **argv )
{
    const bool kerahatOk = checkKerahat();
    const char *path = argc > 1 ? argv[1] : "tests/diyanet_prayer_times.csv";
    FILE *file = fopen( path, "r" );
    if ( file == nullptr ) {
        printf( "Cannot open %s\n", path );
        return 1;
    }
    int total = 0;
    int exact = 0;
    int withinOne = 0;
    int worst = 0;
    char line[160];
    while ( fgets( line, sizeof( line ), file ) != nullptr ) {
        char name[32] = {};
        int year = 0;
        int month = 0;
        int date = 0;
        int hours[6] = {};
        int minutes[6] = {};
        if ( line[0] == '#' || sscanf( line, "%31[^,],%d-%d-%d,%d:%d,%d:%d,%d:%d,%d:%d,%d:%d,%d:%d", name, &year, &month,
                                       &date, &hours[0], &minutes[0], &hours[1], &minutes[1], &hours[2], &minutes[2],
                                       &hours[3], &minutes[3], &hours[4], &minutes[4], &hours[5], &minutes[5] ) != 16 ) {
            continue;
        }
        const City *city = nullptr;
        for ( int i = 0; i < s_cityCount; ++i ) {
            if ( strcmp( s_cities[i].name, name ) == 0 ) {
                city = &s_cities[i];
            }
        }
        if ( city == nullptr ) {
            continue;
        }
        const int day = Dates::dayNumber( year, month, date );
        const int offset = city->european ? europeanOffset( day ) : 180;
        const int64_t midnight = static_cast<int64_t>( day ) * 86400 - offset * 60;
        const Sky::PrayerTimes prayers = Sky::prayerTimes( day, city->observer, city->ishaAngle );
        for ( int i = 0; i < Sky::PrayerCount; ++i ) {
            // Diyanet's summer limit of imsak and the night prayer in 16-degree countries is not
            // reproduced; the watch marks those days as approximate (Sky::prayerTimes).
            if ( prayers.approximate && ( i == Sky::Imsak || i == Sky::Isha ) ) {
                continue;
            }
            const int published = hours[i] * 60 + minutes[i];
            const int computed = static_cast<int>( ( prayers.times[i] - midnight ) / 60 );
            const int difference = std::abs( computed - published );
            ++total;
            exact += difference == 0;
            withinOne += difference <= 1;
            if ( difference > worst ) {
                worst = difference;
            }
            if ( difference > 2 || !prayers.valid[i] ) {
                printf( "FAIL %s %04d-%02d-%02d time %d: %02d:%02d computed, %02d:%02d published\n", name, year, month,
                        date, i, computed / 60, computed % 60, published / 60, published % 60 );
            }
        }
    }
    fclose( file );
    // Diyanet takes the Sun's position once a day and its own city coordinates, which this code
    // does not know exactly (research/prayer-times.md): a minute off now and then is expected,
    // two minutes or more is a fault.
    const bool ok = total > 0 && worst <= 1 && withinOne == total;
    printf( "Prayer times: %d compared, %d exact (%.1f %%), all within %d min%s\n", total, exact,
            total > 0 ? exact * 100.0 / total : 0.0, worst, ok ? "" : " FAIL" );
    return ok && kerahatOk ? 0 : 1;
}
