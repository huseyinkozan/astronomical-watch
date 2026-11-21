#pragma once

#include <cstdint>

#include "Sky.h"

// The four corner gauges (CLAUDE.md, touch input and layout): quarter circles along the screen's
// rounded corners, on every active page and on the always-on face.
// - top left: the day from sunrise to sunset, or the night from sunset to sunrise;
// - top right: the battery;
// - bottom left: the Moon above the horizon from rise to set, or below it from set to rise;
// - bottom right: the Moon's phase as a lit part that grows from the lower end to full and then
//   shrinks towards the upper end.
// What a gauge shows is computed here; CornerView (LVGL) and AlwaysOn draw it. Plain C++ like
// Sky, so the host tests can use it too.
class Corners
{
public:
    enum Corner
    {
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
        CornerCount
    };

    // Decides the colour.
    enum Kind
    {
        Empty,
        Day,
        Night,
        BatteryHigh,    // above 50 %
        BatteryMiddle,  // above 10 %
        BatteryLow,     // the power-off limit
        MoonUp,
        MoonDown,
        MoonLit,
        // The chosen gauges (FaceConfig::GaugeItem).
        SunEvent,       // to the next sunrise or sunset
        MoonEvent,      // to the next moonrise or moonset
        PrayerEvent,    // to the next prayer time
        Calendar,       // the week, the month, the year
        HijriCalendar,  // the Hijri month
        Counter,        // a day counter
        Altitude,       // the Sun's altitude
        Clock,          // the hour, the minute, the second
        KindCount
    };

    // The lit part of a gauge in thousandths of its length, from its lower end.
    struct Gauge
    {
        int start;
        int end;
        Kind kind;
        bool charging;  // the battery gauge: the part above the value is drawn as charging
    };

    // A body's rises and sets around the local day, in time order.
    static const int MaxEvents = 6;
    struct Events
    {
        int64_t times[MaxEvents];
        bool rising[MaxEvents];
        int count;
        bool alwaysUp;    // no rise or set today: above the horizon all day
        bool alwaysDown;  // below it all day
    };

    // What the gauges and the sky faces take from the astronomy of a local day; kept with the
    // always-on face, so that its minute wakes compute no astronomy.
    struct Input
    {
        bool valid;
        Events sun;
        Events civil;  // the Sun at -6 degrees: civil dawn as rising, civil dusk as setting
        Events moon;
        int moonPercent;  // illuminated fraction
        bool waxing;
        int64_t prayers[6];  // today's prayer times (Sky::Prayer)
        bool prayerValid[6];
    };

    // What the chosen gauges take beyond the sky: the moment, the clock's offset, the battery, the
    // Sun's altitude, the Hijri day of the month (0: unknown), the four day counters' dates and the
    // days they were saved (0: unknown), and whether it is the always-on face (no second gauge).
    struct Context
    {
        int64_t now;
        int utcOffsetMinutes;
        int batteryPercent;  // < 0: unknown
        bool charging;
        double sunAltitude;
        bool sunAltitudeValid;
        int hijriDay;
        int counterDays[4];
        int counterSavedDays[4];
        bool counterValid[4];
        bool alwaysOn;
    };

    // Each gauge lies on the quarter circle that cuts the panel's corner (120 px, docs/hardware.md,
    // visible area), its outer edge 3 px inside the visible edge.
    static const int CornerRadius = 120;
    static const int OuterRadius = 117;
    // The gauge's width in active mode (with a channel and round ends) and on the always-on face.
    static const int ActiveWidth = 5;
    static const int AlwaysOnWidth = 2;

    // The centre of a corner's circle on a `width` x `height` screen, and the screen angles
    // (degrees, clockwise from 3 o'clock) of the gauge's lower and upper ends.
    static void geometry( Corner corner, int width, int height, int &centreX, int &centreY, int &lowerAngle,
                          int &upperAngle );

    static Input fromSky( const Sky::Day &day );

    // A body's pass around `now`: from its rise (upStart) to its set (upEnd) and on to the next
    // rise (downEnd); `up` when `now` lies in the first part. False when the events do not
    // surround `now` (no rise or set near it).
    struct Pass
    {
        bool up;
        int64_t upStart;
        int64_t upEnd;
        int64_t downEnd;
    };
    static bool pass( const Events &events, int64_t now, Pass &result );

    // A body on the arc templates (FaceConfig::FullArcs, HalfArcs): whether its pass around `now`
    // is known, whether it is up, its angle on the screen (degrees clockwise from 3 o'clock) and,
    // on the orbits, the rise and set angles. `orbit`: around the circle, the part above the
    // horizon centred at the top and as long as its share of the pass; otherwise over the
    // horizon from left to right, right to left `south` of the equator. `alwaysUp` without a
    // rise or set (the polar day).
    struct ArcPlace
    {
        bool known;
        bool up;
        bool alwaysUp;
        float angle;
        float rise;
        float set;
    };
    static ArcPlace arcPlace( const Events &events, int64_t now, bool orbit, bool south );

    // Whether the Sun is down at `now` (the always-on face's night brightness), whatever the
    // corners show; false without the sky.
    static bool night( const Input &input, int64_t now );

    // Whether the Sun is below -6 degrees at `now` (civil twilight over): the red night mode;
    // false without the sky and in light nights.
    static bool dark( const Input &input, int64_t now );

    // The gauges of `items` (FaceConfig::GaugeItem by corner) with their `parameters`.
    static void compute( const Input &input, const Context &context, const uint8_t items[CornerCount],
                         const uint8_t parameters[CornerCount], Gauge gauges[CornerCount] );
};
