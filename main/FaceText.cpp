#include "FaceText.h"

#include <cmath>
#include <cstdio>

#include "Dates.h"
#include "FaceTable.h"
#include "Language.h"
#include "Sky.h"
#include "Zones.h"

static const Language::Text s_prayerNames[Sky::PrayerCount] = { Language::Imsak, Language::PrayerSunrise,
                                                                 Language::Dhuhr, Language::Asr,
                                                                 Language::Maghrib, Language::Isha };

// The bytes of one UTF-8 character starting with `lead`.
static int characterLength( unsigned char lead )
{
    if ( lead < 0x80 ) {
        return 1;
    }
    if ( ( lead & 0xE0 ) == 0xC0 ) {
        return 2;
    }
    if ( ( lead & 0xF0 ) == 0xE0 ) {
        return 3;
    }
    return 4;
}

int FaceText::fit( const char *text, int advance, int maxWidth )
{
    int bytes = 0;
    int width = advance;
    while ( text[bytes] != '\0' && width <= maxWidth ) {
        bytes += characterLength( static_cast<unsigned char>( text[bytes] ) );
        width += advance;
    }
    return bytes;
}

// "Name h:mm": the event's name and the time left, rounded to the minute as the faces did.
static void timeLeft( const char *name, int64_t seconds, char *out, int size )
{
    const int minutes = static_cast<int>( ( seconds + 30 ) / 60 );
    snprintf( out, size, "%s %d:%02d", name, minutes / 60, minutes % 60 );
}

// Today's Hijri date from the table; after midnight the next day while the month certainly
// lasts (the length of a month is not in the table).
static bool hijriDate( const FaceText::Inputs &in, int &day, int &month, int &year )
{
    if ( in.table == nullptr || !in.skyValid ) {
        return false;
    }
    const int today = Dates::dayNumber( in.year, in.month, in.day );
    const FaceTable &table = *in.table;
    if ( today == table.localDay ) {
        day = table.hijriDay;
    } else if ( today == table.localDay + 1 && table.hijriDay < 29 ) {
        day = table.hijriDay + 1;
    } else {
        return false;
    }
    month = table.hijriMonth;
    year = table.hijriYear;
    return true;
}

static void altitude( bool valid, double degrees, char *out, int size )
{
    if ( valid && degrees >= 0 ) {
        snprintf( out, size, "%d°", static_cast<int>( lround( degrees ) ) );
    }
}

