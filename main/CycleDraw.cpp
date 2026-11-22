#include "CycleDraw.h"

#include <cmath>

namespace CycleDraw
{

lv_color_t red() { return lv_color_hex( 0xFF5064 ); }
lv_color_t dimRed() { return lv_color_hex( 0x963C48 ); }
lv_color_t blue() { return lv_color_hex( 0x5A96FF ); }
lv_color_t dimBlue() { return lv_color_hex( 0x3C5A96 ); }
lv_color_t grey() { return lv_color_hex( 0x969696 ); }
lv_color_t dimGrey() { return lv_color_hex( 0x505050 ); }
lv_color_t amber() { return lv_color_hex( 0xFFB040 ); }

float flowScale( int flow )
{
    return flow <= 1 ? 0.75f : ( flow == 2 ? 1.0f : 1.3f );
}

bool inClip( const lv_layer_t *layer, const lv_area_t &box )
{
    const lv_area_t &clip = layer->_clip_area;
    return box.x2 >= clip.x1 && box.x1 <= clip.x2 && box.y2 >= clip.y1 && box.y1 <= clip.y2;
}

static void disc( lv_layer_t *layer, int x, int y, int r, lv_color_t colour, int border )
{
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init( &dsc );
    dsc.radius = LV_RADIUS_CIRCLE;
    if ( border > 0 ) {
        dsc.bg_opa = LV_OPA_TRANSP;
        dsc.border_color = colour;
        dsc.border_width = border;
    } else {
        dsc.bg_color = colour;
    }
    const lv_area_t area = { x - r, y - r, x + r, y + r };
    lv_draw_rect( layer, &dsc, &area );
}

static void line( lv_layer_t *layer, float x1, float y1, float x2, float y2, int width, lv_color_t colour )
{
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init( &dsc );
    dsc.color = colour;
    dsc.width = width;
    dsc.p1.x = static_cast<int32_t>( lroundf( x1 ) );
    dsc.p1.y = static_cast<int32_t>( lroundf( y1 ) );
    dsc.p2.x = static_cast<int32_t>( lroundf( x2 ) );
    dsc.p2.y = static_cast<int32_t>( lroundf( y2 ) );
    lv_draw_line( layer, &dsc );
}

static void arc( lv_layer_t *layer, int x, int y, int r, int width, float a0, float a1, lv_color_t colour )
{
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init( &dsc );
    dsc.color = colour;
    dsc.width = width;
    dsc.center.x = x;
    dsc.center.y = y;
    dsc.radius = static_cast<uint16_t>( r );
    // LVGL takes 0 to 360 (an end before the start wraps through 0); the spiral's angles run on.
    dsc.start_angle = fmodf( fmodf( a0, 360.0f ) + 360.0f, 360.0f );
    dsc.end_angle = fmodf( fmodf( a1, 360.0f ) + 360.0f, 360.0f );
    lv_draw_arc( layer, &dsc );
}

void drop( lv_layer_t *layer, int x, int y, float scale, lv_color_t colour, bool half )
{
    const int r = static_cast<int>( lroundf( 4.2f * scale ) );
    const int cy = y + static_cast<int>( lroundf( 2 * scale ) );
    const float tip = y - 8 * scale;
    const lv_area_t box = { x - r - 1, static_cast<int32_t>( tip ) - 1, x + r + 1, cy + r + 1 };
    if ( !inClip( layer, box ) ) {
        return;
    }
    if ( half ) {
        disc( layer, x, cy, r, colour, 1 );
        line( layer, x, tip, x - r * 0.9f, cy - r * 0.45f, 1, colour );
        line( layer, x, tip, x + r * 0.9f, cy - r * 0.45f, 1, colour );
        // The lower half: an arc as thick as the radius, from 3 to 9 o'clock through 6.
        arc( layer, x, cy, r, r, 0, 180, colour );
        return;
    }
    disc( layer, x, cy, r, colour, 0 );
    lv_draw_triangle_dsc_t triangle;
    lv_draw_triangle_dsc_init( &triangle );
    triangle.color = colour;
    triangle.p[0].x = x;
    triangle.p[0].y = static_cast<int32_t>( lroundf( tip ) );
    triangle.p[1].x = static_cast<int32_t>( lroundf( x - r * 0.95f ) );
    triangle.p[1].y = cy - r / 3;
    triangle.p[2].x = static_cast<int32_t>( lroundf( x + r * 0.95f ) );
    triangle.p[2].y = cy - r / 3;
    lv_draw_triangle( layer, &triangle );
}

void egg( lv_layer_t *layer, int x, int y, bool big, lv_color_t colour )
{
    const int r = big ? 7 : 5;
    const lv_area_t box = { x - r, y - r, x + r, y + r };
    if ( inClip( layer, box ) ) {
        disc( layer, x, y, r, colour, big ? 2 : 1 );
    }
}

void spermEgg( lv_layer_t *layer, int x, int y, int outAngle, lv_color_t colour )
{
    const int r = 6;
    const lv_area_t box = { x - r - 12, y - r - 12, x + r + 12, y + r + 12 };
    if ( !inClip( layer, box ) ) {
        return;
    }
    arc( layer, x, y, r, 2, static_cast<float>( outAngle + 32 ), static_cast<float>( outAngle + 360 - 32 ), colour );
    disc( layer, x, y, 2, colour, 0 );
    const float a = outAngle * static_cast<float>( M_PI ) / 180.0f;
    line( layer, x, y, x + 11 * cosf( a ), y + 11 * sinf( a ), 1, colour );
}

void dayBox( lv_layer_t *layer, int cx, int cy, int r, float a0, float a1, bool filled, lv_color_t colour )
{
    const float m = ( a0 + a1 ) / 2 * static_cast<float>( M_PI ) / 180.0f;
    const int mx = cx + static_cast<int>( r * cosf( m ) );
    const int my = cy + static_cast<int>( r * sinf( m ) );
    const lv_area_t box = { mx - 20, my - 20, mx + 20, my + 20 };
    if ( !inClip( layer, box ) ) {
        return;
    }
    if ( filled ) {
        arc( layer, cx, cy, r + 8, 16, a0, a1, colour );
        return;
    }
    arc( layer, cx, cy, r + 8, 2, a0, a1, colour );
    arc( layer, cx, cy, r - 6, 2, a0, a1, colour );
    const float p0 = a0 * static_cast<float>( M_PI ) / 180.0f;
    const float p1 = a1 * static_cast<float>( M_PI ) / 180.0f;
    line( layer, cx + ( r - 7 ) * cosf( p0 ), cy + ( r - 7 ) * sinf( p0 ), cx + ( r + 7 ) * cosf( p0 ),
          cy + ( r + 7 ) * sinf( p0 ), 2, colour );
    line( layer, cx + ( r - 7 ) * cosf( p1 ), cy + ( r - 7 ) * sinf( p1 ), cx + ( r + 7 ) * cosf( p1 ),
          cy + ( r + 7 ) * sinf( p1 ), 2, colour );
}

void moon( lv_layer_t *layer, int x, int y, int radius, double phase, lv_color_t lit, lv_color_t dark )
{
    const lv_area_t box = { x - radius, y - radius, x + radius, y + radius };
    if ( !inClip( layer, box ) ) {
        return;
    }
    disc( layer, x, y, radius, dark, 0 );
    // The lit part row by row: from the terminator (an ellipse of half-width k) to the limb.
    const float k = cosf( static_cast<float>( 2 * M_PI * phase ) );
    const bool waxing = phase < 0.5;
    lv_draw_rect_dsc_t row;
    lv_draw_rect_dsc_init( &row );
    row.bg_color = lit;
    for ( int dy = -radius; dy <= radius; ++dy ) {
        const float half = sqrtf( static_cast<float>( radius * radius - dy * dy ) );
        float from = k * half;
        float to = half;
        if ( !waxing ) {
            const float t = from;
            from = -to;
            to = -t;
        }
        const int x1 = x + static_cast<int>( lroundf( from ) );
        const int x2 = x + static_cast<int>( lroundf( to ) ) - 1;
        if ( x2 < x1 ) {
            continue;
        }
        const lv_area_t area = { x1, y + dy, x2, y + dy };
        lv_draw_rect( layer, &row, &area );
    }
}

}  // namespace CycleDraw
