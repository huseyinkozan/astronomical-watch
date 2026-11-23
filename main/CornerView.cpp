#include "CornerView.h"

#include <atomic>
#include <cmath>

#include "Panel.h"

static const uint32_t s_trackColour = 0x222222;
static const float s_chargeColour = 0xFF;  // white
// The charging part grows in this many steps to the full end, stays there one step and starts
// again: a round of 2 s.
static const int s_chargeSteps = 8;
static const uint32_t s_chargeStepMs = 200;
static const float s_notchColour = 0xD0;
// The display timeout's gauge: moved every 200 ms when it changed by about a pixel (a thousandth
// is 0.18 px), grown back after a touch by at most 400 a step, half a second from empty to full.
// 200 ms rather than 100 ms halves the corner's redraws (the owner's choice of 2026-11-23).
static const uint32_t s_timeoutStepMs = 200;
static const int s_timeoutMinMove = 5;
static const int s_timeoutGrowth = 400;
static std::atomic<int> s_timeoutLeft( 1000 );
// The cycle gauge's phases, packed so that one store changes them together: length (8 bits),
// period, fertile first, fertile last (8 each), and the colour flag in bit 31... kept in two words.
static std::atomic<uint32_t> s_cyclePhases( 0 );
static std::atomic<bool> s_cycleColour( false );
// The channel: the gauge's colour at this share of its brightness.
static const float s_channelShare = 0.3f;
static const float s_halfWidth = Corners::ActiveWidth / 2.0f;
// Pixels per step along the gauge, on the gauge's centre line (a quarter circle).
static const float s_pixelsPerAlong =
    static_cast<float>( M_PI / 2 * ( Corners::OuterRadius - Corners::ActiveWidth / 2.0 ) / ( 1000.0 * CORNER_ALONG_SCALE ) );

// Colours by Corners::Kind in active mode.
static const uint32_t s_activeColours[] = { 0x000000, 0xFFD040, 0x5078FF, 0x40D060, 0xFF8020,
                                            0xFF4030, 0xB8C4DC, 0x5C5C60, 0xB8C4DC,
                                            // The chosen gauges: the Sun's and the Moon's events,
                                            // prayers amber, calendars light grey, the Hijri month
                                            // green, counters blue, the altitude as the Sun, clocks
                                            // white.
                                            0xFFD040, 0xB8C4DC, 0xFFB040, 0xD0D0D0, 0x40C080,
                                            0x6090FF, 0xFFD040, 0xFFFFFF,
                                            // The display timeout white, red at its end,
                                            // orange before it as the battery.
                                            0xFFFFFF, 0xFF4030, 0xFF8020,
                                            // The cycle, neutral grey (its phase colours below).
                                            0x969696 };
static_assert( sizeof( s_activeColours ) / sizeof( s_activeColours[0] ) == Corners::KindCount, "a colour per kind" );

static float clamp01( float value )
{
    return value < 0 ? 0 : ( value > 1 ? 1 : value );
}

// How much of a pixel the part from `start` to `end` along the gauge covers, with round ends:
// inside the part the distance to its centre line, past an end the distance to the end's centre.
static float cover( int along, int start, int end, float radial )
{
    float past = 0;
    if ( along < start ) {
        past = ( start - along ) * s_pixelsPerAlong;
    } else if ( along > end ) {
        past = ( along - end ) * s_pixelsPerAlong;
    }
    const float distance = past <= s_halfWidth + 1 ? sqrtf( past * past + radial * radial ) : s_halfWidth + 1;
    return clamp01( s_halfWidth + 0.5f - distance );
}

static void toChannels( uint32_t hex, float share, float out[3] )
{
    out[0] = ( ( hex >> 16 ) & 0xFF ) * share;
    out[1] = ( ( hex >> 8 ) & 0xFF ) * share;
    out[2] = ( hex & 0xFF ) * share;
}

void CornerView::updateLook( int corner )
{
    const Corners::Gauge &gauge = m_gauges[corner];
    Look &look = m_looks[corner];
    // The channel takes the colour of what the gauge shows; below the horizon the Moon's gauge
    // keeps the channel of the Moon above it, so that the dark grey value stands out.
    const Corners::Kind channelKind = gauge.kind == Corners::MoonDown ? Corners::MoonUp : gauge.kind;
    if ( channelKind == Corners::Empty ) {
        toChannels( s_trackColour, 1, look.channel );
    } else {
        toChannels( s_activeColours[channelKind], s_channelShare, look.channel );
    }
    toChannels( s_activeColours[gauge.kind], 1, look.fill );
    look.empty = gauge.kind == Corners::Empty || gauge.end <= gauge.start;
    look.start = gauge.start * CORNER_ALONG_SCALE;
    look.end = gauge.end * CORNER_ALONG_SCALE;
    look.charging = gauge.charging;
}

