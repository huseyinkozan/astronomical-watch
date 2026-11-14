#pragma once

// Gregorian calendar arithmetic on day numbers (days since 1970-01-01). Plain C++ for the host
// tests.
class Dates
{
public:
    // H. Hinnant's days_from_civil.
    static int dayNumber( int year, int month, int day );

    // The Gregorian date of a day number.
    static void civil( int dayNumber, int &year, int &month, int &day );

    // 0 Monday to 6 Sunday.
    static int weekday( int dayNumber );

    static int daysInMonth( int year, int month );

    // ISO 8601: the week of the year's first Thursday is week 1.
    static int isoWeek( int dayNumber );

    // 1 for 1 January.
    static int dayOfYear( int dayNumber );
};
