#include "Styles.h"

namespace {

struct TextStyle
{
    const lv_font_t *font;
    uint32_t colour;
    lv_style_t style;
};

// Enough for the font and colour pairs of today's pages (white, grey and amber in two sizes, the
// face's digits).
const int s_maxTextStyles = 12;
TextStyle s_textStyles[s_maxTextStyles];
int s_textStyleCount = 0;

bool s_initialised = false;
lv_style_t s_centred;
lv_style_t s_row;
lv_style_t s_rowPressed;
lv_style_t s_button;
lv_style_t s_launcherRow;
lv_style_t s_launcherRowPressed;
lv_style_t s_launcherRowCurrent;

void initialise()
{
    if ( s_initialised ) {
        return;
    }
    s_initialised = true;
    lv_style_init( &s_centred );
    lv_style_set_text_align( &s_centred, LV_TEXT_ALIGN_CENTER );
    // A black row with a mid-grey outline: large areas of dark grey shimmer on this AMOLED panel
    // (seen on the board), and black pixels draw no current.
    lv_style_init( &s_row );
    lv_style_set_border_width( &s_row, 2 );
    lv_style_set_border_color( &s_row, lv_color_hex( 0x808080 ) );
    lv_style_set_radius( &s_row, 24 );
    lv_style_set_pad_left( &s_row, 24 );
    lv_style_set_pad_right( &s_row, 24 );
    lv_style_set_bg_color( &s_row, lv_color_black() );
    // Lit while pressed, so the finger sees that the tap arrived.
    lv_style_init( &s_rowPressed );
    lv_style_set_bg_color( &s_rowPressed, lv_color_hex( 0x606060 ) );
    lv_style_init( &s_button );
    lv_style_set_border_width( &s_button, 2 );
    lv_style_set_border_color( &s_button, lv_color_hex( 0x808080 ) );
    lv_style_set_radius( &s_button, 18 );
    lv_style_set_pad_all( &s_button, 0 );
    lv_style_set_bg_color( &s_button, lv_color_black() );
    lv_style_init( &s_launcherRow );
    lv_style_set_border_width( &s_launcherRow, 1 );
    lv_style_set_border_color( &s_launcherRow, lv_color_hex( 0x2C2C2C ) );
    lv_style_set_border_opa( &s_launcherRow, LV_OPA_COVER );
    lv_style_set_radius( &s_launcherRow, 16 );
    lv_style_set_width( &s_launcherRow, Styles::LauncherRowWidth );
    lv_style_set_height( &s_launcherRow, Styles::LauncherRowHeight );
    // The label at the content's top left, in the middle of the row as LV_ALIGN_LEFT_MID put it
    // (inside the 1 px border, the 26 px font's line of about 32 px).
    lv_style_set_pad_left( &s_launcherRow, 16 );
    lv_style_set_pad_top( &s_launcherRow, ( Styles::LauncherRowHeight - 2 - 32 ) / 2 );
    // The part last opened: a brighter outline, no fill (dark-grey areas shimmer on this panel).
    lv_style_init( &s_launcherRowCurrent );
    lv_style_set_border_color( &s_launcherRowCurrent, lv_color_hex( 0x707070 ) );
    lv_style_init( &s_launcherRowPressed );
    lv_style_set_bg_color( &s_launcherRowPressed, lv_color_hex( 0x404040 ) );
    lv_style_set_bg_opa( &s_launcherRowPressed, LV_OPA_COVER );
}

}  // namespace

void Styles::text( lv_obj_t *object, const lv_font_t *font, lv_color_t colour )
{
    if ( object == nullptr ) {
        return;
    }
    const uint32_t rgb = lv_color_to_u32( colour ) & 0xFFFFFF;
    for ( int i = 0; i < s_textStyleCount; ++i ) {
        if ( s_textStyles[i].font == font && s_textStyles[i].colour == rgb ) {
            lv_obj_add_style( object, &s_textStyles[i].style, 0 );
            return;
        }
    }
    if ( s_textStyleCount == s_maxTextStyles ) {
        lv_obj_set_style_text_font( object, font, 0 );
        lv_obj_set_style_text_color( object, colour, 0 );
        return;
    }
    TextStyle &entry = s_textStyles[s_textStyleCount++];
    entry.font = font;
    entry.colour = rgb;
    lv_style_init( &entry.style );
    lv_style_set_text_font( &entry.style, font );
    lv_style_set_text_color( &entry.style, colour );
    lv_obj_add_style( object, &entry.style, 0 );
}

void Styles::centred( lv_obj_t *object )
{
    initialise();
    lv_obj_add_style( object, &s_centred, 0 );
}

void Styles::row( lv_obj_t *object )
{
    initialise();
    lv_obj_add_style( object, &s_row, 0 );
    lv_obj_add_style( object, &s_rowPressed, LV_STATE_PRESSED );
}

void Styles::launcherRow( lv_obj_t *object )
{
    initialise();
    lv_obj_add_style( object, &s_launcherRow, 0 );
    lv_obj_add_style( object, &s_launcherRowCurrent, LV_STATE_CHECKED );
    lv_obj_add_style( object, &s_launcherRowPressed, LV_STATE_PRESSED );
}

void Styles::button( lv_obj_t *object )
{
    initialise();
    lv_obj_add_style( object, &s_button, 0 );
    lv_obj_add_style( object, &s_rowPressed, LV_STATE_PRESSED );
}
