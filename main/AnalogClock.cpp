#include "AnalogClock.h"

#include <cmath>

#include "FaceConfig.h"

// Per style: hour and minute hand widths (the One hand style has one hand), as in the mock-up.
static const float s_handWidths[FaceConfig::AnalogStyleCount][2] = { { 6, 4 }, { 12, 8 }, { 5, 3 }, { 6, 0 }, { 7, 4 } };
static const float s_hourLength = 0.55f;
static const float s_minuteLength = 0.85f;
static const float s_oneHandLength = 0.92f;
static const float s_secondLength = 0.9f;
static const float s_secondTail = 0.15f;

namespace
{
struct List
{
    AnalogClock::Segment *out;
    int capacity;
    int count;
};
}

static void add( List &list, float x0, float y0, float x1, float y1, float width, AnalogClock::Role role )
{
    if ( list.count < list.capacity ) {
        list.out[list.count++] = { x0, y0, x1, y1, width, role };
    }
}

// A segment along the direction `turns` of a full circle clockwise from 12 o'clock, from
// `inner` to `outer` times the radius (inner below 0: a tail behind the centre).
static void radial( List &list, float radius, float turns, float inner, float outer, float width, AnalogClock::Role role )
{
    const float a = turns * 2 * static_cast<float>( M_PI );
    const float x = sinf( a );
    const float y = -cosf( a );
    const float x0 = inner == 0 ? 0 : inner * radius * x;
    const float y0 = inner == 0 ? 0 : inner * radius * y;
    add( list, x0, y0, outer * radius * x, outer * radius * y, width, role );
}

int AnalogClock::segments( int style, float radius, int hour, int minute, int second, bool secondsHand, Segment *out,
                           int capacity )
{
    if ( style < 0 || style >= FaceConfig::AnalogStyleCount ) {
        return 0;
    }
    List list = { out, capacity, 0 };
    const bool fine = style == FaceConfig::FineScale || style == FaceConfig::OneHand;
    const int marks = style == FaceConfig::PlainHands ? 0 : ( fine ? 60 : 12 );
    for ( int i = 0; i < marks; ++i ) {
        const bool major = !fine || i % 5 == 0;
        radial( list, radius, static_cast<float>( i ) / marks, major ? 0.86f : 0.93f, 1, major ? 3 : 1,
                major ? MajorMark : MinorMark );
    }
    const float minutes = minute + second / 60.0f;
    const float hours = ( hour % 12 ) + minute / 60.0f;
    if ( style == FaceConfig::OneHand ) {
        radial( list, radius, hours / 12, 0, s_oneHandLength, s_handWidths[style][0], OneHand );
    } else {
        radial( list, radius, hours / 12, 0, s_hourLength, s_handWidths[style][0], HourHand );
        radial( list, radius, minutes / 60, 0, s_minuteLength, s_handWidths[style][1], MinuteHand );
    }
    if ( secondsHand ) {
        // Through the centre, as on the old dial.
        const float turns = second / 60.0f;
        const float a = turns * 2 * static_cast<float>( M_PI );
        add( list, -s_secondTail * radius * sinf( a ), s_secondTail * radius * cosf( a ), s_secondLength * radius * sinf( a ),
             -s_secondLength * radius * cosf( a ), 2, SecondHand );
    }
    return list.count;
}
