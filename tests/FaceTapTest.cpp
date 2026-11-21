// Host test of main/FaceTap.cpp. Build and run: sh tests/run_host_tests.sh

#include <cstdio>

#include "FaceTap.h"

static int failures = 0;
static int checks = 0;

static void check( const char *name, int value, int expected )
{
    ++checks;
    if ( value != expected ) {
        printf( "FAIL %s: %d, expected %d\n", name, value, expected );
        ++failures;
    }
}

int main()
{
    FaceConfig::Page page = {};
    page.layout = FaceConfig::FullArcs;
    page.items[FaceConfig::TopEdge] = FaceConfig::LongDate;
    page.items[FaceConfig::TopInner] = FaceConfig::NoText;
    page.items[FaceConfig::BottomInner] = FaceConfig::NextPrayer;
    page.items[FaceConfig::BottomEdge] = FaceConfig::CityTime;
    page.items[FaceConfig::TopLeft] = FaceConfig::MoonIcon;
    page.items[FaceConfig::TopRight] = FaceConfig::NoCorner;
    page.items[FaceConfig::BottomLeft] = FaceConfig::TimeIcon;
    page.items[FaceConfig::BottomRight] = FaceConfig::HijriDayOfMonth;
    const uint8_t gauges[4] = { FaceConfig::DayNightGauge, FaceConfig::BatteryGauge, FaceConfig::MoonPassGauge,
                                FaceConfig::NoGauge };
    FaceTap::Input input = { &page, gauges, true, 205, 101, true, 327, 251, false, false };

    // The bodies first, the nearer one when both are close.
    check( "sun", FaceTap::at( input, 210, 110 ), FaceTap::SunPart );
    check( "moon", FaceTap::at( input, 320, 260 ), FaceTap::MoonPart );
    input.moonX = 220;
    input.moonY = 101;
    check( "nearer body", FaceTap::at( input, 216, 101 ), FaceTap::MoonPart );
    input.moonShown = false;
    check( "hidden body: the ring", FaceTap::at( input, 327, 251 ), FaceTap::SkyPart );

    // Corner icons, and a corner without an item falls through to its gauge.
    check( "moon icon", FaceTap::at( input, 66, 70 ), FaceTap::MoonPart );
    check( "empty corner: battery gauge", FaceTap::at( input, 344, 70 ), FaceTap::BatteryPart );
    check( "time icon: alarms", FaceTap::at( input, 66, 432 ), FaceTap::AlarmsPart );
    input.stopwatchRunning = true;
    check( "time icon: stopwatch", FaceTap::at( input, 66, 432 ), FaceTap::StopwatchPart );
    input.countdownRunning = true;
    check( "time icon: countdown first", FaceTap::at( input, 66, 432 ), FaceTap::CountdownPart );
    check( "hijri day corner", FaceTap::at( input, 344, 432 ), FaceTap::HijriPart );

    // The gauges' L zones, the very corner included; a corner without a gauge does nothing.
    check( "top left corner pixel", FaceTap::at( input, 0, 0 ), FaceTap::SunPart );
    check( "top left along the top", FaceTap::at( input, 110, 20 ), FaceTap::SunPart );
    check( "top right along the side", FaceTap::at( input, 400, 140 ), FaceTap::BatteryPart );
    check( "bottom left", FaceTap::at( input, 20, 480 ), FaceTap::MoonPart );
    check( "bottom right without a gauge", FaceTap::at( input, 400, 500 ), FaceTap::NoTarget );

    // Text slots; an empty one does nothing.
    check( "top edge date", FaceTap::at( input, 205, 25 ), FaceTap::GregorianPart );
    check( "empty top slot", FaceTap::at( input, 205, 60 ), FaceTap::NoTarget );
    check( "bottom prayer", FaceTap::at( input, 205, 442 ), FaceTap::PrayerPart );
    check( "bottom edge city", FaceTap::at( input, 205, 478 ), FaceTap::WorldPart );

    // The ring between the arcs, the clock inside it, nothing between the zones.
    check( "ring", FaceTap::at( input, 205 - 120, 251 ), FaceTap::SkyPart );
    check( "clock", FaceTap::at( input, 205, 251 ), FaceTap::WorldPart );
    check( "outside every zone", FaceTap::at( input, 30, 251 ), FaceTap::NoTarget );

    // The Large template: no bodies and no ring, the clock box only.
    page.layout = FaceConfig::Large;
    input.moonShown = true;
    check( "large: no body", FaceTap::at( input, 210, 110 ), FaceTap::NoTarget );
    check( "large: no ring", FaceTap::at( input, 205 - 120, 251 ), FaceTap::NoTarget );
    check( "large: clock", FaceTap::at( input, 150, 200 ), FaceTap::WorldPart );

    // Every item kind has its target.
    check( "gauge: second", FaceTap::gaugeTarget( FaceConfig::SecondGauge ), FaceTap::TimePart );
    check( "gauge: counter", FaceTap::gaugeTarget( FaceConfig::DayCounterGauge ), FaceTap::DaysPart );
    check( "corner: old hourglass", FaceTap::cornerTarget( FaceConfig::TimerGlass, false, false ), FaceTap::AlarmsPart );
    check( "text: minute", FaceTap::textTarget( FaceConfig::MinuteText ), FaceTap::TimePart );
    check( "text: none", FaceTap::textTarget( FaceConfig::NoText ), FaceTap::NoTarget );

    printf( "Face tap: %d checks, %d failures\n", checks, failures );
    return failures == 0 ? 0 : 1;
}
