// Host test of the always-on face's small glyphs (main/AodGlyphs.c, tools/make_aod_glyphs.py):
// every text a face slot can show, in both languages, has a glyph for each of its characters.
// A missing one would leave a gap in the always-on face. Build and run:
// sh tests/run_host_tests.sh

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include "Dates.h"
#include "FaceTable.h"
#include "FaceText.h"
#include "Language.h"
#include "Sky.h"

static int s_checks = 0;
static int s_failures = 0;
static std::set<unsigned> s_codes;

// The code points listed in aodSmallCodes of the generated C file.
static bool readCodes( const char *path )
{
    FILE *file = fopen( path, "r" );
    if ( file == nullptr ) {
        return false;
    }
    std::string text;
    char buffer[4096];
    size_t read = 0;
    while ( ( read = fread( buffer, 1, sizeof( buffer ), file ) ) > 0 ) {
        text.append( buffer, read );
    }
    fclose( file );
    const size_t start = text.find( "aodSmallCodes[AOD_SMALL_COUNT] = {" );
    if ( start == std::string::npos ) {
        return false;
    }
    const size_t end = text.find( '}', start );
    const char *p = text.c_str() + text.find( '{', start ) + 1;
    const char *last = text.c_str() + end;
    while ( p < last ) {
        char *next = nullptr;
        const unsigned long code = strtoul( p, &next, 16 );
        if ( next == p ) {
            ++p;
            continue;
        }
        s_codes.insert( static_cast<unsigned>( code ) );
        p = next;
    }
    return !s_codes.empty();
}

// Every character of the UTF-8 `text` has a small glyph.
static void checkText( const char *what, const char *text )
{
    ++s_checks;
    for ( int i = 0; text[i] != '\0'; ) {
        const unsigned char lead = static_cast<unsigned char>( text[i] );
        unsigned code = lead;
        int extra = 0;
        if ( lead >= 0xE0 ) {
            code = lead & 0x0F;
            extra = 2;
        } else if ( lead >= 0xC0 ) {
            code = lead & 0x1F;
            extra = 1;
        }
        ++i;
        for ( int k = 0; k < extra && text[i] != '\0'; ++k ) {
            code = ( code << 6 ) | ( static_cast<unsigned char>( text[i++] ) & 0x3F );
        }
        if ( s_codes.count( code ) == 0 ) {
            printf( "FAIL %s: \"%s\" has no glyph for U+%04X\n", what, text, code );
            ++s_failures;
            return;
        }
    }
}

int main( int argc, char **argv )
{
    if ( argc < 2 || !readCodes( argv[1] ) ) {
        printf( "FAIL no aodSmallCodes in %s\n", argc < 2 ? "(no file given)" : argv[1] );
        return 1;
    }
    const int offset = 180;
    const Astronomy::Observer istanbul = { 41.0082, 28.9784 };
    const int day = Dates::dayNumber( 2026, 6, 21 );
    const Sky::Day sky = Sky::computeDay( day, offset, istanbul, Hijri::LocalCrescent, 17 );
    FaceTable table = {};
    table.build( sky, offset, istanbul, 17 );
    // Names the owner types in the portal: the watch font's characters (tools/make_fonts.sh).
    // (FaceTable::NameLength bytes at most, as DayCounters and WorldClock keep them.)
    table.setCounter( 0, "Ramazan ÇçĞğİı", Dates::dayNumber( 2027, 2, 8 ) );
    table.setCounter( 1, "ÖöŞşÜü", Dates::dayNumber( 2027, 2, 8 ) );
    table.setCity( 0, "!\"#$%&'()*+,-./;<=>?@[\\]^_`{|}~", { 60, { 0, 0 }, { 0, 0 } } );
    table.setCity( 1, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", { 60, { 0, 0 }, { 0, 0 } } );
    table.setCity( 2, "abcdefghijklmnopqrstuvwxyz", { 60, { 0, 0 }, { 0, 0 } } );

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
    in.now = static_cast<int64_t>( day ) * 86400 + ( 19 * 60 + 19 - offset ) * 60;
    in.table = &table;
    in.batteryPercent = 68;
    in.sunAltitude = 13;
    in.moonAltitude = 25;
    in.altitudesValid = true;

    char text[64];
    for ( int language = 0; language < 2; ++language ) {
        Language::set( language );
        for ( int item = 1; item < FaceConfig::TextItemCount; ++item ) {
            for ( int parameter = 0; parameter < FaceConfig::CounterCount; ++parameter ) {
                FaceText::text( static_cast<FaceConfig::TextItem>( item ), parameter, in, text, sizeof( text ) );
                checkText( "slot text", text );
            }
        }
        for ( int item = 1; item < FaceConfig::CornerItemCount; ++item ) {
            FaceText::corner( static_cast<FaceConfig::CornerItem>( item ), in, text, sizeof( text ) );
            checkText( "corner text", text );
        }
        // The names the slots take on other days.
        for ( int i = 0; i < 7; ++i ) {
            checkText( "day name", Language::dayName( i ) );
            checkText( "short day", Language::shortDay( i ) );
        }
        for ( int i = 1; i <= 12; ++i ) {
            checkText( "month name", Language::monthName( i ) );
            checkText( "short month", Language::shortMonth( i ) );
            checkText( "Hijri month", Language::hijriMonth( i ) );
        }
        const Language::Text events[8] = { Language::Sunrise, Language::Sunset,  Language::Imsak,   Language::PrayerSunrise,
                                           Language::Dhuhr,   Language::Asr,     Language::Maghrib, Language::Isha };
        for ( int i = 0; i < 8; ++i ) {
            checkText( "event name", Language::text( events[i] ) );
        }
    }
    checkText( "digits and signs", "0123456789 :-%°" );

    printf( "Always-on small glyphs: %d checks, %d failures\n", s_checks, s_failures );
    return s_failures == 0 ? 0 : 1;
}
