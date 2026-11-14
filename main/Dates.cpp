#include "Dates.h"

int Dates::dayNumber( int year, int month, int day )
{
    year -= month <= 2;
    const int era = ( year >= 0 ? year : year - 399 ) / 400;
    const int yearOfEra = year - era * 400;
    const int dayOfYear = ( 153 * ( month + ( month > 2 ? -3 : 9 ) ) + 2 ) / 5 + day - 1;
    const int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

void Dates::civil( int dayNumber, int &year, int &month, int &day )
{
    // H. Hinnant's civil_from_days.
    const int z = dayNumber + 719468;
    const int era = ( z >= 0 ? z : z - 146096 ) / 146097;
    const int dayOfEra = z - era * 146097;
    const int yearOfEra = ( dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096 ) / 365;
    const int dayOfYear = dayOfEra - ( 365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100 );
    const int monthIndex = ( 5 * dayOfYear + 2 ) / 153;
    day = dayOfYear - ( 153 * monthIndex + 2 ) / 5 + 1;
    month = monthIndex < 10 ? monthIndex + 3 : monthIndex - 9;
    year = yearOfEra + era * 400 + ( month <= 2 );
}

int Dates::weekday( int dayNumber )
{
    // 1970-01-01 was a Thursday.
    return ( ( dayNumber % 7 ) + 7 + 3 ) % 7;
}

int Dates::daysInMonth( int year, int month )
{
    const int next = month == 12 ? dayNumber( year + 1, 1, 1 ) : dayNumber( year, month + 1, 1 );
    return next - dayNumber( year, month, 1 );
}

int Dates::isoWeek( int dayNumber )
{
    const int thursday = dayNumber - weekday( dayNumber ) + 3;
    int year = 0;
    int month = 0;
    int day = 0;
    civil( thursday, year, month, day );
    return ( thursday - Dates::dayNumber( year, 1, 1 ) ) / 7 + 1;
}

int Dates::dayOfYear( int dayNumber )
{
    int year = 0;
    int month = 0;
    int day = 0;
    civil( dayNumber, year, month, day );
    return dayNumber - Dates::dayNumber( year, 1, 1 ) + 1;
}