void CornerView::invalidate( int corner )
{
    const bool left = corner == Corners::TopLeft || corner == Corners::BottomLeft;
    const bool top = corner == Corners::TopLeft || corner == Corners::TopRight;
    const int x = left ? 0 : Panel::WIDTH - CORNER_TABLE_BOX;
    const int y = top ? 0 : Panel::HEIGHT - CORNER_TABLE_BOX;
    lv_area_t area = { x, y, x + CORNER_TABLE_BOX - 1, y + CORNER_TABLE_BOX - 1 };
    lv_obj_invalidate_area( lv_layer_top(), &area );
}

void CornerView::build()
{
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        updateLook( i );
    }
    m_built = true;
    lv_timer_create( onChargeStep, s_chargeStepMs, this );
    lv_timer_create( onTimeoutStep, s_timeoutStepMs, this );
}

void CornerView::setCycle( int length, int period, int fertileFirst, int fertileLast, bool colour )
{
    const uint32_t packed = static_cast<uint32_t>( length & 0xFF ) | static_cast<uint32_t>( period & 0xFF ) << 8 |
                            static_cast<uint32_t>( fertileFirst & 0xFF ) << 16 | static_cast<uint32_t>( fertileLast & 0xFF ) << 24;
    s_cyclePhases.store( packed );
    s_cycleColour.store( colour );
}

// The colour of cycle day `day` while unlocked: bleeding red, fertile blue, otherwise grey.
static uint32_t cyclePhaseColour( int day )
{
    const uint32_t packed = s_cyclePhases.load();
    if ( day <= static_cast<int>( ( packed >> 8 ) & 0xFF ) ) {
        return 0xFF5064;
    }
    const int first = static_cast<int>( ( packed >> 16 ) & 0xFF );
    const int last = static_cast<int>( packed >> 24 );
    if ( first > 0 && day >= first && day <= last ) {
        return 0x5A96FF;
    }
    return 0x969696;
}

void CornerView::setTimeoutLeft( int left )
{
    s_timeoutLeft.store( left );
}

// Runs in the LVGL task. Only the corners that show the display timeout change.
void CornerView::onTimeoutStep( lv_timer_t *timer )
{
    CornerView *self = static_cast<CornerView *>( lv_timer_get_user_data( timer ) );
    const int left = s_timeoutLeft.load();
    // The cycle gauge's colours follow the lock and the phases.
    const uint32_t cycleKey = s_cyclePhases.load() ^ ( s_cycleColour.load() ? 0x80000000u : 0 );
    if ( cycleKey != self->m_cycleKey ) {
        self->m_cycleKey = cycleKey;
        for ( int i = 0; i < Corners::CornerCount; ++i ) {
            if ( self->m_gauges[i].kind == Corners::CycleKind ) {
                self->invalidate( i );
            }
        }
    }
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        Corners::Gauge &shown = self->m_gauges[i];
        if ( shown.kind != Corners::Timeout && shown.kind != Corners::TimeoutLow && shown.kind != Corners::TimeoutMiddle ) {
            continue;
        }
        const int wanted = left > shown.end + s_timeoutGrowth ? shown.end + s_timeoutGrowth : left;
        const Corners::Gauge gauge = Corners::timeoutGauge( wanted );
        if ( gauge.kind == shown.kind && abs( gauge.end - shown.end ) < s_timeoutMinMove ) {
            continue;
        }
        shown = gauge;
        self->updateLook( i );
        self->invalidate( i );
    }
}

// Runs in the LVGL task, which holds the display lock. Only the battery corners are drawn again,
// and only while the battery charges.
void CornerView::onChargeStep( lv_timer_t *timer )
{
    CornerView *self = static_cast<CornerView *>( lv_timer_get_user_data( timer ) );
    bool animate = false;
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        animate = animate || self->m_looks[i].charging;
    }
    const int step = animate ? ( self->m_chargeStep + 1 ) % ( s_chargeSteps + 2 ) : 0;
    if ( step == self->m_chargeStep ) {
        return;
    }
    self->m_chargeStep = step;
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        if ( self->m_looks[i].charging ) {
            self->invalidate( i );
        }
    }
}

