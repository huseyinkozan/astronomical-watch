#include "Corners.h"

#include <cmath>

static const int64_t s_dayS = 86400;

void Corners::geometry( Corner corner, int width, int height, int &centreX, int &centreY, int &lowerAngle,
                        int &upperAngle )
{
    const bool left = corner == TopLeft || corner == BottomLeft;
    const bool top = corner == TopLeft || corner == TopRight;
    centreX = left ? CornerRadius : width - 1 - CornerRadius;
    centreY = top ? CornerRadius : height - 1 - CornerRadius;
    // The lower end lies on the side edge for the top corners and on the bottom edge for the
    // bottom ones.
    static const int lower[CornerCount] = { 180, 360, 90, 90 };
    static const int upper[CornerCount] = { 270, 270, 180, 0 };
    lowerAngle = lower[corner];
    upperAngle = upper[corner];
}

static void addEvent( Corners::Events &events, const Astronomy::Event &event, bool rising, int64_t shift = 0 )
{
    if ( !event.occurs || events.count >= Corners::MaxEvents ) {
        return;
    }
    // Insertion in time order.
    const int64_t time = event.time + shift;
    int i = events.count;
    while ( i > 0 && events.times[i - 1] > time ) {
        events.times[i] = events.times[i - 1];
        events.rising[i] = events.rising[i - 1];
        --i;
    }
    events.times[i] = time;
    events.rising[i] = rising;
    ++events.count;
}

static void addCrossings( Corners::Events &events, const Astronomy::Crossings &crossings )
{
    addEvent( events, crossings.rising, true );
    addEvent( events, crossings.setting, false );
}

Corners::Input Corners::fromSky( const Sky::Day &day )
{
    Input input = {};
    input.valid = true;
    const Astronomy::Crossings &sun = day.sun.riseSet;
    addEvent( input.sun, day.sunsetBefore, false );
    addCrossings( input.sun, sun );
    // Tomorrow's sunrise as today's a day later: within a few minutes outside the polar regions,
    // under 2 pixels on the gauge.
    addEvent( input.sun, sun.rising, true, s_dayS );
    input.sun.alwaysUp = sun.alwaysAbove;
    input.sun.alwaysDown = sun.alwaysBelow;
    addCrossings( input.moon, day.moonBefore );
    addCrossings( input.moon, day.moon );
    addCrossings( input.moon, day.moonAfter );
    input.moonPercent = static_cast<int>( day.phase.illumination * 100 + 0.5 );
    input.waxing = day.phase.elongation < 180;
    return input;
}

// Whether the body is up at `now` and how much of the time between the events around it has
// passed, in thousandths; false when `now` is not between two events.
static bool progress( const Corners::Events &events, int64_t now, bool &up, int &passed )
{
    for ( int i = 0; i + 1 < events.count; ++i ) {
        if ( events.times[i] <= now && now < events.times[i + 1] ) {
            up = events.rising[i];
            passed = static_cast<int>( ( now - events.times[i] ) * 1000 / ( events.times[i + 1] - events.times[i] ) );
            return true;
        }
    }
    return false;
}

void Corners::compute( const Input &input, int64_t now, int batteryPercent, bool charging,
                       Gauge gauges[CornerCount] )
{
    for ( int i = 0; i < CornerCount; ++i ) {
        gauges[i] = { 0, 0, Empty, false };
    }

    if ( batteryPercent >= 0 ) {
        const int percent = batteryPercent > 100 ? 100 : batteryPercent;
        const Kind kind = percent > 50 ? BatteryHigh : ( percent > 10 ? BatteryMiddle : BatteryLow );
        gauges[TopRight] = { 0, percent * 10, kind, charging };
    }
    if ( !input.valid ) {
        return;
    }

    bool up = false;
    int passed = 0;
    if ( progress( input.sun, now, up, passed ) ) {
        gauges[TopLeft] = { 0, passed, up ? Day : Night, false };
    } else if ( input.sun.alwaysUp ) {
        gauges[TopLeft] = { 0, 1000, Day, false };
    } else if ( input.sun.alwaysDown ) {
        gauges[TopLeft] = { 0, 1000, Night, false };
    }

    if ( progress( input.moon, now, up, passed ) ) {
        gauges[BottomLeft] = { 0, passed, up ? MoonUp : MoonDown, false };
    }

    const int lit = input.moonPercent * 10;
    if ( input.waxing ) {
        gauges[BottomRight] = { 0, lit, MoonLit, false };
    } else {
        gauges[BottomRight] = { 1000 - lit, 1000, MoonLit, false };
    }
}

Corners::ArcPlace Corners::arcPlace( const Events &events, int64_t now, bool orbit, bool south )
{
    ArcPlace place = {};
    place.alwaysUp = events.alwaysUp;
    Pass p = {};
    place.known = Corners::pass( events, now, p ) && p.upEnd > p.upStart && p.downEnd > p.upEnd;
    if ( !place.known ) {
        return place;
    }
    place.up = p.up;
    const float upPart = static_cast<float>( now - p.upStart ) / static_cast<float>( p.upEnd - p.upStart );
    if ( orbit ) {
        const float span = 360.0f * static_cast<float>( p.upEnd - p.upStart ) / static_cast<float>( p.downEnd - p.upStart );
        place.rise = 270 - span / 2;
        place.set = 270 + span / 2;
        place.angle = place.up ? place.rise + span * upPart
                               : place.set + ( 360 - span ) * static_cast<float>( now - p.upEnd ) /
                                                 static_cast<float>( p.downEnd - p.upEnd );
        place.angle = std::fmod( place.angle, 360.0f );
    } else {
        place.angle = south ? 360 - 180 * upPart : 180 + 180 * upPart;
    }
    return place;
}

bool Corners::pass( const Events &events, int64_t now, Pass &result )
{
    for ( int i = 0; i + 1 < events.count; ++i ) {
        if ( !( events.times[i] <= now && now < events.times[i + 1] ) ) {
            continue;
        }
        result.up = events.rising[i];
        if ( result.up ) {
            // Rise, set, then the next rise.
            if ( i + 2 >= events.count ) {
                return false;
            }
            result.upStart = events.times[i];
            result.upEnd = events.times[i + 1];
            result.downEnd = events.times[i + 2];
        } else {
            // The rise before, this set, the next rise.
            if ( i < 1 ) {
                return false;
            }
            result.upStart = events.times[i - 1];
            result.upEnd = events.times[i];
            result.downEnd = events.times[i + 1];
        }
        return result.upStart < result.upEnd && result.upEnd < result.downEnd;
    }
    return false;
}
