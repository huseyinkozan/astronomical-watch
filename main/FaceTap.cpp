#include "FaceTap.h"

namespace {

// A rectangle from (x1, y1) up to, not including, (x2, y2).
struct Box
{
    int x1;
    int y1;
    int x2;
    int y2;
};

bool inside( const Box &box, int x, int y )
{
    return x >= box.x1 && x < box.x2 && y >= box.y1 && y < box.y2;
}

int distance2( int x1, int y1, int x2, int y2 )
{
    return ( x1 - x2 ) * ( x1 - x2 ) + ( y1 - y2 ) * ( y1 - y2 );
}

const int s_bodyRadius = 26;
const int s_cornerHalf = 32;  // the corner icons' 64 px boxes
// Each corner gauge's L, by corner: along the side edge, then along the top or bottom edge.
const Box s_gaugeZones[4][2] = { { { 0, 0, 34, 150 }, { 34, 0, 120, 38 } },
                                 { { 376, 0, 410, 150 }, { 290, 0, 376, 38 } },
                                 { { 0, 352, 34, 502 }, { 34, 464, 120, 502 } },
                                 { { 376, 352, 410, 502 }, { 290, 464, 376, 502 } } };
const Box s_textZones[FaceConfig::TextSlotCount] = { { 120, 8, 290, 42 }, { 100, 43, 310, 78 },
                                                     { 100, 424, 310, 459 }, { 120, 460, 290, 495 } };
// The ring between the arcs, around the face's centre.
const int s_ringX = 205;
const int s_ringY = 251;
const int s_ringInner = 95;
const int s_ringOuter = 155;
const Box s_clockZone = { 110, 190, 300, 315 };

}  // namespace

FaceTap::Target FaceTap::gaugeTarget( int gauge )
{
    switch ( gauge ) {
    case FaceConfig::DayNightGauge:
    case FaceConfig::NextSunriseGauge:
    case FaceConfig::NextSunsetGauge:
    case FaceConfig::SunAltitudeGauge:
        return SunPart;
    case FaceConfig::BatteryGauge:
        return BatteryPart;
    case FaceConfig::MoonPassGauge:
    case FaceConfig::MoonPhaseGauge:
    case FaceConfig::NextMoonriseGauge:
    case FaceConfig::NextMoonsetGauge:
        return MoonPart;
    case FaceConfig::NextPrayerGauge:
        return PrayerPart;
    case FaceConfig::WeekGauge:
    case FaceConfig::MonthGauge:
    case FaceConfig::YearGauge:
        return GregorianPart;
    case FaceConfig::HijriMonthGauge:
        return HijriPart;
    case FaceConfig::DayCounterGauge:
        return DaysPart;
    case FaceConfig::HourGauge:
    case FaceConfig::MinuteGauge:
    case FaceConfig::SecondGauge:
        return TimePart;
    case FaceConfig::TimeoutGauge:
        return SettingsPart;
    default:
        return NoTarget;
    }
}

FaceTap::Target FaceTap::cornerTarget( int item, bool stopwatchRunning, bool countdownRunning )
{
    switch ( item ) {
    case FaceConfig::MoonIcon:
    case FaceConfig::MoonAltitudeCorner:
        return MoonPart;
    case FaceConfig::WeekdayIcon:
    case FaceConfig::WeekdayShort:
    case FaceConfig::DayOfMonth:
    case FaceConfig::MonthShort:
        return GregorianPart;
    case FaceConfig::HijriDayOfMonth:
    case FaceConfig::HijriMonthShort:
        return HijriPart;
    case FaceConfig::SunAltitudeCorner:
        return SunPart;
    case FaceConfig::HourCorner:
    case FaceConfig::MinuteCorner:
    case FaceConfig::SecondCorner:
    case FaceConfig::TimeCorner:
        return TimePart;
    case FaceConfig::AlarmDays:
    case FaceConfig::TimeIcon:
    case FaceConfig::TimerGlass:
        // What the icon shows: the timer that runs (the countdown first), else the alarms.
        return countdownRunning ? CountdownPart : ( stopwatchRunning ? StopwatchPart : AlarmsPart );
    default:
        return NoTarget;
    }
}

FaceTap::Target FaceTap::textTarget( int item )
{
    switch ( item ) {
    case FaceConfig::LongDate:
    case FaceConfig::ShortDate:
    case FaceConfig::WeekdayText:
    case FaceConfig::DayOfMonthText:
    case FaceConfig::MonthText:
        return GregorianPart;
    case FaceConfig::HijriDate:
    case FaceConfig::HijriDayText:
    case FaceConfig::HijriMonthText:
        return HijriPart;
    case FaceConfig::NextSunEvent:
    case FaceConfig::SunAltitude:
        return SunPart;
    case FaceConfig::NextPrayer:
        return PrayerPart;
    case FaceConfig::BatteryPercent:
        return BatteryPart;
    case FaceConfig::CityTime:
        return WorldPart;
    case FaceConfig::DayCounter:
        return DaysPart;
    case FaceConfig::MoonAltitude:
        return MoonPart;
    case FaceConfig::TimeText:
    case FaceConfig::Seconds:
    case FaceConfig::HourText:
    case FaceConfig::MinuteText:
        return TimePart;
    default:
        return NoTarget;
    }
}

FaceTap::Target FaceTap::at( const Input &input, int x, int y )
{
    const FaceConfig::Page &page = *input.page;
    const bool arcs = page.layout == FaceConfig::FullArcs || page.layout == FaceConfig::HalfArcs;
    if ( arcs ) {
        const int limit = s_bodyRadius * s_bodyRadius;
        const int sun = input.sunShown ? distance2( x, y, input.sunX, input.sunY ) : limit + 1;
        const int moon = input.moonShown ? distance2( x, y, input.moonX, input.moonY ) : limit + 1;
        // The nearer of the two when their circles overlap.
        if ( sun <= limit && sun <= moon ) {
            return SunPart;
        }
        if ( moon <= limit ) {
            return MoonPart;
        }
    }
    for ( int i = 0; i < 4; ++i ) {
        const int item = page.items[FaceConfig::TopLeft + i];
        const int dx = x - FaceConfig::CornerX[i];
        const int dy = y - FaceConfig::CornerY[i];
        const bool inBox = dx >= -s_cornerHalf && dx < s_cornerHalf && dy >= -s_cornerHalf && dy < s_cornerHalf;
        if ( inBox && item != FaceConfig::NoCorner ) {
            return cornerTarget( item, input.stopwatchRunning, input.countdownRunning );
        }
        // A corner without an item belongs to its gauge.
        if ( inBox || inside( s_gaugeZones[i][0], x, y ) || inside( s_gaugeZones[i][1], x, y ) ) {
            return gaugeTarget( input.gauges[i] );
        }
    }
    for ( int i = 0; i < FaceConfig::TextSlotCount; ++i ) {
        if ( inside( s_textZones[i], x, y ) ) {
            return textTarget( page.items[i] );
        }
    }
    if ( arcs ) {
        const int d = distance2( x, y, s_ringX, s_ringY );
        if ( d >= s_ringInner * s_ringInner && d <= s_ringOuter * s_ringOuter ) {
            return SkyPart;
        }
    }
    return inside( s_clockZone, x, y ) ? WorldPart : NoTarget;
}
