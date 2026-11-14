#include "AodPage.h"

#include "Panel.h"

// By AlwaysOn::Colour.
static const lv_color_t s_colours[6] = { LV_COLOR_MAKE( 0xFF, 0xFF, 0xFF ), LV_COLOR_MAKE( 0xFF, 0xFF, 0x00 ),
                                         LV_COLOR_MAKE( 0xFF, 0x00, 0x00 ), LV_COLOR_MAKE( 0x00, 0xFF, 0x00 ),
                                         LV_COLOR_MAKE( 0x00, 0x00, 0xFF ), LV_COLOR_MAKE( 0x80, 0x80, 0x80 ) };

// Draws a moon bitmap into a canvas of at least its size, white on black.
static void paintMoon( lv_obj_t *canvas, const AodGlyph &moon )
{
    lv_canvas_fill_bg( canvas, lv_color_black(), LV_OPA_COVER );
    for ( int y = 0; y < moon.height; ++y ) {
        for ( int x = 0; x < moon.width; ++x ) {
            const int bit = y * moon.width + x;
            if ( moon.bits[bit / 8] & ( 0x80 >> ( bit % 8 ) ) ) {
                lv_canvas_set_px( canvas, x, y, s_colours[AlwaysOn::White], LV_OPA_COVER );
            }
        }
    }
}

// UTF-8 of a code point up to U+FFFF.
static void utf8( uint32_t code, char *text )
{
    if ( code < 0x80 ) {
        text[0] = static_cast<char>( code );
        text[1] = '\0';
    } else if ( code < 0x800 ) {
        text[0] = static_cast<char>( 0xC0 | ( code >> 6 ) );
        text[1] = static_cast<char>( 0x80 | ( code & 0x3F ) );
        text[2] = '\0';
    } else {
        text[0] = static_cast<char>( 0xE0 | ( code >> 12 ) );
        text[1] = static_cast<char>( 0x80 | ( ( code >> 6 ) & 0x3F ) );
        text[2] = static_cast<char>( 0x80 | ( code & 0x3F ) );
        text[3] = '\0';
    }
}

void AodPage::build( lv_obj_t *page )
{
    m_page = page;
    lv_obj_set_scrollbar_mode( page, LV_SCROLLBAR_MODE_OFF );
    m_content = lv_obj_create( page );
    lv_obj_remove_style_all( m_content );
    lv_obj_set_size( m_content, Panel::WIDTH, Panel::HEIGHT );
    // Not clickable: swipes go to the page, which takes the vertical ones.
    lv_obj_set_clickable( m_content, false );
    lv_obj_set_scrollable( m_content, false );
    for ( int i = 0; i < AlwaysOn::MaxSprites; ++i ) {
        m_labels[i] = lv_label_create( m_content );
        lv_obj_set_hidden( m_labels[i], true );
    }
    // The moon changes with the phase, so it is a small canvas rather than a glyph.
    const AlwaysOn::Look look = { 0, AlwaysOn::MoonItem, 50, true, {}, 0, 0 };
    const int size = AlwaysOn::moon( look )->width;
    m_moon = lv_canvas_create( m_content );
    lv_obj_set_pos( m_moon, AlwaysOn::MoonX, AlwaysOn::MoonY );
    lv_canvas_set_draw_buf( m_moon, lv_draw_buf_create( size, size, LV_COLOR_FORMAT_RGB565, 0 ) );
    lv_obj_set_hidden( m_moon, true );
    m_shapesObject = lv_obj_create( m_content );
    lv_obj_remove_style_all( m_shapesObject );
    lv_obj_set_size( m_shapesObject, Panel::WIDTH, Panel::HEIGHT );
    lv_obj_set_clickable( m_shapesObject, false );
    lv_obj_add_event_cb( m_shapesObject, onDrawShapes, LV_EVENT_DRAW_MAIN, this );
    // Over the shapes. The sky moon has radius 10; the canvas is made for the largest moon.
    m_skyMoon = lv_canvas_create( m_content );
    lv_canvas_set_draw_buf( m_skyMoon, lv_draw_buf_create( 21, 21, LV_COLOR_FORMAT_RGB565, 0 ) );
    lv_obj_set_hidden( m_skyMoon, true );
}

