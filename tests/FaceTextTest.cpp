// Host test of main/FaceText.cpp and main/FaceTable.cpp with the fixed scene of the published
// screenshots: Istanbul, 2026-06-21 19:19:00 local time (UTC+3). Build and run:
// sh tests/run_host_tests.sh

#include <cstdio>
#include <cstring>

#include "Astronomy.h"
#include "Dates.h"
#include "FaceTable.h"
#include "FaceText.h"
#include "Language.h"
#include "Sky.h"

static int s_checks = 0;
static int s_failures = 0;

static void checkText( const char *name, const char *text, const char *expected )
{
    ++s_checks;
    if ( strcmp( text, expected ) != 0 ) {
        printf( "FAIL %s: \"%s\", expected \"%s\"\n", name, text, expected );
        ++s_failures;
    }
}

static void check( const char *name, bool condition )
{
    ++s_checks;
    if ( !condition ) {
        printf( "FAIL %s\n", name );
        ++s_failures;
    }
}

static int64_t localToUtc( int year, int month, int day, int hour, int minute, int offsetMinutes )
{
    return static_cast<int64_t>( Dates::dayNumber( year, month, day ) ) * 86400 + ( hour * 60 + minute - offsetMinutes ) * 60;
}

int main()
{
    const int offset = 180;
    const Astronomy::Observer istanbul = { 41.0082, 28.9784 };
    const int day = Dates::dayNumber( 2026, 6, 21 );
    // The scene's Hijri method: local crescent visibility ("Hilal" on the setup-info part).
    const Sky::Day sky = Sky::computeDay( day, offset, istanbul, Hijri::LocalCrescent, 17 );
    FaceTable table = {};
    table.build( sky, offset, istanbul, 17 );
    table.setCounter( 0, "Ramazan", Dates::dayNumber( 2027, 2, 8 ) );
    table.setCity( 0, "London", { 60, { 0, 0 }, { 0, 0 } } );

    FaceText::Inputs in = {};
    in.timeValid = true;
    in.year = 2026;
    in.month = 6;
    in.day = 21;
    in.hour = 19;
    in.minute = 19;
    in.second = 42;
    in.weekday = Dates::weekday( day );
    in.skyValid = true;
    in.now = localToUtc( 2026, 6, 21, 19, 19, offset );
    in.table = &table;
    in.batteryPercent = 68;
    in.sunAltitude = Astronomy::sunPosition( in.now, istanbul ).altitude;
    in.moonAltitude = Astronomy::moonPosition( in.now, istanbul ).altitude;
    in.altitudesValid = true;

    char text[48];
    Language::set( Language::Turkish );
    FaceText::text( FaceConfig::LongDate, 0, in, text, sizeof( text ) );
    checkText( "tr long date", text, "21 Haziran Pazar" );
    FaceText::text( FaceConfig::ShortDate, 0, in, text, sizeof( text ) );
    checkText( "tr short date", text, "21 Haz" );
    FaceText::text( FaceConfig::HijriDate, 0, in, text, sizeof( text ) );
    checkText( "tr Hijri date", text, "5 Muharrem 1448" );
    FaceText::text( FaceConfig::NextSunEvent, 0, in, text, sizeof( text ) );
    checkText( "tr next sun event", text, "Gün batımı 1:21" );
    FaceText::text( FaceConfig::NextPrayer, 0, in, text, sizeof( text ) );
    checkText( "tr next prayer", text, "Akşam 1:28" );
    FaceText::text( FaceConfig::SunAltitude, 0, in, text, sizeof( text ) );
    checkText( "tr sun altitude", text, "13°" );
    FaceText::text( FaceConfig::Seconds, 0, in, text, sizeof( text ) );
    checkText( "tr seconds", text, ":42" );
    FaceText::text( FaceConfig::BatteryPercent, 0, in, text, sizeof( text ) );
    checkText( "tr battery", text, "68 %" );
    FaceText::text( FaceConfig::CityTime, 0, in, text, sizeof( text ) );
    checkText( "tr city", text, "London 17:19" );
    FaceText::text( FaceConfig::DayCounter, 0, in, text, sizeof( text ) );
    checkText( "tr day counter", text, "Ramazan 232" );
    FaceText::text( FaceConfig::EmergencyText, 0, in, text, sizeof( text ) );
    checkText( "tr emergency, no card", text, "" );
    table.emergencyCard = true;
    FaceText::text( FaceConfig::EmergencyText, 0, in, text, sizeof( text ) );
    checkText( "tr emergency, card without blood type", text, "Acil durum" );
    snprintf( table.bloodType, sizeof( table.bloodType ), "%s", "A Rh+" );
    FaceText::text( FaceConfig::EmergencyText, 0, in, text, sizeof( text ) );
    checkText( "tr emergency: the blood type", text, "A Rh+" );
    FaceText::text( FaceConfig::CityTime, 1, in, text, sizeof( text ) );
    checkText( "tr city not set", text, "" );
    FaceText::corner( FaceConfig::WeekdayShort, in, text, sizeof( text ) );
    checkText( "tr weekday corner", text, "Paz" );
    FaceText::corner( FaceConfig::DayOfMonth, in, text, sizeof( text ) );
    checkText( "tr day corner", text, "21" );
    FaceText::corner( FaceConfig::HijriDayOfMonth, in, text, sizeof( text ) );
    checkText( "tr Hijri day corner", text, "5" );
    FaceText::corner( FaceConfig::MonthShort, in, text, sizeof( text ) );
    checkText( "tr month corner", text, "Haz" );
    FaceText::corner( FaceConfig::HijriMonthShort, in, text, sizeof( text ) );
    checkText( "tr Hijri month corner", text, "Muh" );
    FaceText::corner( FaceConfig::HourCorner, in, text, sizeof( text ) );
    checkText( "tr hour corner", text, "19" );
    FaceText::corner( FaceConfig::SecondCorner, in, text, sizeof( text ) );
    checkText( "tr second corner", text, "42" );
    FaceText::corner( FaceConfig::MoonIcon, in, text, sizeof( text ) );
    checkText( "tr icon corner is drawn", text, "" );

    Language::set( Language::English );
    FaceText::text( FaceConfig::LongDate, 0, in, text, sizeof( text ) );
    checkText( "en long date", text, "Sunday 21 June" );
    FaceText::text( FaceConfig::NextSunEvent, 0, in, text, sizeof( text ) );
    checkText( "en next sun event", text, "Sunset 1:21" );
    FaceText::text( FaceConfig::ShortDate, 0, in, text, sizeof( text ) );
    checkText( "en short date", text, "Jun 21" );

    // The clock as a slot (HH:MM), in a text line and in a corner; the short corner items in a
    // text line give the corner's text.
    FaceText::text( FaceConfig::TimeText, 0, in, text, sizeof( text ) );
    checkText( "time line", text, "19:19" );
    FaceText::corner( FaceConfig::TimeCorner, in, text, sizeof( text ) );
    checkText( "time corner", text, "19:19" );
    const FaceConfig::TextItem lines[7] = { FaceConfig::WeekdayText, FaceConfig::DayOfMonthText, FaceConfig::MonthText,
                                             FaceConfig::HijriDayText, FaceConfig::HijriMonthText, FaceConfig::HourText,
                                             FaceConfig::MinuteText };
    const FaceConfig::CornerItem corners[7] = { FaceConfig::WeekdayShort, FaceConfig::DayOfMonth, FaceConfig::MonthShort,
                                                FaceConfig::HijriDayOfMonth, FaceConfig::HijriMonthShort,
                                                FaceConfig::HourCorner, FaceConfig::MinuteCorner };
    bool sameAsCorner = true;
    for ( int i = 0; i < 7; ++i ) {
        char line[32];
        char corner[32];
        FaceText::text( lines[i], 0, in, line, sizeof( line ) );
        FaceText::corner( corners[i], in, corner, sizeof( corner ) );
        sameAsCorner = sameAsCorner && line[0] != '\0' && strcmp( line, corner ) == 0;
    }
    check( "short items in a line as in a corner", sameAsCorner );

    // No clock set yet: every slot stays empty but the emergency card, which a helper may need
    // on a watch that lost its time too.
    FaceText::Inputs none = in;
    none.timeValid = false;
    bool allEmpty = true;
    for ( int item = 1; item < FaceConfig::TextItemCount; ++item ) {
        if ( item == FaceConfig::EmergencyText ) {
            continue;
        }
        FaceText::text( static_cast<FaceConfig::TextItem>( item ), 0, none, text, sizeof( text ) );
        allEmpty = allEmpty && text[0] == '\0';
    }
    for ( int item = 1; item < FaceConfig::CornerItemCount; ++item ) {
        FaceText::corner( static_cast<FaceConfig::CornerItem>( item ), none, text, sizeof( text ) );
        allEmpty = allEmpty && text[0] == '\0';
    }
    check( "no time: all empty", allEmpty );
    FaceText::text( FaceConfig::EmergencyText, 0, none, text, sizeof( text ) );
    checkText( "no time: the emergency card still shows", text, "A Rh+" );
    // No location: the sky slots stay empty.
    FaceText::Inputs noSky = in;
    noSky.skyValid = false;
    noSky.altitudesValid = false;
    FaceText::text( FaceConfig::NextSunEvent, 0, noSky, text, sizeof( text ) );
    checkText( "no sky: sun event empty", text, "" );
    FaceText::text( FaceConfig::SunAltitude, 0, noSky, text, sizeof( text ) );
    checkText( "no sky: altitude empty", text, "" );
    FaceText::Inputs noBattery = in;
    noBattery.batteryPercent = -1;
    FaceText::text( FaceConfig::BatteryPercent, 0, noBattery, text, sizeof( text ) );
    checkText( "no battery: empty", text, "" );

    // The table: after the last prayer of the day comes tomorrow's Imsak; past the table, nothing.
    FaceTable::Event event = {};
    check( "table: next prayer at 23:59", table.next( localToUtc( 2026, 6, 21, 23, 59, offset ), true, event ) );
    check( "table: tomorrow's imsak", event.kind == FaceTable::PrayerEvent + Sky::Imsak &&
                                          event.time > localToUtc( 2026, 6, 22, 3, 0, offset ) &&
                                          event.time < localToUtc( 2026, 6, 22, 4, 0, offset ) );
    check( "table: tomorrow's sunrise after sunset", table.next( localToUtc( 2026, 6, 21, 21, 0, offset ), false, event ) &&
                                                         event.kind == FaceTable::SunriseEvent );
    check( "table: past its end", !table.next( localToUtc( 2026, 6, 23, 0, 1, offset ), true, event ) );
    // The always-on face keeps the table on today and tomorrow: a minute wake on a new local day
    // computes the events again (without the sky day) and moves the Hijri date on while the month
    // certainly lasts (Review Focus 5).
    FaceTable later = table;
    later.follow( day, offset, istanbul, 17 );
    check( "follow: same day keeps the events", later.count == table.count && later.validUntil == table.validUntil );
    later.follow( day + 1, offset, istanbul, 17 );
    check( "follow: next evening still has a sunrise",
           later.next( localToUtc( 2026, 6, 22, 21, 30, offset ), false, event ) && event.kind == FaceTable::SunriseEvent );
    check( "follow: next late evening still has a prayer",
           later.next( localToUtc( 2026, 6, 22, 23, 30, offset ), true, event ) &&
               event.kind == FaceTable::PrayerEvent + Sky::Imsak );
    later.follow( day + 2, offset, istanbul, 17 );
    check( "follow: two days on, the next imsak", later.next( localToUtc( 2026, 6, 23, 23, 59, offset ), true, event ) &&
                                                     event.time > localToUtc( 2026, 6, 24, 3, 0, offset ) &&
                                                     event.time < localToUtc( 2026, 6, 24, 4, 0, offset ) );
    check( "follow: keeps the counters", strcmp( later.counterNames[0], "Ramazan" ) == 0 );
    FaceText::Inputs laterIn = in;
    laterIn.table = &later;
    laterIn.day = 23;
    FaceText::text( FaceConfig::HijriDate, 0, laterIn, text, sizeof( text ) );
    checkText( "follow: Hijri date two days on", text, "7 Muharram 1448" );
    FaceTable monthEnd = table;
    monthEnd.hijriDay = 29;
    monthEnd.follow( day + 1, offset, istanbul, 17 );
    FaceText::Inputs monthEndIn = in;
    monthEndIn.table = &monthEnd;
    monthEndIn.day = 22;
    FaceText::text( FaceConfig::HijriDate, 0, monthEndIn, text, sizeof( text ) );
    // The day after the 29th is the 30th or the 1st: an "O", like the new Moon, keeps the month.
    checkText( "follow: after day 29 the Hijri day is an O", text, "O Muharram 1448" );
    FaceText::corner( FaceConfig::HijriDayOfMonth, monthEndIn, text, sizeof( text ) );
    checkText( "follow: Hijri day corner after day 29", text, "O" );
    FaceTable lastDay = table;
    lastDay.hijriDay = 29;
    FaceText::Inputs lastDayIn = in;
    lastDayIn.table = &lastDay;
    lastDayIn.day = 22;
    FaceText::text( FaceConfig::HijriDate, 0, lastDayIn, text, sizeof( text ) );
    checkText( "after midnight past day 29: an O", text, "O Muharram 1448" );

    // The default pages' texts fit their slots in both languages, except a name before a number
    // (the next event or prayer, a city, a day counter), which loses letters of its name only: the
    // defaults are the owner's own pages (2026-11-19), whose first page cuts "Gün batımı" in
    // Turkish.
    FaceConfig::Pages defaults = {};
    FaceConfig::defaults( defaults );
    const Language::Id languages[2] = { Language::English, Language::Turkish };
    for ( int l = 0; l < 2; ++l ) {
        Language::set( languages[l] );
        for ( int p = 0; p < defaults.count; ++p ) {
            const FaceConfig::Page &page = defaults.pages[p];
            for ( int slot = 0; slot < FaceConfig::TextSlotCount; ++slot ) {
                FaceText::text( static_cast<FaceConfig::TextItem>( page.items[slot] ), page.parameters[slot], in, text,
                                sizeof( text ) );
                const int width = FaceConfig::textWidth( page, slot );
                char name[96];
                snprintf( name, sizeof( name ), "default page %d slot %d fits (%s)", p, slot, text );
                const int item = page.items[slot];
                const bool namedNumber = item == FaceConfig::NextSunEvent || item == FaceConfig::NextPrayer ||
                                         item == FaceConfig::CityTime || item == FaceConfig::DayCounter;
                check( name, namedNumber || FaceText::fit( text, 16, width ) == static_cast<int>( strlen( text ) ) );
            }
        }
    }

    // A text cut to its slot loses letters of its name, never digits of its number: "Eve dönüş 1"
    // would tell a wrong count.
    FaceTable trip = table;
    trip.setCounter( 0, "Eve dönüş", day + 12 );
    trip.setCity( 0, "Rio de Janeiro", { -180, { 0, 0 }, { 0, 0 } } );
    FaceText::Inputs tripIn = in;
    tripIn.table = &trip;
    FaceText::text( FaceConfig::DayCounter, 0, tripIn, text, sizeof( text ), 11 );
    checkText( "counter cut keeps the number", text, "Eve dönü 12" );
    FaceText::text( FaceConfig::CityTime, 0, tripIn, text, sizeof( text ), 11 );
    checkText( "city cut keeps the time", text, "Rio d 13:19" );
    FaceText::text( FaceConfig::NextSunEvent, 0, tripIn, text, sizeof( text ), 11 );
    checkText( "event cut keeps the time left", text, "Gün ba 1:21" );
    FaceText::text( FaceConfig::DayCounter, 0, tripIn, text, sizeof( text ), 20 );
    checkText( "counter that fits stays whole", text, "Eve dönüş 12" );

    // Whole characters that fit a width (Review Focus 1).
    check( "fit: ten characters", FaceText::fit( "21 Haziran Pazar", 16, 160 ) == 10 );
    check( "fit: Turkish letters count once", FaceText::fit( "Gün batımı", 16, 64 ) == 5 );
    check( "fit: all of a short text", FaceText::fit( "21 Haz", 16, 160 ) == 6 );

    printf( "Face text: %d checks, %d failures\n", s_checks, s_failures );
    return s_failures == 0 ? 0 : 1;
}
