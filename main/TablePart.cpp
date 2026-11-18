#include "TablePart.h"
#include "Fonts.h"

void TablePart::build( lv_obj_t *parent, const Layout &layout )
{
    m_layout = layout;
    if ( m_layout.rowCount > TableText::MaxRows ) {
        m_layout.rowCount = TableText::MaxRows;
    }
    m_object = lv_obj_create( parent );
    if ( m_object == nullptr ) {
        return;
    }
    lv_obj_remove_style_all( m_object );
    lv_obj_set_size( m_object, LV_PCT( 100 ), LV_PCT( 100 ) );
    // Touches belong to the part (its swipes).
    lv_obj_set_clickable( m_object, false );
    lv_obj_set_scrollable( m_object, false );
    lv_obj_add_event_cb( m_object, onDraw, LV_EVENT_DRAW_MAIN, this );
}

TableText &TablePart::text()
{
    return m_text;
}

void TablePart::refresh()
{
    if ( m_text.takeChanged() && m_object != nullptr ) {
        lv_obj_invalidate( m_object );
    }
}

// One cell: a single line, clipped to its box (a longer text does not run into its neighbour).
void TablePart::drawCell( lv_layer_t *layer, const lv_area_t &coords, int x, int y, int width, const char *text,
                          uint32_t colour, lv_text_align_t align ) const
{
    if ( text == nullptr || text[0] == '\0' ) {
        return;
    }
    const int lineHeight = jetbrains_mono_26.line_height;
    const lv_area_t box = { coords.x1 + x, coords.y1 + y, coords.x1 + x + width - 1, coords.y1 + y + lineHeight - 1 };
    const lv_area_t clip = layer->_clip_area;
    const lv_area_t cellClip = { LV_MAX( clip.x1, box.x1 ), LV_MAX( clip.y1, box.y1 ), LV_MIN( clip.x2, box.x2 ),
                                 LV_MIN( clip.y2, box.y2 ) };
    if ( cellClip.x1 > cellClip.x2 || cellClip.y1 > cellClip.y2 ) {
        return;
    }
    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init( &label );
    label.font = &jetbrains_mono_26;
    label.color = lv_color_hex( colour );
    label.align = align;
    label.text = text;
    // LVGL takes the clip area when it records the draw task.
    layer->_clip_area = cellClip;
    lv_draw_label( layer, &label, &box );
    layer->_clip_area = clip;
}

void TablePart::onDraw( lv_event_t *event )
{
    TablePart *self = static_cast<TablePart *>( lv_event_get_user_data( event ) );
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t coords = {};
    lv_obj_get_coords( self->m_object, &coords );
    const Layout &layout = self->m_layout;
    const TableText &text = self->m_text;
    const int lineHeight = jetbrains_mono_26.line_height;
    int top = layout.top;
    if ( top < 0 ) {
        // As a centred flex column of the title and the rows.
        const int height = layout.titleGap + ( layout.rowCount - 1 ) * layout.pitch + lineHeight;
        top = ( lv_area_get_height( &coords ) - height ) / 2;
    }
    self->drawCell( layer, coords, 0, top, lv_area_get_width( &coords ), layout.title, TableText::Grey,
                    LV_TEXT_ALIGN_CENTER );
    for ( int row = 0; row < layout.rowCount; ++row ) {
        const int y = top + layout.titleGap + row * layout.pitch;
        const uint32_t valueColour = text.valueColour( row );
        if ( layout.kinds[row] == Line ) {
            self->drawCell( layer, coords, layout.nameX, y, layout.valueX + 2 * layout.valueWidth - layout.nameX,
                            text.text( row, 0 ), valueColour, LV_TEXT_ALIGN_CENTER );
            continue;
        }
        self->drawCell( layer, coords, layout.nameX, y, layout.nameWidth, text.text( row, 0 ), text.nameColour( row ),
                        LV_TEXT_ALIGN_LEFT );
        if ( layout.kinds[row] == TwoValues ) {
            self->drawCell( layer, coords, layout.valueX, y, layout.valueWidth, text.text( row, 1 ), valueColour,
                            LV_TEXT_ALIGN_RIGHT );
            self->drawCell( layer, coords, layout.valueX + layout.valueWidth, y, layout.valueWidth, text.text( row, 2 ),
                            valueColour, LV_TEXT_ALIGN_RIGHT );
            continue;
        }
        const char *value = text.text( row, 1 );
        if ( layout.dotsLetters > 0 ) {
            TableText::fitWithDots( value, layout.dotsLetters, self->m_dots[row], sizeof( self->m_dots[row] ) );
            value = self->m_dots[row];
        }
        self->drawCell( layer, coords, layout.valueX, y, 2 * layout.valueWidth, value, valueColour, LV_TEXT_ALIGN_RIGHT );
    }
}