void CornerView::show( const Corners::Gauge gauges[Corners::CornerCount] )
{
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        const Corners::Gauge &old = m_gauges[i];
        const Corners::Gauge &gauge = gauges[i];
        if ( old.start != gauge.start || old.end != gauge.end || old.kind != gauge.kind ||
             old.charging != gauge.charging ) {
            m_gauges[i] = gauge;
            updateLook( i );
            if ( m_built ) {
                invalidate( i );
            }
        }
    }
}

// The colour of one ring pixel in LVGL's RGB565, 0 to leave LVGL's pixel.
uint16_t CornerView::colour( int corner, const CornerPixel &pixel, int along ) const
{
    const Look &look = m_looks[corner];
    const float radial = pixel.radial / 16.0f;
    const float fill = look.empty ? 0 : cover( along, look.start, look.end, radial );
    float charge = 0;
    if ( look.charging && m_chargeStep > 0 ) {
        const int full = 1000 * CORNER_ALONG_SCALE;
        const int steps = m_chargeStep < s_chargeSteps ? m_chargeStep : s_chargeSteps;
        const int chargeEnd = look.end + ( full - look.end ) * steps / s_chargeSteps;
        if ( chargeEnd > look.end ) {
            charge = cover( along, look.end, chargeEnd, radial );
        }
    }
    const float channel = pixel.channel / 255.0f;
    float notch = pixel.notch / 255.0f * channel;
    float channelRgb[3] = { look.channel[0], look.channel[1], look.channel[2] };
    float fillRgb[3] = { look.fill[0], look.fill[1], look.fill[2] };
    if ( m_gauges[corner].kind == Corners::CycleKind ) {
        // By phase while unlocked; and today, the bright part's end, as a white notch.
        if ( s_cycleColour.load() ) {
            const int length = static_cast<int>( s_cyclePhases.load() & 0xFF );
            const int day = length > 0 ? along * length / ( 1000 * CORNER_ALONG_SCALE ) + 1 : 1;
            const uint32_t hex = cyclePhaseColour( day );
            toChannels( hex, s_channelShare, channelRgb );
            toChannels( hex, 1, fillRgb );
        }
        if ( !look.empty && fabsf( ( along - look.end ) * s_pixelsPerAlong ) < 1.2f ) {
            notch = channel;
        }
    }
    float rgb[3] = {};
    for ( int i = 0; i < 3; ++i ) {
        float value = channelRgb[i] * channel;
        value = s_chargeColour * charge + value * ( 1 - charge );
        value = fillRgb[i] * fill + value * ( 1 - fill );
        rgb[i] = s_notchColour * notch + value * ( 1 - notch );
    }
    const int r = static_cast<int>( rgb[0] * 31 / 255 + 0.5f );
    const int g = static_cast<int>( rgb[1] * 63 / 255 + 0.5f );
    const int b = static_cast<int>( rgb[2] * 31 / 255 + 0.5f );
    return static_cast<uint16_t>( ( r << 11 ) | ( g << 5 ) | b );
}

void CornerView::paint( const lv_area_t &area, uint16_t *pixels ) const
{
    if ( !m_built ) {
        return;
    }
    const int width = lv_area_get_width( &area );
    for ( int y = area.y1; y <= area.y2; ++y ) {
        const bool bottom = y >= Panel::HEIGHT - CORNER_TABLE_BOX;
        if ( y >= CORNER_TABLE_BOX && !bottom ) {
            continue;
        }
        const int row = bottom ? Panel::HEIGHT - 1 - y : y;
        const int first = cornerRowFirst[row];
        const int count = cornerRowCount[row];
        const CornerPixel *run = cornerPixels + cornerRowOffset[row];
        uint16_t *line = pixels + ( y - area.y1 ) * width;
        for ( int side = 0; side < 2; ++side ) {
            const bool right = side == 1;
            const int corner = bottom ? ( right ? Corners::BottomRight : Corners::BottomLeft )
                                      : ( right ? Corners::TopRight : Corners::TopLeft );
            for ( int i = 0; i < count; ++i ) {
                const int x = right ? Panel::WIDTH - 1 - ( first + i ) : first + i;
                if ( x < area.x1 || x > area.x2 ) {
                    continue;
                }
                // As in cornerTablePixel(): the bottom gauges run the other way.
                const int along = bottom ? 1000 * CORNER_ALONG_SCALE - run[i].along : run[i].along;
                const uint16_t value = colour( corner, run[i], along );
                if ( value != 0 ) {
                    line[x - area.x1] = value;
                }
            }
        }
    }
}
