#include "TablePart.h"
#include "Fonts.h"
// lv_text_get_size_attributes: a row as high as its wrapped texts, as the labels were.
#include "lvgl_private.h"

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

void TablePart::forget()
{
    m_object = nullptr;
}

// One cell, clipped to its box (a longer text does not run into its neighbour).
void TablePart::drawCell( lv_layer_t *layer, const lv_area_t &coords, int x, int y, int width, int height,
                          const char *text, uint32_t colour, lv_text_align_t align, lv_text_flag_t flag ) const
{
    if ( text == nullptr || text[0] == '\0' ) {
        return;
    }
    const lv_area_t box = { coords.x1 + x, coords.y1 + y, coords.x1 + x + width - 1, coords.y1 + y + height - 1 };
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
    label.flag = flag;
    label.text = text;
    // LVGL takes the clip area when it records the draw task.
    layer->_clip_area = cellClip;
    lv_draw_label( layer, &label, &box );
    layer->_clip_area = clip;
}

static int textHeight( const char *text, int width )
{
    lv_text_attributes_t attributes;
    lv_text_attributes_init( &attributes );
    attributes.max_width = width;
    attributes.text_flags = LV_TEXT_FLAG_NONE;
    lv_point_t size = {};
    lv_text_get_size_attributes( &size, text, &jetbrains_mono_26, &attributes );
    return size.y > jetbrains_mono_26.line_height ? size.y : jetbrains_mono_26.line_height;
}

// The height of a row of the centred layout: its highest cell, each wrapped to its width.
int TablePart::rowHeight( int row ) const
{
    const Layout &layout = m_layout;
    if ( layout.kinds[row] == Line ) {
        return textHeight( m_text.text( row, 0 ), layout.valueX + 2 * layout.valueWidth - layout.nameX );
    }
    int height = textHeight( m_text.text( row, 0 ), layout.nameWidth );
    if ( layout.kinds[row] == TwoValues ) {
        height = LV_MAX( height, textHeight( m_text.text( row, 1 ), layout.valueWidth ) );
        return LV_MAX( height, textHeight( m_text.text( row, 2 ), layout.valueWidth ) );
    }
    return LV_MAX( height, textHeight( m_text.text( row, 1 ), 2 * layout.valueWidth ) );
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
    const bool centred = layout.top < 0;
    int heights[TableText::MaxRows] = {};
    int tops[TableText::MaxRows] = {};
    int top = layout.top;
    for ( int row = 0; row < layout.rowCount; ++row ) {
        heights[row] = centred ? self->rowHeight( row ) : lineHeight;
        tops[row] = layout.top + layout.titleGap + row * layout.pitch;
    }
    if ( centred ) {
        top = TableText::columnTops( lineHeight, heights, layout.rowCount, layout.pitch - lineHeight,
                                     lv_area_get_height( &coords ), tops );
    }
    // Fixed rows keep their names on one line, as the earlier labels of their own width did.
    const lv_text_flag_t nameFlag = centred ? LV_TEXT_FLAG_NONE : LV_TEXT_FLAG_EXPAND;
    self->drawCell( layer, coords, 0, top, lv_area_get_width( &coords ), lineHeight, layout.title, TableText::Grey,
                    LV_TEXT_ALIGN_CENTER, LV_TEXT_FLAG_NONE );
    for ( int row = 0; row < layout.rowCount; ++row ) {
        const int y = tops[row];
        const int height = heights[row];
        const uint32_t valueColour = text.valueColour( row );
        if ( layout.kinds[row] == Line ) {
            self->drawCell( layer, coords, layout.nameX, y, layout.valueX + 2 * layout.valueWidth - layout.nameX, height,
                            text.text( row, 0 ), valueColour, LV_TEXT_ALIGN_CENTER, LV_TEXT_FLAG_NONE );
            continue;
        }
        self->drawCell( layer, coords, layout.nameX, y, layout.nameWidth, height, text.text( row, 0 ),
                        text.nameColour( row ), LV_TEXT_ALIGN_LEFT, nameFlag );
        if ( layout.kinds[row] == TwoValues ) {
            self->drawCell( layer, coords, layout.valueX, y, layout.valueWidth, height, text.text( row, 1 ), valueColour,
                            LV_TEXT_ALIGN_RIGHT, LV_TEXT_FLAG_NONE );
            self->drawCell( layer, coords, layout.valueX + layout.valueWidth, y, layout.valueWidth, height,
                            text.text( row, 2 ), valueColour, LV_TEXT_ALIGN_RIGHT, LV_TEXT_FLAG_NONE );
            continue;
        }
        const char *value = text.text( row, 1 );
        if ( layout.dotsLetters > 0 ) {
            TableText::fitWithDots( value, layout.dotsLetters, self->m_dots[row], sizeof( self->m_dots[row] ) );
            value = self->m_dots[row];
        }
        self->drawCell( layer, coords, layout.valueX, y, 2 * layout.valueWidth, height, value, valueColour,
                        LV_TEXT_ALIGN_RIGHT, LV_TEXT_FLAG_NONE );
    }
}
