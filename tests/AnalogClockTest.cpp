// Host test of main/AnalogClock.cpp: the marks and hands of the analog styles. Build and run:
// sh tests/run_host_tests.sh

#include <cmath>
#include <cstdio>

#include "AnalogClock.h"
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

static int countRole( const AnalogClock::Segment *segments, int count, AnalogClock::Role role )
{
    int found = 0;
    for ( int i = 0; i < count; ++i ) {
        found += segments[i].role == role ? 1 : 0;
    }
    return found;
}

static const AnalogClock::Segment *findRole( const AnalogClock::Segment *segments, int count, AnalogClock::Role role )
{
    for ( int i = 0; i < count; ++i ) {
        if ( segments[i].role == role ) {
            return &segments[i];
        }
    }
    return nullptr;
}

// The hand's far end is at (x, y) within half a pixel.
static bool endsAt( const AnalogClock::Segment *hand, float x, float y )
{
    return hand != nullptr && std::fabs( hand->x1 - x ) < 0.5f && std::fabs( hand->y1 - y ) < 0.5f;
}

int main()
{
    const float r = 140;
    AnalogClock::Segment segments[AnalogClock::MaxSegments];
    const int marks[FaceConfig::AnalogStyleCount] = { 12, 12, 60, 60, 0 };
    for ( int style = 0; style < FaceConfig::AnalogStyleCount; ++style ) {
        const int count = AnalogClock::segments( style, r, 10, 10, 0, false, segments, AnalogClock::MaxSegments );
        char name[48];
        snprintf( name, sizeof( name ), "style %d: %d marks", style, marks[style] );
        check( name, countRole( segments, count, AnalogClock::MajorMark ) +
                             countRole( segments, count, AnalogClock::MinorMark ) ==
                         marks[style] );
    }
    int count = AnalogClock::segments( FaceConfig::FineScale, r, 10, 10, 0, false, segments, AnalogClock::MaxSegments );
    check( "fine scale: 12 major marks", countRole( segments, count, AnalogClock::MajorMark ) == 12 );

    // Angles: clockwise from 12 o'clock, y downwards.
    count = AnalogClock::segments( FaceConfig::ClassicHands, r, 3, 0, 0, false, segments, AnalogClock::MaxSegments );
    check( "3:00 hour hand right", endsAt( findRole( segments, count, AnalogClock::HourHand ), 0.55f * r, 0 ) );
    check( "3:00 minute hand up", endsAt( findRole( segments, count, AnalogClock::MinuteHand ), 0, -0.85f * r ) );
    count = AnalogClock::segments( FaceConfig::ClassicHands, r, 18, 0, 0, false, segments, AnalogClock::MaxSegments );
    check( "18:00 hour hand down", endsAt( findRole( segments, count, AnalogClock::HourHand ), 0, 0.55f * r ) );
    count = AnalogClock::segments( FaceConfig::ClassicHands, r, 21, 45, 0, false, segments, AnalogClock::MaxSegments );
    check( "21:45 minute hand left", endsAt( findRole( segments, count, AnalogClock::MinuteHand ), -0.85f * r, 0 ) );
    count = AnalogClock::segments( FaceConfig::ClassicHands, r, 0, 0, 0, false, segments, AnalogClock::MaxSegments );
    check( "0:00 hour hand up", endsAt( findRole( segments, count, AnalogClock::HourHand ), 0, -0.55f * r ) );
    check( "hands start at the centre", findRole( segments, count, AnalogClock::HourHand )->x0 == 0 &&
                                            findRole( segments, count, AnalogClock::HourHand )->y0 == 0 );

    // One hand: 19:19 is 7 h 19 min, 219.5 degrees from 12 o'clock, read on the fine scale.
    count = AnalogClock::segments( FaceConfig::OneHand, r, 19, 19, 0, false, segments, AnalogClock::MaxSegments );
    const float a = 219.5f * static_cast<float>( M_PI ) / 180.0f;
    check( "one hand at 19:19", endsAt( findRole( segments, count, AnalogClock::OneHand ), 0.92f * r * sinf( a ),
                                        -0.92f * r * cosf( a ) ) );
    check( "one hand: no hour or minute hand", countRole( segments, count, AnalogClock::HourHand ) == 0 &&
                                                   countRole( segments, count, AnalogClock::MinuteHand ) == 0 );

    // The seconds hand only when asked; at 15 s it points right and has a tail.
    count = AnalogClock::segments( FaceConfig::ClassicHands, r, 10, 10, 15, false, segments, AnalogClock::MaxSegments );
    check( "no seconds hand unless asked", countRole( segments, count, AnalogClock::SecondHand ) == 0 );
    count = AnalogClock::segments( FaceConfig::ClassicHands, r, 10, 10, 15, true, segments, AnalogClock::MaxSegments );
    const AnalogClock::Segment *second = findRole( segments, count, AnalogClock::SecondHand );
    check( "seconds hand at 15 s", endsAt( second, 0.9f * r, 0 ) && second->x0 < 0 );
    check( "the minute hand moves with the seconds",
           endsAt( findRole( segments, count, AnalogClock::MinuteHand ),
                   0.85f * r * sinf( 10.25f * 6 * static_cast<float>( M_PI ) / 180.0f ),
                   -0.85f * r * cosf( 10.25f * 6 * static_cast<float>( M_PI ) / 180.0f ) ) );
    count = AnalogClock::segments( FaceConfig::OneHand, r, 10, 10, 15, true, segments, AnalogClock::MaxSegments );
    check( "fine scale with a seconds hand fits", count <= AnalogClock::MaxSegments &&
                                                      countRole( segments, count, AnalogClock::SecondHand ) == 1 );

    check( "capacity respected",
           AnalogClock::segments( FaceConfig::FineScale, r, 10, 10, 0, false, segments, 10 ) == 10 );
    check( "unknown style: nothing", AnalogClock::segments( 9, r, 10, 10, 0, false, segments, AnalogClock::MaxSegments ) == 0 );
    count = AnalogClock::segments( FaceConfig::BoldHands, r, 10, 10, 0, false, segments, AnalogClock::MaxSegments );
    check( "bold hands are wider", findRole( segments, count, AnalogClock::HourHand )->width == 12 &&
                                       findRole( segments, count, AnalogClock::MinuteHand )->width == 8 );

    printf( "Analog clock: %d checks, %d failures\n", s_checks, s_failures );
    return s_failures == 0 ? 0 : 1;
}
