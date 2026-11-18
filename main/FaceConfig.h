#pragma once

#include <cstdint>

// The face pages the setup portal composes (docs/superpowers/specs/2026-11-18-face-templates-design.md):
// each page is a template with a clock and eight slots around it, four texts and four corners,
// each slot also shown on the always-on face when its flag is set. The vertical swipe on the
// face steps through the pages that are not hidden. Plain C++ for the host tests; FaceStore
// keeps the pages in NVS.
class FaceConfig
{
public:
    static const int MaxPages = 8;  // PageMarks::MaxCount
    static const int SlotCount = 8;

    enum Template : uint8_t
    {
        Large,     // the clock alone in the middle
        FullArcs,  // the Sun's and the Moon's orbits around a small clock
        HalfArcs,  // their paths over the horizon, a small clock below (the Milky Way optional)
        TemplateCount
    };

    enum ClockType : uint8_t
    {
        DigitalClock,
        AnalogClock,
        NoClock,
        ClockTypeCount
    };

    enum DigitalStyle : uint8_t
    {
        MonoDigits,
        SevenSegment,
        DigitalStyleCount
    };

    enum AnalogStyle : uint8_t
    {
        ClassicHands,
        BoldHands,
        FineScale,
        OneHand,
        PlainHands,
        AnalogStyleCount
    };

    // The first four slots take a TextItem, the corners a CornerItem.
    enum Slot
    {
        TopEdge,
        TopInner,
        BottomInner,
        BottomEdge,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };
    static const int TextSlotCount = 4;

    // The slots' places on the screen (docs/superpowers/specs/2026-11-18-face-templates-design.md),
    // the same on the active and the always-on face: the text slots' centre lines, centred on
    // CentreX; the corners' centres (TopLeft, TopRight, BottomLeft, BottomRight); the centre line
    // of the Large template's clock.
    static constexpr int CentreX = 205;
    static constexpr int TextY[TextSlotCount] = { 25, 60, 442, 478 };
    static constexpr int CornerX[4] = { 66, 344, 66, 344 };
    static constexpr int CornerY[4] = { 70, 70, 432, 432 };
    static constexpr int ClockY = 251;

    enum TextItem : uint8_t
    {
        NoText,
        LongDate,
        ShortDate,
        HijriDate,
        NextSunEvent,
        NextPrayer,
        Seconds,
        BatteryPercent,
        CityTime,
        DayCounter,
        SunAltitude,
        MoonAltitude,
        TextItemCount
    };

    enum CornerItem : uint8_t
    {
        NoCorner,
        MoonIcon,
        WeekdayIcon,
        WeekdayShort,
        DayOfMonth,
        HijriDayOfMonth,
        MonthShort,
        HijriMonthShort,
        SunAltitudeCorner,
        MoonAltitudeCorner,
        HourCorner,
        MinuteCorner,
        SecondCorner,
        CornerItemCount
    };

    // Bits of Page::flags.
    static const uint8_t SecondsHandFlag = 0x01;
    static const uint8_t MilkyWayFlag = 0x02;
    static const uint8_t HiddenFlag = 0x80;

    // The number of cities and day counters a slot can name (WorldClock::ShownCities,
    // DayCounters::MaxCounters).
    static const int CityCount = 3;
    static const int CounterCount = 4;

    struct Page
    {
        uint8_t layout;   // Template
        uint8_t clock;    // ClockType
        uint8_t digital;  // DigitalStyle
        uint8_t analog;   // AnalogStyle
        uint8_t flags;
        uint8_t items[SlotCount];
        uint8_t parameters[SlotCount];  // the city or the counter of a CityTime or DayCounter slot
        uint8_t alwaysOn;               // a bit per slot: also on the always-on face
    };

    // The corner gauges (global, the same on every page; Corners::compute).
    enum GaugeItem : uint8_t
    {
        NoGauge,
        DayNightGauge,     // sunrise to sunset, or sunset to sunrise
        BatteryGauge,
        MoonPassGauge,     // the Moon above the horizon, or below it
        MoonPhaseGauge,
        NextPrayerGauge,   // from the previous prayer time to the next
        NextSunriseGauge,  // from the previous sunrise to the next
        NextSunsetGauge,
        NextMoonriseGauge,
        NextMoonsetGauge,
        WeekGauge,         // Monday 00:00 to the next
        MonthGauge,
        HijriMonthGauge,
        YearGauge,
        DayCounterGauge,   // parameter: the counter (bits 0-1) and the fill (bits 2-3, CounterFill)
        SunAltitudeGauge,  // 0 to 90 degrees, empty at night
        HourGauge,         // parameter: 0 24 hours a turn, 1 12 hours
        MinuteGauge,
        SecondGauge,       // the active face only
        GaugeItemCount
    };
    enum CounterFill : uint8_t
    {
        SinceSaved,  // from the day the counter was saved in the portal to its date
        Yearly,      // from the date's last anniversary to the next
        Last30Days,  // the 30 days before the date
        Last100Days
    };

    struct Pages
    {
        uint8_t count;
        Page pages[MaxPages];
        uint8_t gauges[4];  // by Corners::Corner
        uint8_t gaugeParameters[4];
    };

    static void defaults( Pages &pages );

    // False for no pages, too many, or any value out of its range.
    static bool valid( const Pages &pages );

    // Values out of range become the first of their list (none, digital, ...); the count 1 to
    // MaxPages.
    static void clamp( Pages &pages );

    // Pages separated by ';', each as 22 decimal numbers separated by ',': layout, clock, digital,
    // analog, flags, the eight items, the eight parameters, alwaysOn. Returns the length, -1 when
    // `size` is too small.
    static int encode( const Pages &pages, char *text, int size );

    // The inverse of encode(): false, with `pages` left as it was, for a text that is not such a
    // list; numbers out of their range are clamped.
    static bool decode( const char *text, Pages &pages );

    // The pages without the hidden flag, in order; at least the first one when all are hidden.
    static int shown( const Pages &pages, int list[MaxPages] );

    // Whether the always-on face shows `slot`: its flag set, and never the seconds.
    static bool alwaysOnSlot( const Page &page, int slot );

    // The width in pixels a text slot's text may take (0 for a corner slot): the edge slots
    // between the gauges' ends, an inner slot narrower when a corner of its row is in use.
    static int textWidth( const Page &page, int slot );

    // Where a template's clock goes: the centre line, the digits' height (seven segment; the
    // JetBrains Mono font of that size) and the analog dial's radius.
    struct ClockPlace
    {
        int centreY;
        int digitHeight;
        int analogRadius;
    };
    static ClockPlace clockPlace( int layout );
};
