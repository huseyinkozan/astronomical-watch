#include "CornerView.h"

#include <cmath>

#include "Panel.h"

static const int s_activeWidth = 5;
static const int s_alwaysOnWidth = 2;
static const uint32_t s_trackColour = 0x222222;
static const uint32_t s_notchColour = 0xD0D0D0;
// The channel: the gauge's colour at this share of its brightness.
static const int s_channelPercent = 30;

// Colours by Corners::Kind: active mode, and the always-on face's idle-mode colours (each channel
// on or off), where orange becomes yellow and the Moon's dark grey white.
static const uint32_t s_activeColours[] = { 0x000000, 0xFFD040, 0x5078FF, 0x40D060, 0xFF8020,
                                            0xFF4030, 0xB8C4DC, 0x5C5C60, 0xB8C4DC };
static const uint32_t s_alwaysOnColours[] = { 0x000000, 0xFFFF00, 0x0000FF, 0x00FF00, 0xFFFF00,
                                              0xFF0000, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF };

void CornerView::build()
{
    lv_obj_t *layer = lv_layer_top();
    lv_obj_set_scrollable( layer, false );
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        int centreX = 0;
        int centreY = 0;
        int lower = 0;
        int upper = 0;
        Corners::geometry( static_cast<Corners::Corner>( i ), Panel::WIDTH, Panel::HEIGHT, centreX, centreY, lower,
                           upper );
        lv_obj_t *arc = lv_arc_create( layer );
        lv_obj_remove_style_all( arc );
        lv_obj_set_clickable( arc, false );
        lv_obj_set_size( arc, 2 * Corners::OuterRadius, 2 * Corners::OuterRadius );
        lv_obj_set_pos( arc, centreX - Corners::OuterRadius, centreY - Corners::OuterRadius );
        lv_arc_set_bg_angles( arc, lower < upper ? lower : upper, lower < upper ? upper : lower );
        m_arcs[i] = arc;
        apply( i );
    }
    // Over the arcs.
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        for ( int k = 0; k < 3; ++k ) {
            lv_obj_t *notch = lv_line_create( layer );
            lv_obj_set_clickable( notch, false );
            m_notches[i][k] = notch;
        }
        placeNotches( i );
    }
}

void CornerView::placeNotches( int corner )
{
    int centreX = 0;
    int centreY = 0;
    int lower = 0;
    int upper = 0;
    Corners::geometry( static_cast<Corners::Corner>( corner ), Panel::WIDTH, Panel::HEIGHT, centreX, centreY, lower,
                       upper );
    const int width = m_alwaysOn ? s_alwaysOnWidth : s_activeWidth;
    for ( int k = 0; k < 3; ++k ) {
        const double angle = ( lower + ( upper - lower ) * k / 2.0 ) * M_PI / 180.0;
        // Across the gauge, from its outer edge inwards; a line's width is centred on its points.
        const double outer = Corners::OuterRadius - 0.5;
        const double inner = Corners::OuterRadius - width + 0.5;
        lv_point_precise_t *points = m_notchPoints[corner][k];
        points[0] = { static_cast<lv_value_precise_t>( std::lround( centreX + outer * std::cos( angle ) ) ),
                      static_cast<lv_value_precise_t>( std::lround( centreY + outer * std::sin( angle ) ) ) };
        points[1] = { static_cast<lv_value_precise_t>( std::lround( centreX + inner * std::cos( angle ) ) ),
                      static_cast<lv_value_precise_t>( std::lround( centreY + inner * std::sin( angle ) ) ) };
        lv_obj_t *notch = m_notches[corner][k];
        lv_line_set_points( notch, points, 2 );
        lv_obj_set_style_line_width( notch, 2, 0 );
        lv_obj_set_style_line_color( notch, lv_color_hex( m_alwaysOn ? 0xFFFFFF : s_notchColour ), 0 );
        // Active mode rounds the gauge's ends, which then mark the limits themselves; the
        // always-on look (no channel) keeps the end notches.
        lv_obj_set_hidden( notch, !m_alwaysOn && k != 1 );
    }
}

void CornerView::show( const Corners::Gauge gauges[Corners::CornerCount] )
{
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        const Corners::Gauge &old = m_gauges[i];
        const Corners::Gauge &gauge = gauges[i];
        if ( old.start != gauge.start || old.end != gauge.end || old.kind != gauge.kind ) {
            m_gauges[i] = gauge;
            apply( i );
        }
    }
}

void CornerView::setAlwaysOnLook( bool alwaysOn )
{
    if ( alwaysOn == m_alwaysOn ) {
        return;
    }
    m_alwaysOn = alwaysOn;
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        apply( i );
        placeNotches( i );
    }
}

void CornerView::apply( int corner )
{
    lv_obj_t *arc = m_arcs[corner];
    if ( arc == nullptr ) {
        return;
    }
    int centreX = 0;
    int centreY = 0;
    int lower = 0;
    int upper = 0;
    Corners::geometry( static_cast<Corners::Corner>( corner ), Panel::WIDTH, Panel::HEIGHT, centreX, centreY, lower,
                       upper );
    const Corners::Gauge &gauge = m_gauges[corner];
    const int width = m_alwaysOn ? s_alwaysOnWidth : s_activeWidth;
    lv_obj_set_style_arc_width( arc, width, LV_PART_MAIN );
    lv_obj_set_style_arc_width( arc, width, LV_PART_INDICATOR );
    lv_obj_set_style_arc_rounded( arc, !m_alwaysOn, LV_PART_MAIN );
    lv_obj_set_style_arc_rounded( arc, !m_alwaysOn, LV_PART_INDICATOR );
    const uint32_t colour = m_alwaysOn ? s_alwaysOnColours[gauge.kind] : s_activeColours[gauge.kind];
    // The channel takes the colour of what the gauge shows; below the horizon the Moon's gauge
    // keeps the channel of the Moon above it, so that the dark grey value stands out.
    const Corners::Kind channelKind = gauge.kind == Corners::MoonDown ? Corners::MoonUp : gauge.kind;
    lv_color_t channel = lv_color_hex( s_trackColour );
    if ( channelKind != Corners::Empty ) {
        channel = lv_color_mix( lv_color_hex( s_activeColours[channelKind] ), lv_color_black(),
                                static_cast<uint8_t>( 255 * s_channelPercent / 100 ) );
    }
    lv_obj_set_style_arc_color( arc, channel, LV_PART_MAIN );
    lv_obj_set_style_arc_opa( arc, m_alwaysOn ? LV_OPA_TRANSP : LV_OPA_COVER, LV_PART_MAIN );

    lv_obj_set_style_arc_color( arc, lv_color_hex( colour ), LV_PART_INDICATOR );
    // The always-on face dots the Moon's time below the horizon; half opacity stands for it here.
    const bool dotted = m_alwaysOn && gauge.kind == Corners::MoonDown;
    const bool empty = gauge.kind == Corners::Empty || gauge.end <= gauge.start;
    lv_obj_set_style_arc_opa( arc, empty ? LV_OPA_TRANSP : ( dotted ? LV_OPA_50 : LV_OPA_COVER ), LV_PART_INDICATOR );
    if ( !empty ) {
        const int a = lower + ( upper - lower ) * gauge.start / 1000;
        const int b = lower + ( upper - lower ) * gauge.end / 1000;
        lv_arc_set_angles( arc, a < b ? a : b, a < b ? b : a );
    }
}
