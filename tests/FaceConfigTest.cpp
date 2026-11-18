// Host test of main/FaceConfig.cpp. Build and run: sh tests/run_host_tests.sh

#include <cstdio>
#include <cstring>

#include "FaceConfig.h"

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
    FaceConfig::Pages pages = {};
    FaceConfig::defaults( pages );
    check( "defaults: two pages", pages.count == 2 );
    check( "defaults: valid", FaceConfig::valid( pages ) );
    const FaceConfig::Page &first = pages.pages[0];
    check( "defaults: large", first.layout == FaceConfig::Large );
    check( "defaults: digital", first.clock == FaceConfig::DigitalClock );
    check( "defaults: seven segment", first.digital == FaceConfig::SevenSegment );
    check( "defaults: weekday icon top left", first.items[FaceConfig::TopLeft] == FaceConfig::WeekdayIcon );

    // Slot widths: the edges between the gauges' ends; an inner slot narrows when a corner of its
    // row is in use (the corners share its row).
    check( "width: top edge", FaceConfig::textWidth( first, FaceConfig::TopEdge ) == 176 );
    check( "width: bottom edge", FaceConfig::textWidth( first, FaceConfig::BottomEdge ) == 176 );
    FaceConfig::Page plain = {};
    check( "width: inner without corners", FaceConfig::textWidth( plain, FaceConfig::TopInner ) == 320 );
    plain.items[FaceConfig::TopRight] = FaceConfig::DayOfMonth;
    check( "width: inner beside a corner", FaceConfig::textWidth( plain, FaceConfig::TopInner ) == 208 );
    check( "width: other row unchanged", FaceConfig::textWidth( plain, FaceConfig::BottomInner ) == 320 );
    check( "width: not a text slot", FaceConfig::textWidth( plain, FaceConfig::TopLeft ) == 0 );

    char text[512];
    const int length = FaceConfig::encode( pages, text, sizeof( text ) );
    check( "encode: fits", length > 0 );
    FaceConfig::Pages decoded = {};
    check( "decode: encoded text", FaceConfig::decode( text, decoded ) );
    check( "decode: same pages", memcmp( &decoded, &pages, sizeof( pages ) ) == 0 );
    check( "encode: too small a buffer", FaceConfig::encode( pages, text, 8 ) == -1 );

    FaceConfig::Pages clamped = {};
    check( "decode: out of range", FaceConfig::decode( "0,0,9,0,0,1,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0,255", clamped ) );
    check( "decode: one page", clamped.count == 1 );
    check( "decode: digital style clamped", clamped.pages[0].digital == 0 );
    check( "decode: text slot kept", clamped.pages[0].items[FaceConfig::TopEdge] == 1 );
    check( "decode: corner kept", clamped.pages[0].items[FaceConfig::TopLeft] == 5 );
    check( "decode: clamped pages valid", FaceConfig::valid( clamped ) );

    FaceConfig::Pages untouched = pages;
    check( "decode: garbage refused", !FaceConfig::decode( "garbage", untouched ) );
    check( "decode: empty refused", !FaceConfig::decode( "", untouched ) );
    check( "decode: truncated page refused", !FaceConfig::decode( "0,0,1,0,0,1,2,3", untouched ) );
    check( "decode: refused leaves the pages", memcmp( &untouched, &pages, sizeof( pages ) ) == 0 );

    FaceConfig::Pages hidden = pages;
    for ( int i = 0; i < hidden.count; ++i ) {
        hidden.pages[i].flags |= FaceConfig::HiddenFlag;
    }
    int list[FaceConfig::MaxPages] = {};
    check( "shown: all hidden leaves one", FaceConfig::shown( hidden, list ) == 1 );
    check( "shown: the first", list[0] == 0 );
    hidden.pages[1].flags &= static_cast<uint8_t>( ~FaceConfig::HiddenFlag );
    check( "shown: one visible", FaceConfig::shown( hidden, list ) == 1 && list[0] == 1 );

    FaceConfig::Page slots = pages.pages[0];
    slots.items[FaceConfig::TopEdge] = FaceConfig::Seconds;
    slots.items[FaceConfig::TopInner] = FaceConfig::LongDate;
    slots.items[FaceConfig::TopRight] = FaceConfig::SecondCorner;
    slots.alwaysOn = 0xFF;
    check( "always-on: never the seconds", !FaceConfig::alwaysOnSlot( slots, FaceConfig::TopEdge ) );
    check( "always-on: never the second corner", !FaceConfig::alwaysOnSlot( slots, FaceConfig::TopRight ) );
    check( "always-on: a date when marked", FaceConfig::alwaysOnSlot( slots, FaceConfig::TopInner ) );
    slots.alwaysOn = 0;
    check( "always-on: not when unmarked", !FaceConfig::alwaysOnSlot( slots, FaceConfig::TopInner ) );

    FaceConfig::Pages bad = pages;
    bad.count = 0;
    check( "valid: no pages", !FaceConfig::valid( bad ) );
    bad = pages;
    bad.pages[0].items[FaceConfig::TopLeft] = 200;
    check( "valid: corner out of range", !FaceConfig::valid( bad ) );
    FaceConfig::clamp( bad );
    check( "clamp: corner to none", bad.pages[0].items[FaceConfig::TopLeft] == FaceConfig::NoCorner );

    printf( "Face config: %d checks, %d failures\n", s_checks, s_failures );
    return s_failures == 0 ? 0 : 1;
}
