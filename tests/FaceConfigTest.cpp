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

int main( int argc, char **argv )
{
    FaceConfig::Pages pages = {};
    FaceConfig::defaults( pages );
    // The owner's pages of 2026-11-19: Full arcs, Half arcs, Large seven segment, Full arcs with an
    // analog Classic clock and the time on top, Large JetBrains Mono.
    check( "defaults: five pages", pages.count == 5 );
    check( "defaults: valid", FaceConfig::valid( pages ) );
    const FaceConfig::Page &full = pages.pages[0];
    check( "defaults: full arcs, seven segment", full.layout == FaceConfig::FullArcs && full.clock == FaceConfig::DigitalClock &&
                                                     full.digital == FaceConfig::SevenSegment );
    const FaceConfig::Page &half = pages.pages[1];
    check( "defaults: half arcs, Mono, Milky Way", half.layout == FaceConfig::HalfArcs && half.digital == FaceConfig::MonoDigits &&
                                                       ( half.flags & FaceConfig::MilkyWayFlag ) != 0 );
    const FaceConfig::Page &first = pages.pages[2];
    check( "defaults: large", first.layout == FaceConfig::Large );
    check( "defaults: digital", first.clock == FaceConfig::DigitalClock );
    check( "defaults: seven segment", first.digital == FaceConfig::SevenSegment );
    check( "defaults: weekday icon top left", first.items[FaceConfig::TopLeft] == FaceConfig::WeekdayIcon );
    const FaceConfig::Page &analog = pages.pages[3];
    check( "defaults: full arcs analog Classic", analog.layout == FaceConfig::FullArcs && analog.clock == FaceConfig::AnalogClock &&
                                                     analog.analog == FaceConfig::ClassicHands );
    check( "defaults: Classic with the seconds hand", ( analog.flags & FaceConfig::SecondsHandFlag ) != 0 );
    check( "defaults: the time on the analog page's top edge", analog.items[FaceConfig::TopEdge] == FaceConfig::TimeText );
    check( "defaults: the analog page shows alarm 1's days", analog.items[FaceConfig::TopRight] == FaceConfig::AlarmDays &&
                                                                 analog.parameters[FaceConfig::TopRight] == 0 );
    check( "defaults: full arcs without the top right corner", full.items[FaceConfig::TopRight] == FaceConfig::NoCorner );
    check( "defaults: full arcs with the Milky Way", ( full.flags & FaceConfig::MilkyWayFlag ) != 0 );
    check( "defaults: full and half arcs with the planets",
           ( full.flags & FaceConfig::PlanetsFlag ) != 0 && ( half.flags & FaceConfig::PlanetsFlag ) != 0 );
    check( "defaults: large pages without the planets", ( first.flags & FaceConfig::PlanetsFlag ) == 0 );
    bool wifi = true;
    for ( int i = 0; i < 4; ++i ) {
        wifi = wifi && pages.pages[i].items[FaceConfig::BottomRight] == FaceConfig::WifiIcon &&
               !FaceConfig::alwaysOnSlot( pages.pages[i], FaceConfig::BottomRight );
    }
    check( "defaults: the Wi-Fi icon bottom right, not on the always-on face", wifi );
    bool emergency = true;
    for ( int i = 0; i < pages.count; ++i ) {
        emergency = emergency && pages.pages[i].items[FaceConfig::BottomEdge] == FaceConfig::EmergencyText &&
                    FaceConfig::alwaysOnSlot( pages.pages[i], FaceConfig::BottomEdge );
    }
    check( "defaults: the emergency card on every bottom edge, also always-on", emergency );
    check( "defaults: the analog page's short date moved to the top row",
           analog.items[FaceConfig::TopInner] == FaceConfig::ShortDate && FaceConfig::alwaysOnSlot( analog, FaceConfig::TopInner ) );
    check( "defaults: Mono last", pages.pages[4].layout == FaceConfig::Large && pages.pages[4].digital == FaceConfig::MonoDigits );
    check( "defaults: today's corner gauges", pages.gauges[0] == FaceConfig::DayNightGauge &&
                                                  pages.gauges[1] == FaceConfig::BatteryGauge &&
                                                  pages.gauges[2] == FaceConfig::MoonPassGauge &&
                                                  pages.gauges[3] == FaceConfig::TimeoutGauge );

    // The clock's place and size by template.
    const FaceConfig::ClockPlace large = FaceConfig::clockPlace( FaceConfig::Large );
    check( "clock: large", large.centreY == 251 && large.digitHeight == 122 && large.analogRadius == 140 );
    const FaceConfig::ClockPlace fullPlace = FaceConfig::clockPlace( FaceConfig::FullArcs );
    check( "clock: full arcs", fullPlace.centreY == 251 && fullPlace.digitHeight == 64 && fullPlace.analogRadius == 100 );
    const FaceConfig::ClockPlace halfPlace = FaceConfig::clockPlace( FaceConfig::HalfArcs );
    check( "clock: half arcs", halfPlace.centreY == 310 && halfPlace.digitHeight == 64 && halfPlace.analogRadius == 70 );
    check( "clock: unknown template as large", FaceConfig::clockPlace( 9 ).centreY == 251 );

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

    // The corner gauges (global): today's four by default; encoded after the pages.
    const uint8_t todays[4] = { FaceConfig::DayNightGauge, FaceConfig::BatteryGauge, FaceConfig::MoonPassGauge,
                                FaceConfig::TimeoutGauge };
    check( "gauges: today's four", memcmp( pages.gauges, todays, 4 ) == 0 );
    {
        FaceConfig::Pages withGauges = pages;
        withGauges.gauges[0] = FaceConfig::DayCounterGauge;
        withGauges.gaugeParameters[0] = 2 | ( 1 << 2 );  // counter 3, yearly
        withGauges.gauges[3] = FaceConfig::HourGauge;
        withGauges.gaugeParameters[3] = 1;  // 12 hours
        char encoded[600];
        check( "gauges: encode", FaceConfig::encode( withGauges, encoded, sizeof( encoded ) ) > 0 &&
                                     strchr( encoded, '/' ) != nullptr );
        FaceConfig::Pages back = {};
        check( "gauges: decode", FaceConfig::decode( encoded, back ) &&
                                     memcmp( &back, &withGauges, sizeof( back ) ) == 0 );
        FaceConfig::Pages old = {};
        check( "gauges: an older string without them",
               FaceConfig::decode( "0,0,1,0,0,2,5,0,9,2,8,1,4,0,0,0,0,0,0,0,0,209", old ) &&
                   memcmp( old.gauges, todays, 4 ) == 0 );
        FaceConfig::Pages odd = {};
        check( "gauges: unknown items and parameters clamped",
               FaceConfig::decode( "0,0,1,0,0,2,5,0,9,2,8,1,4,0,0,0,0,0,0,0,0,209/99,16,2,3,7,200,0,9", odd ) &&
                   odd.gauges[0] == FaceConfig::NoGauge && odd.gauges[1] == FaceConfig::HourGauge &&
                   odd.gaugeParameters[1] == 0 && odd.gaugeParameters[2] == 0 && odd.gaugeParameters[3] == 0 );
        check( "gauges: a broken gauge part refused", !FaceConfig::decode( "0,0,1,0,0,2,5,0,9,2,8,1,4,0,0,0,0,0,0,0,0,209/1,2", odd ) );
    }

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

    // The alarm corner icons: the alarm days take the alarm (0-3) as their parameter.
    FaceConfig::Pages alarmPages = pages;
    alarmPages.pages[2].items[FaceConfig::TopLeft] = FaceConfig::AlarmDays;
    alarmPages.pages[2].parameters[FaceConfig::TopLeft] = 3;
    alarmPages.pages[2].items[FaceConfig::TopRight] = FaceConfig::TimeIcon;
    alarmPages.pages[2].items[FaceConfig::BottomLeft] = FaceConfig::TimerGlass;
    check( "alarm icons valid", FaceConfig::valid( alarmPages ) );
    // The alarm days and the hourglass of their own, saved before the merges, are the time icon
    // and keep their numbers.
    check( "time icon: three numbers", FaceConfig::isTimeIcon( 13 ) && FaceConfig::isTimeIcon( 14 ) &&
                                           FaceConfig::isTimeIcon( 15 ) && !FaceConfig::isTimeIcon( FaceConfig::TimeCorner ) );
    char saved[512] = {};
    FaceConfig::encode( alarmPages, saved, sizeof( saved ) );
    FaceConfig::Pages reread = {};
    check( "time icon: decode keeps the old number", FaceConfig::decode( saved, reread ) &&
                                                          reread.pages[2].items[FaceConfig::BottomLeft] == FaceConfig::TimerGlass );
    alarmPages.pages[2].parameters[FaceConfig::TopLeft] = 4;
    check( "alarm days: alarm 5 invalid", !FaceConfig::valid( alarmPages ) );

    // The portal preview's fake watch (argv[1]: tools/portal_preview.py) sends the defaults as
    // the watch encodes them, so that the published portal pictures show the watch's boxes.
    if ( argc > 1 ) {
        char encoded[512] = {};
        FaceConfig::Pages defaults = {};
        FaceConfig::defaults( defaults );
        FaceConfig::encode( defaults, encoded, sizeof( encoded ) );
        char wanted[600] = {};
        snprintf( wanted, sizeof( wanted ), "'faceDefaults': '%s'", encoded );
        static char preview[65536] = {};
        FILE *file = fopen( argv[1], "r" );
        const size_t length = file != nullptr ? fread( preview, 1, sizeof( preview ) - 1, file ) : 0;
        if ( file != nullptr ) {
            fclose( file );
        }
        preview[length] = 0;
        check( "portal preview: faceDefaults as encode() writes them", strstr( preview, wanted ) != nullptr );
    }

    printf( "Face config: %d checks, %d failures\n", s_checks, s_failures );
    return s_failures == 0 ? 0 : 1;
}
