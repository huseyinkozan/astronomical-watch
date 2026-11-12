#include "AodPage.h"

#include "Panel.h"

static const lv_color_t s_colours[3] = { LV_COLOR_MAKE( 0xFF, 0xFF, 0xFF ), LV_COLOR_MAKE( 0xFF, 0xFF, 0x00 ),
                                         LV_COLOR_MAKE( 0xFF, 0x00, 0x00 ) };

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
    const AlwaysOn::Face face = AlwaysOn::layout( { 0, AlwaysOn::MoonItem, 50, true }, { 2026, 1, 1, 0, 0, 0 }, false );
    const int size = face.sprites[face.count - 1].glyph->width;
    m_moon = lv_canvas_create( m_content );
    lv_canvas_set_draw_buf( m_moon, lv_draw_buf_create( size, size, LV_COLOR_FORMAT_RGB565, 0 ) );
    lv_obj_set_hidden( m_moon, true );
}

static void setTranslateY( void *object, int32_t y )
{
    lv_obj_set_style_translate_y( static_cast<lv_obj_t *>( object ), y, 0 );
}

void AodPage::show( const AlwaysOn::Look &look, const Rtc::DateTime &localTime, int direction )
{
    if ( m_page == nullptr ) {
        return;
    }
    const AlwaysOn::Face face = AlwaysOn::layout( look, localTime, true );
    // Centred as on the first always-on minute (it then moves a few pixels against burn-in).
    const int faceX = ( ( Panel::WIDTH - face.width ) / 2 ) & ~1;
    const int faceY = ( ( Panel::HEIGHT - face.height ) / 2 ) & ~1;
    int used = 0;
    lv_obj_set_hidden( m_moon, true );
    for ( int i = 0; i < face.count; ++i ) {
        const AlwaysOn::Sprite &sprite = face.sprites[i];
        if ( sprite.font == nullptr ) {
            // The moon: its bitmap, pixel by pixel, in yellow on black.
            const AodGlyph &glyph = *sprite.glyph;
            lv_canvas_fill_bg( m_moon, lv_color_black(), LV_OPA_COVER );
            for ( int y = 0; y < glyph.height; ++y ) {
                for ( int x = 0; x < glyph.width; ++x ) {
                    const int bit = y * glyph.width + x;
                    if ( glyph.bits[bit / 8] & ( 0x80 >> ( bit % 8 ) ) ) {
                        lv_canvas_set_px( m_moon, x, y, s_colours[sprite.colour], LV_OPA_COVER );
                    }
                }
            }
            lv_obj_set_pos( m_moon, faceX + sprite.x, faceY + sprite.y );
            lv_obj_set_hidden( m_moon, false );
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
