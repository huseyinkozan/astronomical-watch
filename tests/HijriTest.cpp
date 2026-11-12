// Host test of main/Hijri.cpp: the Diyanet method against month starts published by Turkey's
// Diyanet (Presidency of Religious Affairs), and the tabular calendar against known dates.
// Build and run: sh tests/run_host_tests.sh

#include <cstdio>

#include "Hijri.h"

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

// Month starts where the Diyanet method differs by a day: evenings within 0.1 degree of a limit
// (see Hijri::diyanetEvening). Any other difference fails the test.
static const int s_knownDifferences[3][3] = { { 2028, 8, 21 }, { 2029, 5, 14 }, { 2030, 5, 3 } };

static bool knownDifference( int year, int month, int day )
{
    for ( int i = 0; i < 3; ++i ) {
        if ( s_knownDifferences[i][0] == year && s_knownDifferences[i][1] == month && s_knownDifferences[i][2] == day ) {
            return true;
        }
    }
    return false;
}

int main( int argc, char **argv )
{
    // First days of Hijri months published by Diyanet (tools: see the file's header).
    const char *path = argc > 1 ? argv[1] : "tests/diyanet_month_starts.csv";
    FILE *file = fopen( path, "r" );
    if ( file == nullptr ) {
        printf( "Cannot open %s\n", path );
        return 1;
    }
    const int turkey = 180;
    const Astronomy::Observer ankara = { 39.93, 32.86 };
    int checked = 0;
    int matched = 0;
    int failed = 0;
    char line[128];
    while ( fgets( line, sizeof( line ), file ) != nullptr ) {
        int year = 0;
        int month = 0;
        int day = 0;
        int hijriYear = 0;
        int hijriMonth = 0;
        if ( line[0] == '#' || sscanf( line, "%d,%d,%d,%d,%d", &year, &month, &day, &hijriYear, &hijriMonth ) != 5 ) {
            continue;
        }
        ++checked;
        const int number = dayNumber( year, month, day );
        const Hijri::Date first = Hijri::date( number, turkey, ankara, Hijri::Diyanet );
        const bool ok = first.year == hijriYear && first.month == hijriMonth && first.day == 1;
        const bool known = knownDifference( year, month, day );
        if ( ok ) {
            ++matched;
        } else if ( !known ) {
            ++failed;
        }
        if ( !ok || known ) {
            printf( "%s %04d-%02d-%02d: Diyanet %d/%d/1, computed %d/%d/%d\n", ok ? "now matches" : ( known ? "known" : "FAIL" ),
                    year, month, day, hijriYear, hijriMonth, first.year, first.month, first.day );
        }
    }
    fclose( file );

    // The local method has no official list; show the month starts at Ankara around today.
    for ( int i = 0; i < 4; ++i ) {
        const int number = dayNumber( 2026, 11, 9 ) + i;
        const Hijri::Date local = Hijri::date( number, turkey, ankara, Hijri::LocalCrescent );
        const Hijri::Crescent crescent = Hijri::crescent( number - 1, turkey, ankara );
        printf( "Ankara, day %d: local crescent date %d/%d/%d (crescent the evening before: zone %d, V %.2f)\n", number,
                local.year, local.month, local.day, crescent.zone, crescent.v );
    }
    printf( "\nDiyanet: %d of %d month starts match, %d unexpected differences\n", matched, checked, failed );
    return failed == 0 ? 0 : 1;
}
