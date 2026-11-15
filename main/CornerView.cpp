#include "CornerView.h"

#include <cmath>

#include "AlwaysOn.h"
#include "Panel.h"

static const uint32_t s_trackColour = 0x222222;
static const float s_chargeColour = 0xFF;  // white
// The charging part grows in this many steps to the full end, stays there one step and starts
// again: a round of 2 s.
static const int s_chargeSteps = 8;
static const uint32_t s_chargeStepMs = 200;
static const float s_notchColour = 0xD0;
// The channel: the gauge's colour at this share of its brightness.
static const float s_channelShare = 0.3f;
static const float s_halfWidth = Corners::ActiveWidth / 2.0f;
// Pixels per step along the gauge, on the gauge's centre line (a quarter circle).
static const float s_pixelsPerAlong =
    static_cast<float>( M_PI / 2 * ( Corners::OuterRadius - Corners::ActiveWidth / 2.0 ) / ( 1000.0 * CORNER_ALONG_SCALE ) );

// Colours by Corners::Kind in active mode.
static const uint32_t s_activeColours[] = { 0x000000, 0xFFD040, 0x5078FF, 0x40D060, 0xFF8020,
                                            0xFF4030, 0xB8C4DC, 0x5C5C60, 0xB8C4DC };

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
}

// Runs in the LVGL task, which holds the display lock. Only the battery corner is drawn again,
// and only while it charges in the active look.
void CornerView::onChargeStep( lv_timer_t *timer )
{
    CornerView *self = static_cast<CornerView *>( lv_timer_get_user_data( timer ) );
    const bool animate = self->m_looks[Corners::TopRight].charging && !self->m_alwaysOn;
    const int step = animate ? ( self->m_chargeStep + 1 ) % ( s_chargeSteps + 2 ) : 0;
    if ( step != self->m_chargeStep ) {
        self->m_chargeStep = step;
        self->invalidate( Corners::TopRight );
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

void CornerView::setAlwaysOnLook( bool alwaysOn )
{
    if ( alwaysOn == m_alwaysOn ) {
        return;
    }
    m_alwaysOn = alwaysOn;
    for ( int i = 0; i < Corners::CornerCount && m_built; ++i ) {
        invalidate( i );
    }
}

// The colour of one ring pixel in LVGL's RGB565, 0 to leave LVGL's pixel.
uint16_t CornerView::colour( int corner, const CornerPixel &pixel, int along, int x, int y ) const
{
    if ( m_alwaysOn ) {
        // The always-on face's own pixels (panel byte order), LVGL's buffer the other way.
        const uint16_t panel = AlwaysOn::cornerPixel( m_gauges, x, y );
        return static_cast<uint16_t>( ( panel >> 8 ) | ( panel << 8 ) );
    }
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
    const float notch = pixel.notch / 255.0f * channel;
    float rgb[3] = {};
    for ( int i = 0; i < 3; ++i ) {
        float value = look.channel[i] * channel;
        value = s_chargeColour * charge + value * ( 1 - charge );
        value = look.fill[i] * fill + value * ( 1 - fill );
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
                const uint16_t value = colour( corner, run[i], along, x, y );
                if ( value != 0 ) {
                    line[x - area.x1] = value;
                }
            }
        }
    }
}
