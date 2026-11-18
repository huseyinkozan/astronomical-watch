#pragma once

// The marks and hands of the analog clock styles (FaceConfig::AnalogStyle;
// docs/superpowers/specs/2026-11-18-face-templates-design.md and its mock-up's geometry) as line
// segments, for the active face (LVGL lines) and the always-on face (its line shapes). Plain C++
// for the host tests.
class AnalogClock
{
public:
    enum Role
    {
        MajorMark,
        MinorMark,
        HourHand,
        MinuteHand,
        SecondHand,
        OneHand  // the hour hand of the One hand style, read on the fine scale
    };

    // From (x0, y0) to (x1, y1), the clock's centre at (0, 0), y downwards; `width` in pixels.
    struct Segment
    {
        float x0;
        float y0;
        float x1;
        float y1;
        float width;
        Role role;
    };

    // Sixty marks, three hands.
    static const int MaxSegments = 64;

    // The segments of `style` with `radius` at the time given, marks first; the seconds hand only
    // when `secondsHand` (the active face). Returns how many, at most `capacity`; 0 for an
    // unknown style.
    static int segments( int style, float radius, int hour, int minute, int second, bool secondsHand, Segment *out,
                         int capacity );
};
