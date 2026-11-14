#include "StackPage.h"

#include "Panel.h"

static const uint32_t s_slideMs = 250;

static void setTranslateY( void *object, int32_t y )
{
    lv_obj_set_style_translate_y( static_cast<lv_obj_t *>( object ), y, 0 );
}

static void hideAfterSlide( lv_anim_t *animation )
{
    lv_obj_t *part = static_cast<lv_obj_t *>( lv_anim_get_user_data( animation ) );
    lv_obj_set_hidden( part, true );
    lv_obj_set_style_translate_y( part, 0, 0 );
}

// Moves `part` vertically from `from` to `to` pixels; `hide` hides it at the end.
static void slide( lv_obj_t *part, int from, int to, bool hide )
{
    lv_anim_t animation;
    lv_anim_init( &animation );
    lv_anim_set_var( &animation, part );
    lv_anim_set_user_data( &animation, part );
    lv_anim_set_exec_cb( &animation, setTranslateY );
    lv_anim_set_values( &animation, from, to );
    lv_anim_set_duration( &animation, s_slideMs );
    lv_anim_set_path_cb( &animation, lv_anim_path_ease_out );
    if ( hide ) {
        lv_anim_set_completed_cb( &animation, hideAfterSlide );
    }
    lv_anim_start( &animation );
}

void StackPage::build( lv_obj_t *tile, int count )
{
    m_count = count > MaxParts ? MaxParts : count;
    lv_obj_set_scrollable( tile, false );
    // LVGL sends a gesture to the first ancestor that does not pass it on, so the tile keeps it.
    lv_obj_set_gesture_bubble( tile, false );
    lv_obj_add_event_cb( tile, onGesture, LV_EVENT_GESTURE, this );
    for ( int i = 0; i < m_count; ++i ) {
        lv_obj_t *part = lv_obj_create( tile );
        lv_obj_remove_style_all( part );
        lv_obj_set_size( part, Panel::WIDTH, Panel::HEIGHT );
        lv_obj_set_scrollable( part, false );
        // Not clickable: presses and swipes go to the tile (the rows in it still take taps).
        lv_obj_set_clickable( part, false );
        m_parts[i] = part;
    }
    // A single part needs no marks.
    if ( m_count > 1 ) {
        m_marks.build( tile, m_count );
    }
    show( 0, 0 );
}

lv_obj_t *StackPage::part( int index ) const
{
    return m_parts[index];
}

int StackPage::current() const
{
    return m_current;
}

void StackPage::show( int index, int direction )
{
    const int previous = m_current;
    m_current = index;
    for ( int i = 0; i < m_count; ++i ) {
        // A slide still running ends here, so that a quick second swipe starts from rest.
        lv_anim_delete( m_parts[i], setTranslateY );
        lv_obj_set_style_translate_y( m_parts[i], 0, 0 );
        lv_obj_set_hidden( m_parts[i], i != index );
    }
    if ( m_count > 1 ) {
        m_marks.show( index );
    }
    if ( direction != 0 && previous != index ) {
        lv_obj_set_hidden( m_parts[previous], false );
        slide( m_parts[previous], 0, -direction * Panel::HEIGHT, true );
        slide( m_parts[index], direction * Panel::HEIGHT, 0, false );
    }
}

void StackPage::onGesture( lv_event_t *event )
{
    StackPage *self = static_cast<StackPage *>( lv_event_get_user_data( event ) );
    const lv_dir_t direction = lv_indev_get_gesture_dir( lv_indev_active() );
    if ( self->m_count < 2 || ( direction != LV_DIR_TOP && direction != LV_DIR_BOTTOM ) ) {
        return;
    }
    const int step = direction == LV_DIR_TOP ? 1 : self->m_count - 1;
    self->show( ( self->m_current + step ) % self->m_count, direction == LV_DIR_TOP ? 1 : -1 );
}