void AodPage::onDrawShapes( lv_event_t *event )
{
    AodPage *self = static_cast<AodPage *>( lv_event_get_user_data( event ) );
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t area = {};
    lv_obj_get_coords( self->m_shapesObject, &area );
    const int originX = area.x1 + self->m_faceX;
    const int originY = area.y1 + self->m_faceY;
    for ( int i = 0; i < self->m_shapeCount; ++i ) {
        const AlwaysOn::Shape &shape = self->m_shapes[i];
        const lv_color_t colour = s_colours[shape.colour];
        const int x = originX + shape.x0;
        const int y = originY + shape.y0;
        if ( shape.kind == AlwaysOn::DotShape || shape.kind == AlwaysOn::DiscShape ) {
            lv_draw_rect_dsc_t rect;
            lv_draw_rect_dsc_init( &rect );
            rect.bg_color = colour;
            const int r = shape.kind == AlwaysOn::DotShape ? 0 : shape.radius;
            rect.radius = shape.kind == AlwaysOn::DotShape ? 0 : LV_RADIUS_CIRCLE;
            const lv_area_t box = { x - r, y - r, shape.kind == AlwaysOn::DotShape ? x + 1 : x + r,
                                    shape.kind == AlwaysOn::DotShape ? y + 1 : y + r };
            lv_draw_rect( layer, &rect, &box );
        } else if ( shape.kind == AlwaysOn::RingShape ) {
            lv_draw_arc_dsc_t arc;
            lv_draw_arc_dsc_init( &arc );
            arc.color = colour;
            arc.width = shape.width;
            arc.center.x = x;
            arc.center.y = y;
            arc.radius = shape.radius;
            arc.start_angle = 0;
            arc.end_angle = 360;
            lv_draw_arc( layer, &arc );
        } else {
            lv_draw_line_dsc_t line;
            lv_draw_line_dsc_init( &line );
            line.color = colour;
            line.width = shape.width;
            line.p1.x = x;
            line.p1.y = y;
            line.p2.x = originX + shape.x1;
            line.p2.y = originY + shape.y1;
            lv_draw_line( layer, &line );
        }
    }
}

static void setTranslateY( void *object, int32_t y )
{
    lv_obj_set_style_translate_y( static_cast<lv_obj_t *>( object ), y, 0 );
}

void AodPage::show( const AlwaysOn::Look &look, const Rtc::DateTime &localTime, int64_t utcNow, int direction )
{
    if ( m_page == nullptr ) {
        return;
    }
    const AlwaysOn::Face &face = AlwaysOn::layout( look, localTime, utcNow );
    // Centred as on the first always-on minute (it then moves a few pixels against burn-in); the
    // sky styles have a fixed place.
    const int faceX = face.x >= 0 ? face.x : ( ( Panel::WIDTH - face.width ) / 2 ) & ~1;
    const int faceY = face.y >= 0 ? face.y : ( ( Panel::HEIGHT - face.height ) / 2 ) & ~1;
    m_faceX = faceX;
    m_faceY = faceY;
    m_shapeCount = face.shapeCount;
    for ( int i = 0; i < face.shapeCount; ++i ) {
        m_shapes[i] = face.shapes[i];
    }
    lv_obj_invalidate( m_shapesObject );
    // The moon on its path is a sprite without font (sky styles).
    lv_obj_set_hidden( m_skyMoon, true );
    int used = 0;
    for ( int i = 0; i < face.count; ++i ) {
        const AlwaysOn::Sprite &sprite = face.sprites[i];
        if ( sprite.font == nullptr ) {
            paintMoon( m_skyMoon, *sprite.glyph );
            lv_obj_set_pos( m_skyMoon, faceX + sprite.x, faceY + sprite.y );
            lv_obj_set_hidden( m_skyMoon, false );
            continue;
        }
        lv_obj_t *label = m_labels[used++];
        char text[4];
        utf8( sprite.code, text );
        lv_obj_set_style_text_font( label, sprite.font, 0 );
        lv_obj_set_style_text_color( label, s_colours[sprite.colour], 0 );
        lv_label_set_text( label, text );
        // The glyphs hang at the bottom of the font's line (base line 0, no offsets).
        lv_obj_set_pos( label, faceX + sprite.x, faceY + sprite.y - ( lv_font_get_line_height( sprite.font ) - sprite.glyph->height ) );
        lv_obj_set_hidden( label, false );
    }
    for ( int i = used; i < AlwaysOn::MaxSprites; ++i ) {
        lv_obj_set_hidden( m_labels[i], true );
    }
    // The corner moon of the digit styles, at its fixed place (computed after the layout, which
    // shares the moon's buffer).
    const AodGlyph *moon = AlwaysOn::moon( look );
    lv_obj_set_hidden( m_moon, moon == nullptr );
    if ( moon != nullptr ) {
        paintMoon( m_moon, *moon );
    }
    lv_anim_delete( m_content, setTranslateY );
    lv_obj_set_style_translate_y( m_content, 0, 0 );
    if ( direction != 0 ) {
        lv_anim_t animation;
        lv_anim_init( &animation );
        lv_anim_set_var( &animation, m_content );
        lv_anim_set_exec_cb( &animation, setTranslateY );
        lv_anim_set_values( &animation, direction * Panel::HEIGHT, 0 );
        lv_anim_set_duration( &animation, 250 );
        lv_anim_set_path_cb( &animation, lv_anim_path_ease_out );
        lv_anim_start( &animation );
    }
}
