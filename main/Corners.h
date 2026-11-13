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
        MoonLit
    };

    // The lit part of a gauge in thousandths of its length, from its lower end.
    struct Gauge
    {
        int start;
        int end;
        Kind kind;
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

    // What the gauges take from the astronomy of a local day; kept with the always-on face, so
    // that its minute wakes compute no astronomy.
    struct Input
    {
        bool valid;
        Events sun;
        Events moon;
        int moonPercent;  // illuminated fraction
        bool waxing;
    };

    // Each gauge lies on the quarter circle that cuts the panel's corner (120 px, docs/hardware.md,
    // visible area), its outer edge 3 px inside the visible edge.
    static const int CornerRadius = 120;
    static const int OuterRadius = 117;

    // The centre of a corner's circle on a `width` x `height` screen, and the screen angles
    // (degrees, clockwise from 3 o'clock) of the gauge's lower and upper ends.
    static void geometry( Corner corner, int width, int height, int &centreX, int &centreY, int &lowerAngle,
                          int &upperAngle );

    static Input fromSky( const Sky::Day &day );

    // The gauges at `now` (Unix time). `batteryPercent` < 0: unknown.
    static void compute( const Input &input, int64_t now, int batteryPercent, Gauge gauges[CornerCount] );
};
