#include "PageMarks.h"

#include "Corners.h"
#include "Panel.h"

// In line with the corner gauges: their 5 px band (radius 113 to 117 around the corner circle's
// centre) continued along the straight right (or bottom) edge.
static const int s_width = 5;
static const int s_x = Panel::WIDTH - 1 - Corners::CornerRadius + Corners::OuterRadius - s_width + 1;
static const int s_y = Panel::HEIGHT - 1 - Corners::CornerRadius + Corners::OuterRadius - s_width + 1;
// Clear of the gauges' ends, which lie at the corner circles' centres.
static const int s_gapToGauge = 10;
static const int s_top = Corners::CornerRadius + s_gapToGauge;
static const int s_bottom = Panel::HEIGHT - 1 - Corners::CornerRadius - s_gapToGauge;
static const int s_left = Corners::CornerRadius + s_gapToGauge;
static const int s_right = Panel::WIDTH - 1 - Corners::CornerRadius - s_gapToGauge;
static const int s_gap = 6;  // between the marks
static const uint32_t s_current = 0xD0D0D0;
static const uint32_t s_other = 0x303030;
static const uint32_t s_flashMs = 1200;

void PageMarks::build( lv_obj_t *parent, int count, bool horizontal )
{
    m_horizontal = horizontal;
    for ( int i = 0; i < MaxCount; ++i ) {
        lv_obj_t *mark = lv_obj_create( parent );
        lv_obj_remove_style_all( mark );
        lv_obj_set_style_bg_opa( mark, LV_OPA_COVER, 0 );
        lv_obj_set_clickable( mark, false );
        m_marks[i] = mark;
    }
    setCount( count );
    show( 0 );
}

void PageMarks::setCount( int count )
{
    m_count = count > MaxCount ? MaxCount : ( count < 1 ? 1 : count );
    const int span = m_horizontal ? s_right - s_left + 1 : s_bottom - s_top + 1;
    const int length = ( span - ( m_count - 1 ) * s_gap ) / m_count;
    for ( int i = 0; i < MaxCount; ++i ) {
        lv_obj_t *mark = m_marks[i];
        if ( m_horizontal ) {
            lv_obj_set_size( mark, length, s_width );
            lv_obj_set_pos( mark, s_left + i * ( length + s_gap ), s_y );
        } else {
            lv_obj_set_size( mark, s_width, length );
            lv_obj_set_pos( mark, s_x, s_top + i * ( length + s_gap ) );
        }
        lv_obj_set_hidden( mark, !m_visible || i >= m_count );
    }
}

void PageMarks::show( int index )
{
    for ( int i = 0; i < m_count; ++i ) {
        lv_obj_set_style_bg_color( m_marks[i], lv_color_hex( i == index ? s_current : s_other ), 0 );
    }
}

void PageMarks::flash( int index )
{
    show( index );
    setVisible( true );
    if ( m_hideTimer == nullptr ) {
        m_hideTimer = lv_timer_create( onHide, s_flashMs, this );
        lv_timer_set_repeat_count( m_hideTimer, 1 );
        lv_timer_set_auto_delete( m_hideTimer, false );
    } else {
        lv_timer_set_repeat_count( m_hideTimer, 1 );
        lv_timer_reset( m_hideTimer );
        lv_timer_resume( m_hideTimer );
    }
}

void PageMarks::setVisible( bool visible )
{
    m_visible = visible;
    for ( int i = 0; i < MaxCount; ++i ) {
        lv_obj_set_hidden( m_marks[i], !visible || i >= m_count );
    }
}

void PageMarks::onHide( lv_timer_t *timer )
{
    static_cast<PageMarks *>( lv_timer_get_user_data( timer ) )->setVisible( false );
}
