#include "HolyDays.h"

#include "Dates.h"

static const int s_thursday = 3;

HolyDays::Day HolyDays::on( int month, int day, int length, int weekday )
{
    switch ( month ) {
    case 1:
        return day == 1 ? NewYear : ( day == 10 ? Ashura : None );
    case 3:
        return day == 11 ? Mevlid : None;
    case 6:
        return day == length && weekday == s_thursday ? Regaip : None;
    case 7:
        // A Thursday of days 1 to 6 comes before the month's first Friday.
        if ( day <= 6 && weekday == s_thursday ) {
            return Regaip;
        }
        return day == 26 ? Mirac : None;
    case 8:
        return day == 14 ? Berat : None;
    case 9:
        return day == 1 ? RamadanStart : ( day == 26 ? Kadir : None );
    case 10:
        return day <= 3 ? RamazanBayrami : None;
    case 12:
        return day >= 10 && day <= 13 ? KurbanBayrami : None;
    default:
        return None;
    }
}

uint32_t HolyDays::marks( const Hijri::Month &month )
{
    uint32_t bits = 0;
    for ( int day = 1; day <= month.length && day <= 30; ++day ) {
        if ( on( month.month, day, month.length, Dates::weekday( month.firstDay + day - 1 ) ) != None ) {
            bits |= 1u << ( day - 1 );
        }
    }
    return bits;
}

HolyDays::Day HolyDays::firstFrom( const Hijri::Month &month, int from, int &found )
{
    for ( int day = 1; day <= month.length; ++day ) {
        const int number = month.firstDay + day - 1;
        if ( number < from ) {
            continue;
        }
        const Day holy = on( month.month, day, month.length, Dates::weekday( number ) );
        if ( holy != None ) {
            found = number;
            return holy;
        }
    }
    return None;
}

HolyDays::Next HolyDays::next( int today, const Hijri::Month *months, int count )
{
    Next result = {};
    if ( count > 0 ) {
        const Hijri::Month &current = months[0];
        if ( current.month == 8 ) {
            result.ramadanIn = current.firstDay + current.length - today;
        } else if ( current.month == 9 ) {
            result.ramadanDay = today - current.firstDay + 1;
        }
    }
    for ( int i = 0; i < count; ++i ) {
        int found = 0;
        const Day holy = firstFrom( months[i], today, found );
        if ( holy != None ) {
            result.day = holy;
            result.dayNumber = found;
            result.daysLeft = found - today;
            return result;
        }
    }
    return result;
}
