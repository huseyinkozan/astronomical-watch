#include "Keypad.h"

#include "Fonts.h"
#include "Styles.h"

// The look of Styles::button and Styles::accent, drawn here.
static const uint32_t s_outline = 0x808080;
static const uint32_t s_lit = 0x606060;
static const uint32_t s_accentLit = 0x604018;
static const int s_radius = 14;

// Whether `cell` continues the key of the cell on its left (a key over two cells).
static bool continues( int pad, int cell )
{
    return cell % Calculator::Columns != 0 && Calculator::keyAt( pad, cell ) == Calculator::keyAt( pad, cell - 1 );
}

void Keypad::build( lv_obj_t *parent, int x, int y, Calculator &calculator,
                    void ( *onKey )( void *user, Calculator::Key key ), void *user )
{
    m_calculator = &calculator;
    m_onKey = onKey;
    m_user = user;
    m_pressed = -1;
    m_released = -1;
    m_object = lv_obj_create( parent );
    lv_obj_remove_style_all( m_object );
    lv_obj_set_pos( m_object, x, y );
    lv_obj_set_size( m_object, Width, Height );
    lv_obj_set_scrollable( m_object, false );
    lv_obj_set_press_lock( m_object, false );
    lv_obj_add_event_cb( m_object, onDraw, LV_EVENT_DRAW_MAIN, this );
    const lv_event_code_t codes[5] = { LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST,
                                       LV_EVENT_CLICKED };
    for ( int i = 0; i < 5; ++i ) {
        lv_obj_add_event_cb( m_object, onEvent, codes[i], this );
    }
}

void Keypad::forget()
{
    m_object = nullptr;
    m_pressed = -1;
    m_released = -1;
}

void Keypad::refresh()
{
    if ( m_object != nullptr ) {
        lv_obj_invalidate( m_object );
    }
}

int Keypad::cellAt( const lv_point_t &point ) const
{
    lv_area_t area = {};
    lv_obj_get_coords( m_object, &area );
    const int dx = point.x - area.x1;
    const int dy = point.y - area.y1;
    if ( dx < 0 || dy < 0 || dx >= Width || dy >= Height || dy % ( KeyHeight + Gap ) >= KeyHeight ) {
        return -1;
    }
    const int column = dx / ( KeyWidth + Gap );
    const int cell = dy / ( KeyHeight + Gap ) * Calculator::Columns + column;
    const int pad = m_calculator->pad();
    if ( dx % ( KeyWidth + Gap ) >= KeyWidth ) {
        // The gap: inside a key over two cells only.
        if ( column + 1 >= Calculator::Columns || !continues( pad, cell + 1 ) ) {
            return -1;
        }
    }
    return continues( pad, cell ) ? cell - 1 : cell;
}

void Keypad::setPressed( int cell )
{
    if ( cell != m_pressed ) {
        m_pressed = cell;
        lv_obj_invalidate( m_object );
    }
}

void Keypad::onEvent( lv_event_t *event )
{
    Keypad *self = static_cast<Keypad *>( lv_event_get_user_data( event ) );
    const lv_event_code_t code = lv_event_get_code( event );
    lv_indev_t *indev = lv_indev_active();
    lv_point_t point = {};
    if ( indev != nullptr ) {
        lv_indev_get_point( indev, &point );
    }
    if ( code == LV_EVENT_PRESSED ) {
        self->m_released = -1;
        self->setPressed( self->cellAt( point ) );
    } else if ( code == LV_EVENT_PRESSING ) {
        if ( self->m_pressed >= 0 && self->cellAt( point ) != self->m_pressed ) {
            self->setPressed( -1 );
        }
    } else if ( code == LV_EVENT_RELEASED ) {
        // LVGL sends CLICKED after this unless the pages scrolled.
        self->m_released = self->m_pressed;
        self->setPressed( -1 );
    } else if ( code == LV_EVENT_PRESS_LOST ) {
        self->m_released = -1;
        self->setPressed( -1 );
    } else if ( code == LV_EVENT_CLICKED ) {
        const int cell = self->m_released;
        self->m_released = -1;
        // A swipe (the parts' step) is no tap, even if it ended on the key it started on.
        const bool swiped = indev != nullptr && lv_indev_get_gesture_dir( indev ) != LV_DIR_NONE;
        if ( cell >= 0 && !swiped && self->m_onKey != nullptr ) {
            self->m_onKey( self->m_user, Calculator::keyAt( self->m_calculator->pad(), cell ) );
        }
    }
}

void Keypad::onDraw( lv_event_t *event )
{
    Keypad *self = static_cast<Keypad *>( lv_event_get_user_data( event ) );
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t area = {};
    lv_obj_get_coords( self->m_object, &area );
    const Calculator &calculator = *self->m_calculator;
    const int pad = calculator.pad();
    lv_draw_rect_dsc_t box;
    lv_draw_rect_dsc_init( &box );
    box.radius = s_radius;
    box.border_width = 2;
    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init( &label );
    label.font = &jetbrains_mono_26;
    label.align = LV_TEXT_ALIGN_CENTER;
    for ( int cell = 0; cell < Calculator::Cells; ++cell ) {
        if ( continues( pad, cell ) ) {
            continue;
        }
        const Calculator::Key key = Calculator::keyAt( pad, cell );
        const int span = cell % Calculator::Columns + 1 < Calculator::Columns && continues( pad, cell + 1 ) ? 2 : 1;
        const int x = area.x1 + cell % Calculator::Columns * ( KeyWidth + Gap );
        const int y = area.y1 + cell / Calculator::Columns * ( KeyHeight + Gap );
        const lv_area_t keyArea = { x, y, x + span * KeyWidth + ( span - 1 ) * Gap - 1, y + KeyHeight - 1 };
        const bool accent = calculator.accented( key );
        const bool lit = cell == self->m_pressed;
        box.bg_color = lv_color_hex( lit ? ( accent ? s_accentLit : s_lit ) : 0x000000 );
        box.border_color = lv_color_hex( accent ? Styles::Amber : s_outline );
        lv_draw_rect( layer, &box, &keyArea );
        // The 26 px font's line (32 px) in the middle of the key.
        const int top = y + ( KeyHeight - jetbrains_mono_26.line_height ) / 2;
        const lv_area_t textArea = { keyArea.x1, top, keyArea.x2, top + jetbrains_mono_26.line_height - 1 };
        label.color = accent ? lv_color_hex( Styles::Amber ) : lv_color_white();
        label.text = calculator.label( key );
        lv_draw_label( layer, &label, &textArea );
    }
}
