// Host test of main/TableText.cpp. Build and run: sh tests/run_host_tests.sh

#include <cstdio>
#include <cstring>

#include "TableText.h"

static int s_checks = 0;
static int s_failures = 0;

static void check( const char *name, bool condition )
{
    ++s_checks;
    if ( !condition ) {
        printf( "FAIL %s\n", name );
        ++s_failures;
    }
}

int main()
{
    TableText table;
    check( "starts unchanged", !table.takeChanged() );
    check( "a new text changes", table.set( 0, 1, "05:32" ) );
    check( "the change is reported once", table.takeChanged() && !table.takeChanged() );
    check( "the same text changes nothing", !table.set( 0, 1, "05:32" ) && !table.takeChanged() );
    check( "the text is kept", std::strcmp( table.text( 0, 1 ), "05:32" ) == 0 );

    char longText[TableText::CellSize * 2] = {};
    std::memset( longText, 'x', sizeof( longText ) - 1 );
    table.set( 1, 0, longText );
    check( "a long text is cut and terminated", std::strlen( table.text( 1, 0 ) ) == TableText::CellSize - 1 );
    // A cut never splits a UTF-8 letter: "ş" is two bytes.
    char turkish[TableText::CellSize + 4] = {};
    std::memset( turkish, 'a', TableText::CellSize - 2 );
    std::strcat( turkish, "\xC5\x9F" );
    table.set( 1, 1, turkish );
    check( "a cut keeps whole letters", std::strlen( table.text( 1, 1 ) ) == TableText::CellSize - 2 );

    check( "a row out of range writes nothing", !table.set( TableText::MaxRows, 0, "x" ) && !table.set( -1, 0, "x" ) );
    check( "a cell out of range writes nothing", !table.set( 0, TableText::Cells, "x" ) );
    check( "out of range reads an empty text", std::strcmp( table.text( TableText::MaxRows, 0 ), "" ) == 0 );
    table.takeChanged();
    check( "null clears the cell", table.set( 0, 1, nullptr ) && std::strcmp( table.text( 0, 1 ), "" ) == 0 );

    table.takeChanged();
    check( "new colours change", table.setColours( 2, 0xFFB040, 0xFFB040 ) && table.takeChanged() );
    check( "the same colours change nothing", !table.setColours( 2, 0xFFB040, 0xFFB040 ) && !table.takeChanged() );
    check( "the colours are kept", table.nameColour( 2 ) == 0xFFB040 && table.valueColour( 2 ) == 0xFFB040 );
    check( "default colours: grey names, white values",
           table.nameColour( 3 ) == TableText::Grey && table.valueColour( 3 ) == TableText::White );

    // Like LVGL's dots mode: a text longer than the room ends in "..." within the room.
    char shown[TableText::CellSize] = {};
    TableText::fitWithDots( "Istanbul", 13, shown, sizeof( shown ) );
    check( "a text that fits stays", std::strcmp( shown, "Istanbul" ) == 0 );
    TableText::fitWithDots( "41.0082N 28.9784E", 13, shown, sizeof( shown ) );
    check( "a long text ends in dots", std::strcmp( shown, "41.0082N 2..." ) == 0 );
    TableText::fitWithDots( "\xC4\xB0stanbul, T\xC3\xBCrkiye", 13, shown, sizeof( shown ) );
    check( "dots count letters, not bytes", std::strcmp( shown, "\xC4\xB0stanbul, ..." ) == 0 );

    printf( "Table text: %d checks, %d failures\n", s_checks, s_failures );
    return s_failures == 0 ? 0 : 1;
}