void FaceText::text( FaceConfig::TextItem item, int parameter, const Inputs &in, char *out, int size )
{
    out[0] = '\0';
    if ( !in.timeValid ) {
        return;
    }
    int hijriDay = 0;
    int hijriMonth = 0;
    int hijriYear = 0;
    FaceTable::Event event = {};
    switch ( item ) {
    case FaceConfig::LongDate:
        Language::longDate( out, size, in.weekday, in.day, in.month );
        break;
    case FaceConfig::ShortDate:
        if ( Language::current() == Language::Turkish ) {
            snprintf( out, size, "%d %s", in.day, Language::shortMonth( in.month ) );
        } else {
            snprintf( out, size, "%s %d", Language::shortMonth( in.month ), in.day );
        }
        break;
    case FaceConfig::HijriDate:
        if ( hijriDate( in, hijriDay, hijriMonth, hijriYear ) ) {
            snprintf( out, size, "%d %s %d", hijriDay, Language::hijriMonth( hijriMonth ), hijriYear );
        }
        break;
    case FaceConfig::NextSunEvent:
        if ( in.skyValid && in.table != nullptr && in.table->next( in.now, false, event ) ) {
            const bool sunset = event.kind == FaceTable::SunsetEvent;
            timeLeft( Language::text( sunset ? Language::Sunset : Language::Sunrise ), event.time - in.now, out, size );
        }
        break;
    case FaceConfig::NextPrayer:
        if ( in.skyValid && in.table != nullptr && in.table->next( in.now, true, event ) ) {
            timeLeft( Language::text( s_prayerNames[event.kind - FaceTable::PrayerEvent] ), event.time - in.now, out, size );
        }
        break;
    case FaceConfig::Seconds:
        snprintf( out, size, ":%02d", in.second );
        break;
    case FaceConfig::BatteryPercent:
        if ( in.batteryPercent >= 0 ) {
            snprintf( out, size, "%d %%", in.batteryPercent );
        }
        break;
    case FaceConfig::CityTime:
        if ( in.table != nullptr && parameter >= 0 && parameter < FaceTable::CityCount &&
             in.table->cityNames[parameter][0] != '\0' ) {
            const int64_t local = in.now + Zones::offsetAt( in.table->cityZones[parameter], in.now ) * 60;
            const int minuteOfDay = static_cast<int>( ( ( local % 86400 ) + 86400 ) % 86400 / 60 );
            snprintf( out, size, "%s %02d:%02d", in.table->cityNames[parameter], minuteOfDay / 60, minuteOfDay % 60 );
        }
        break;
    case FaceConfig::DayCounter:
        if ( in.table != nullptr && parameter >= 0 && parameter < FaceTable::CounterCount &&
             in.table->counterNames[parameter][0] != '\0' ) {
            const int today = Dates::dayNumber( in.year, in.month, in.day );
            snprintf( out, size, "%s %d", in.table->counterNames[parameter], in.table->counterDays[parameter] - today );
        }
        break;
    case FaceConfig::SunAltitude:
        altitude( in.altitudesValid && in.skyValid, in.sunAltitude, out, size );
        break;
    case FaceConfig::MoonAltitude:
        altitude( in.altitudesValid && in.skyValid, in.moonAltitude, out, size );
        break;
    default:
        break;
    }
}

// The first three characters of a name ("Muh" of "Muharrem").
static void shortName( const char *name, char *out, int size )
{
    int bytes = 0;
    for ( int i = 0; i < 3 && name[bytes] != '\0'; ++i ) {
        bytes += characterLength( static_cast<unsigned char>( name[bytes] ) );
    }
    snprintf( out, size, "%.*s", bytes, name );
}

void FaceText::corner( FaceConfig::CornerItem item, const Inputs &in, char *out, int size )
{
    out[0] = '\0';
    if ( !in.timeValid ) {
        return;
    }
    int hijriDay = 0;
    int hijriMonth = 0;
    int hijriYear = 0;
    switch ( item ) {
    case FaceConfig::WeekdayShort:
        snprintf( out, size, "%s", Language::shortDay( in.weekday ) );
        break;
    case FaceConfig::DayOfMonth:
        snprintf( out, size, "%d", in.day );
        break;
    case FaceConfig::HijriDayOfMonth:
        if ( hijriDate( in, hijriDay, hijriMonth, hijriYear ) ) {
            snprintf( out, size, "%d", hijriDay );
        }
        break;
    case FaceConfig::MonthShort:
        snprintf( out, size, "%s", Language::shortMonth( in.month ) );
        break;
    case FaceConfig::HijriMonthShort:
        if ( hijriDate( in, hijriDay, hijriMonth, hijriYear ) ) {
            shortName( Language::hijriMonth( hijriMonth ), out, size );
        }
        break;
    case FaceConfig::SunAltitudeCorner:
        altitude( in.altitudesValid && in.skyValid, in.sunAltitude, out, size );
        break;
    case FaceConfig::MoonAltitudeCorner:
        altitude( in.altitudesValid && in.skyValid, in.moonAltitude, out, size );
        break;
    case FaceConfig::HourCorner:
        snprintf( out, size, "%02d", in.hour );
        break;
    case FaceConfig::MinuteCorner:
        snprintf( out, size, "%02d", in.minute );
        break;
    case FaceConfig::SecondCorner:
        snprintf( out, size, "%02d", in.second );
        break;
    default:
        break;
    }
}
