#include "WatchFace.h"
#include "Language.h"

#include <cmath>
#include <cstdio>

#include "Corners.h"
#include "Fonts.h"
#include "SkyPages.h"

static const int s_width = 410;
static const int s_height = 502;
// The Moon in the bottom-left corner, inside the Moon's gauge, as on the always-on face
// (AlwaysOn::MoonX, MoonY).
static const int s_moonSize = 37;
static const int s_moonX = 48;
static const int s_moonY = 417;
static const lv_color_t s_grey = LV_COLOR_MAKE( 0xA0, 0xA0, 0xA0 );
// The dial's texts: next to its white ticks and hands s_grey looked white on the panel.
static const lv_color_t s_dialGrey = LV_COLOR_MAKE( 0x70, 0x70, 0x70 );
static const lv_color_t s_amber = LV_COLOR_MAKE( 0xFF, 0xB0, 0x40 );


static const uint32_t s_slideMs = 250;

// The day arc: centre and radius.
static const int s_arcX = 205;
static const int s_arcY = 200;
static const int s_arcRadius = 160;

// The sky arcs: centre on the horizon and the paths' radii; the dial: centre and orbits.
static const int s_skyX = 205;
static const int s_skyY = 236;
static const int s_skySunRadius = 180;
static const int s_skyMoonRadius = 132;
static const int s_skyMoonSize = 23;
static const int s_dialX = 205;
static const int s_dialY = 243;
static const int s_dialSunRadius = 182;
static const int s_dialMoonRadius = 140;
static const int s_dialMoonSize = 25;
static const lv_color_t s_sunPath = LV_COLOR_MAKE( 0x80, 0x60, 0x28 );
static const lv_color_t s_sunPathDim = LV_COLOR_MAKE( 0x40, 0x30, 0x18 );
static const lv_color_t s_moonPath = LV_COLOR_MAKE( 0x58, 0x60, 0x74 );
static const lv_color_t s_moonPathDim = LV_COLOR_MAKE( 0x28, 0x2C, 0x34 );

// A moon of `size` pixels into `canvas`: the lit part in pale white, the dark part dim, the
// terminator half an ellipse; 4 x 4 samples per pixel smooth the edges. `dim` darkens both (the
// Moon below the horizon).
static void paintMoon( lv_obj_t *canvas, int size, int percent, bool waxing, bool dim, lv_color_t dark )
{
    lv_color_t lit = lv_color_make( 0xE6, 0xE8, 0xF0 );
    if ( dim ) {
        lit = lv_color_mix( lit, lv_color_black(), 140 );
        dark = lv_color_mix( dark, lv_color_black(), 140 );
    }
    const double radius = size / 2.0 - 1;
    const double centre = size / 2.0;
    const double fraction = percent / 100.0;
    for ( int y = 0; y < size; ++y ) {
        for ( int x = 0; x < size; ++x ) {
            int litSamples = 0;
            int darkSamples = 0;
            for ( int sy = 0; sy < 4; ++sy ) {
                for ( int sx = 0; sx < 4; ++sx ) {
                    const double dx = x + ( sx + 0.5 ) / 4 - centre;
                    const double dy = y + ( sy + 0.5 ) / 4 - centre;
                    if ( dx * dx + dy * dy > radius * radius ) {
                        continue;
                    }
                    const double half = std::sqrt( radius * radius - dy * dy );
                    const double terminator = half * ( 1 - 2 * fraction );
                    if ( waxing ? dx >= terminator : -dx >= terminator ) {
                        ++litSamples;
                    } else {
                        ++darkSamples;
                    }
                }
            }
            const lv_color_t colour = lv_color_mix( lit, lv_color_black(), static_cast<uint8_t>( litSamples * 255 / 16 ) );
            lv_canvas_set_px( canvas, x, y, lv_color_mix( dark, colour, static_cast<uint8_t>( darkSamples * 255 / 16 ) ),
                              LV_OPA_COVER );
        }
    }
}

static lv_obj_t *createArc( lv_obj_t *parent, int centreX, int centreY, int radius, lv_color_t colour )
{
    lv_obj_t *arc = lv_arc_create( parent );
    lv_obj_remove_style_all( arc );
    lv_obj_set_size( arc, 2 * radius, 2 * radius );
    lv_obj_set_pos( arc, centreX - radius, centreY - radius );
    lv_arc_set_bg_angles( arc, 180, 360 );
    lv_obj_set_style_arc_width( arc, 2, LV_PART_MAIN );
    lv_obj_set_style_arc_color( arc, colour, LV_PART_MAIN );
    lv_obj_set_style_arc_opa( arc, LV_OPA_COVER, LV_PART_MAIN );
    lv_obj_set_clickable( arc, false );
    return arc;
}

static lv_obj_t *createDot( lv_obj_t *parent, int size, lv_color_t colour )
{
    lv_obj_t *dot = lv_obj_create( parent );
    lv_obj_remove_style_all( dot );
    lv_obj_set_size( dot, size, size );
    lv_obj_set_style_radius( dot, LV_RADIUS_CIRCLE, 0 );
    lv_obj_set_style_bg_color( dot, colour, 0 );
    lv_obj_set_style_bg_opa( dot, LV_OPA_COVER, 0 );
    lv_obj_set_clickable( dot, false );
    return dot;
}

static lv_obj_t *createCanvas( lv_obj_t *parent, int size )
{
    lv_obj_t *canvas = lv_canvas_create( parent );
    lv_canvas_set_draw_buf( canvas, lv_draw_buf_create( size, size, LV_COLOR_FORMAT_RGB565, 0 ) );
    lv_canvas_fill_bg( canvas, lv_color_black(), LV_OPA_COVER );
    return canvas;
}

enum Anchor
{
    Centre,
    Left,
    Right
};

// A label with its anchor point at (x, y): the middle of its left edge, centre or right edge.
static lv_obj_t *label( lv_obj_t *parent, const lv_font_t *font, lv_color_t colour, Anchor anchor, int x, int y )
{
    lv_obj_t *text = lv_label_create( parent );
    lv_obj_set_style_text_font( text, font, 0 );
    lv_obj_set_style_text_color( text, colour, 0 );
    lv_label_set_text( text, "" );
    if ( anchor == Centre ) {
        lv_obj_align( text, LV_ALIGN_CENTER, x - s_width / 2, y - s_height / 2 );
    } else if ( anchor == Left ) {
        lv_obj_align( text, LV_ALIGN_LEFT_MID, x, y - s_height / 2 );
    } else {
        lv_obj_align( text, LV_ALIGN_RIGHT_MID, x - s_width, y - s_height / 2 );
    }
    return text;
}

static lv_obj_t *createRoot( lv_obj_t *page )
{
    lv_obj_t *root = lv_obj_create( page );
    lv_obj_remove_style_all( root );
    lv_obj_set_size( root, s_width, s_height );
    // Not clickable: presses and swipes go to the page, which takes the vertical swipe.
    lv_obj_set_clickable( root, false );
    lv_obj_set_scrollable( root, false );
    return root;
}

// "HH:MM" of a Unix time in local time, or "--:--".
static void formatTime( char *text, size_t size, const Astronomy::Event &event, int utcOffsetMinutes )
{
    if ( !event.occurs ) {
        snprintf( text, size, "--:--" );
        return;
    }
    int64_t seconds = ( event.time + static_cast<int64_t>( utcOffsetMinutes ) * 60 ) % 86400;
    if ( seconds < 0 ) {
        seconds += 86400;
    }
    const int minutes = static_cast<int>( ( seconds + 30 ) / 60 ) % ( 24 * 60 );
    snprintf( text, size, "%02d:%02d", minutes / 60, minutes % 60 );
}

static int weekday( const Rtc::DateTime &time )
{
    // Days since 1970-01-01 (a Thursday), H. Hinnant's days_from_civil.
    const int year = time.year - ( time.month <= 2 ? 1 : 0 );
    const int era = year / 400;
    const int yearOfEra = year - era * 400;
    const int dayOfYear = ( 153 * ( time.month + ( time.month > 2 ? -3 : 9 ) ) + 2 ) / 5 + time.day - 1;
    const int days = era * 146097 + yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear - 719468;
    return ( ( days + 4 ) % 7 + 7 ) % 7;
}

void WatchFace::buildClassic( lv_obj_t *page )
{
    Layout &layout = m_layouts[0];
    layout.root = createRoot( page );
    layout.time = label( layout.root, &jetbrains_mono_light_110, lv_color_white(), Centre, 205, 150 );
    layout.seconds = label( layout.root, &jetbrains_mono_26, s_grey, Centre, 205, 224 );
    layout.date = label( layout.root, &jetbrains_mono_26, lv_color_white(), Centre, 205, 280 );
    layout.hijri = label( layout.root, &jetbrains_mono_26, s_grey, Centre, 205, 316 );
    layout.sunName = label( layout.root, &jetbrains_mono_26, s_amber, Centre, 205, 396 );
    layout.sunTime = label( layout.root, &jetbrains_mono_26, lv_color_white(), Centre, 205, 430 );
    layout.sunLeft = label( layout.root, &jetbrains_mono_26, s_grey, Centre, 205, 462 );
}

void WatchFace::buildArc( lv_obj_t *page )
{
    Layout &layout = m_layouts[1];
    layout.root = createRoot( page );
    layout.arc = lv_arc_create( layout.root );
    lv_obj_remove_style_all( layout.arc );
    lv_obj_set_size( layout.arc, 2 * s_arcRadius, 2 * s_arcRadius );
    lv_obj_set_pos( layout.arc, s_arcX - s_arcRadius, s_arcY - s_arcRadius );
    lv_arc_set_bg_angles( layout.arc, 180, 360 );
    lv_obj_set_style_arc_width( layout.arc, 4, LV_PART_MAIN );
    lv_obj_set_style_arc_color( layout.arc, lv_color_make( 0x80, 0x60, 0x28 ), LV_PART_MAIN );
    lv_obj_set_style_arc_opa( layout.arc, LV_OPA_COVER, LV_PART_MAIN );
    lv_obj_set_clickable( layout.arc, false );
    m_horizonPoints[0] = { 20, s_arcY };
    m_horizonPoints[1] = { s_width - 20, s_arcY };
    layout.horizon = lv_line_create( layout.root );
    lv_line_set_points( layout.horizon, m_horizonPoints, 2 );
    lv_obj_set_style_line_width( layout.horizon, 2, 0 );
    lv_obj_set_style_line_color( layout.horizon, lv_color_make( 0x60, 0x60, 0x60 ), 0 );
    layout.sunDot = lv_obj_create( layout.root );
    lv_obj_remove_style_all( layout.sunDot );
    lv_obj_set_size( layout.sunDot, 24, 24 );
    lv_obj_set_style_radius( layout.sunDot, LV_RADIUS_CIRCLE, 0 );
    lv_obj_set_style_bg_color( layout.sunDot, s_amber, 0 );
    lv_obj_set_style_bg_opa( layout.sunDot, LV_OPA_COVER, 0 );
    lv_obj_set_clickable( layout.sunDot, false );
    layout.riseText = label( layout.root, &jetbrains_mono_26, s_grey, Centre, s_arcX - s_arcRadius, s_arcY + 24 );
    layout.setText = label( layout.root, &jetbrains_mono_26, s_grey, Centre, s_arcX + s_arcRadius, s_arcY + 24 );
    layout.time = label( layout.root, &jetbrains_mono_light_90, lv_color_white(), Centre, 205, 150 );
    layout.date = label( layout.root, &jetbrains_mono_26, lv_color_white(), Centre, 205, 290 );
    layout.hijri = label( layout.root, &jetbrains_mono_26, s_grey, Centre, 205, 326 );
    layout.sunName = label( layout.root, &jetbrains_mono_26, s_amber, Centre, 205, 400 );
}

void WatchFace::buildSkyArcs( lv_obj_t *page )
{
    Layout &layout = m_layouts[2];
    layout.root = createRoot( page );
    layout.arc = createArc( layout.root, s_skyX, s_skyY, s_skySunRadius, s_sunPath );
    layout.moonArc = createArc( layout.root, s_skyX, s_skyY, s_skyMoonRadius, s_moonPath );
    m_skyHorizonPoints[0] = { 16, s_skyY };
    m_skyHorizonPoints[1] = { s_width - 16, s_skyY };
    layout.horizon = lv_line_create( layout.root );
    lv_line_set_points( layout.horizon, m_skyHorizonPoints, 2 );
    lv_obj_set_style_line_width( layout.horizon, 2, 0 );
    lv_obj_set_style_line_color( layout.horizon, lv_color_make( 0x60, 0x60, 0x60 ), 0 );
    layout.sunDot = createDot( layout.root, 24, s_amber );
    layout.skyMoon = createCanvas( layout.root, s_skyMoonSize );
    layout.time = label( layout.root, &jetbrains_mono_light_110, lv_color_white(), Centre, 205, 318 );
    layout.date = label( layout.root, &jetbrains_mono_26, lv_color_white(), Centre, 205, 400 );
    layout.hijri = label( layout.root, &jetbrains_mono_26, s_grey, Centre, 205, 432 );
    layout.dayName = label( layout.root, &jetbrains_mono_26, lv_color_white(), Centre, 205, 464 );
}

void WatchFace::buildDial( lv_obj_t *page )
{
    Layout &layout = m_layouts[3];
    layout.root = createRoot( page );
    layout.dial = lv_obj_create( layout.root );
    lv_obj_remove_style_all( layout.dial );
    lv_obj_set_size( layout.dial, s_width, s_height );
    lv_obj_set_clickable( layout.dial, false );
    lv_obj_add_event_cb( layout.dial, onDrawDial, LV_EVENT_DRAW_MAIN, this );
    layout.skyMoon = createCanvas( layout.root, s_dialMoonSize );
    layout.date = label( layout.root, &jetbrains_mono_26, s_dialGrey, Centre, 205, 29 );
    layout.hijri = label( layout.root, &jetbrains_mono_26, s_dialGrey, Centre, 205, 459 );
    // The day name along the top-left corner, turned 45 degrees.
    layout.dayName = label( layout.root, &jetbrains_mono_26, s_dialGrey, Centre, 62, 62 );
    lv_obj_set_style_transform_rotation( layout.dayName, -450, 0 );
    lv_obj_set_style_transform_pivot_x( layout.dayName, lv_pct( 50 ), 0 );
    lv_obj_set_style_transform_pivot_y( layout.dayName, lv_pct( 50 ), 0 );
}

static void drawLine( lv_layer_t *layer, lv_color_t colour, int width, float x0, float y0, float x1, float y1 )
{
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init( &line );
    line.color = colour;
    line.width = width;
    line.p1.x = static_cast<lv_value_precise_t>( lroundf( x0 ) );
    line.p1.y = static_cast<lv_value_precise_t>( lroundf( y0 ) );
    line.p2.x = static_cast<lv_value_precise_t>( lroundf( x1 ) );
    line.p2.y = static_cast<lv_value_precise_t>( lroundf( y1 ) );
    lv_draw_line( layer, &line );
}

// A radial line from `inner` to `outer` at `angle` (degrees, clockwise from 3 o'clock).
static void drawSpoke( lv_layer_t *layer, lv_color_t colour, int width, int cx, int cy, float angle, float inner,
                       float outer )
{
    const float a = angle * static_cast<float>( M_PI ) / 180.0f;
    drawLine( layer, colour, width, cx + inner * cosf( a ), cy + inner * sinf( a ), cx + outer * cosf( a ),
              cy + outer * sinf( a ) );
}

static void drawOrbitArc( lv_layer_t *layer, lv_color_t colour, int cx, int cy, int radius, float from, float to )
{
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init( &arc );
    arc.color = colour;
    arc.width = 2;
    arc.center.x = cx;
    arc.center.y = cy;
    arc.radius = radius;
    arc.start_angle = static_cast<lv_value_precise_t>( fmodf( from + 720, 360 ) );
    arc.end_angle = static_cast<lv_value_precise_t>( fmodf( to + 720, 360 ) );
    lv_draw_arc( layer, &arc );
}

void WatchFace::onDrawDial( lv_event_t *event )
{
    WatchFace *self = static_cast<WatchFace *>( lv_event_get_user_data( event ) );
    const Dial &dial = self->m_dial;
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t area = {};
    lv_obj_get_coords( self->m_layouts[3].dial, &area );
    const int cx = area.x1 + s_dialX;
    const int cy = area.y1 + s_dialY;
    // The orbits: bright over the horizon, dim under it, with marks at rise and set.
    const float sunRise = dial.sunKnown ? dial.sunRise : 180;
    const float sunSet = dial.sunKnown ? dial.sunSet : 360;
    drawOrbitArc( layer, s_sunPath, cx, cy, s_dialSunRadius, sunRise, sunSet );
    drawOrbitArc( layer, s_sunPathDim, cx, cy, s_dialSunRadius, sunSet, sunRise + 360 );
    const float moonRise = dial.moonKnown ? dial.moonRise : 180;
    const float moonSet = dial.moonKnown ? dial.moonSet : 360;
    drawOrbitArc( layer, s_moonPath, cx, cy, s_dialMoonRadius, moonRise, moonSet );
    drawOrbitArc( layer, s_moonPathDim, cx, cy, s_dialMoonRadius, moonSet, moonRise + 360 );
    for ( int i = 0; i < 2; ++i ) {
        if ( dial.sunKnown ) {
            drawSpoke( layer, s_sunPath, 2, cx, cy, i == 0 ? sunRise : sunSet, s_dialSunRadius - 6, s_dialSunRadius + 6 );
        }
        if ( dial.moonKnown ) {
            drawSpoke( layer, s_moonPath, 2, cx, cy, i == 0 ? moonRise : moonSet, s_dialMoonRadius - 6,
                       s_dialMoonRadius + 6 );
        }
    }
    // Ticks: the quarters long, the hours medium, the minutes short.
    for ( int i = 0; i < 60; ++i ) {
        if ( i % 15 == 0 ) {
            drawSpoke( layer, lv_color_white(), 4, cx, cy, i * 6.0f - 90, 100, 122 );
        } else if ( i % 5 == 0 ) {
            drawSpoke( layer, lv_color_make( 0xC0, 0xC0, 0xC0 ), 3, cx, cy, i * 6.0f - 90, 108, 122 );
        } else {
            drawSpoke( layer, lv_color_make( 0x70, 0x70, 0x70 ), 1, cx, cy, i * 6.0f - 90, 116, 122 );
        }
    }
    // The Sun: bright above the horizon, dim below.
    if ( dial.sunKnown ) {
        const float a = dial.sunAngle * static_cast<float>( M_PI ) / 180.0f;
        lv_draw_rect_dsc_t disc;
        lv_draw_rect_dsc_init( &disc );
        disc.bg_color = dial.sunUp ? s_amber : lv_color_make( 0x60, 0x44, 0x1A );
        disc.radius = LV_RADIUS_CIRCLE;
        const int x = cx + static_cast<int>( lroundf( s_dialSunRadius * cosf( a ) ) );
        const int y = cy + static_cast<int>( lroundf( s_dialSunRadius * sinf( a ) ) );
        const lv_area_t box = { x - 13, y - 13, x + 13, y + 13 };
        lv_draw_rect( layer, &disc, &box );
    }
    // The hands: hour, minute, and the seconds through the centre.
    const float hour = ( ( dial.hour % 12 ) + dial.minute / 60.0f ) * 30 - 90;
    const float minute = ( dial.minute + dial.second / 60.0f ) * 6 - 90;
    const float second = dial.second * 6.0f - 90;
    drawSpoke( layer, lv_color_white(), 7, cx, cy, hour, 0, 58 );
    drawSpoke( layer, lv_color_white(), 5, cx, cy, minute, 0, 92 );
    drawSpoke( layer, s_amber, 2, cx, cy, second, -18, 112 );
    lv_draw_rect_dsc_t centre;
    lv_draw_rect_dsc_init( &centre );
    centre.bg_color = s_amber;
    centre.radius = LV_RADIUS_CIRCLE;
    const lv_area_t outer = { cx - 6, cy - 6, cx + 6, cy + 6 };
    lv_draw_rect( layer, &centre, &outer );
    centre.bg_color = lv_color_black();
    const lv_area_t inner = { cx - 2, cy - 2, cx + 2, cy + 2 };
    lv_draw_rect( layer, &centre, &inner );
}

void WatchFace::build( lv_obj_t *page )
{
    buildClassic( page );
    buildArc( page );
    buildSkyArcs( page );
    buildDial( page );
    m_moon = lv_canvas_create( page );
    lv_canvas_set_draw_buf( m_moon, lv_draw_buf_create( s_moonSize, s_moonSize, LV_COLOR_FORMAT_RGB565, 0 ) );
    lv_canvas_fill_bg( m_moon, lv_color_black(), LV_OPA_COVER );
    setStyle( 0, DateItem | SunItem | MoonItem );
}

static void setTranslateY( void *object, int32_t y )
{
    lv_obj_set_style_translate_y( static_cast<lv_obj_t *>( object ), y, 0 );
}

static void hideAfterSlide( lv_anim_t *animation )
{
    lv_obj_t *root = static_cast<lv_obj_t *>( lv_anim_get_user_data( animation ) );
    lv_obj_set_hidden( root, true );
    lv_obj_set_style_translate_y( root, 0, 0 );
}

// Moves `root` vertically from `from` to `to` pixels; `hide` hides it at the end.
static void slide( lv_obj_t *root, int from, int to, bool hide )
{
    lv_anim_t animation;
    lv_anim_init( &animation );
    lv_anim_set_var( &animation, root );
    lv_anim_set_user_data( &animation, root );
    lv_anim_set_exec_cb( &animation, setTranslateY );
    lv_anim_set_values( &animation, from, to );
    lv_anim_set_duration( &animation, s_slideMs );
    lv_anim_set_path_cb( &animation, lv_anim_path_ease_out );
    if ( hide ) {
        lv_anim_set_completed_cb( &animation, hideAfterSlide );
    }
    lv_anim_start( &animation );
}

void WatchFace::setStyle( int style, int items, int direction )
{
    const int previous = m_style;
    m_style = ( style >= 0 && style < StyleCount ) ? style : 0;
    m_items = items;
    for ( int i = 0; i < StyleCount; ++i ) {
        // A slide still running ends here, so that a quick second swipe starts from rest.
        lv_anim_delete( m_layouts[i].root, setTranslateY );
        lv_obj_set_style_translate_y( m_layouts[i].root, 0, 0 );
        lv_obj_set_hidden( m_layouts[i].root, i != m_style );
    }
    if ( direction != 0 && previous != m_style ) {
        lv_obj_set_hidden( m_layouts[previous].root, false );
        slide( m_layouts[previous].root, 0, -direction * s_height, true );
        slide( m_layouts[m_style].root, direction * s_height, 0, false );
    }
    const Layout &layout = m_layouts[m_style];
    lv_obj_set_parent( m_moon, layout.root );
    lv_obj_set_pos( m_moon, s_moonX, s_moonY );
    // The sky moons are drawn again for the layout shown.
    m_skyMoonState[0] = -1;
    applyItems();
}

void WatchFace::applyItems()
{
    const Layout &layout = m_layouts[m_style];
    const bool date = m_items & DateItem;
    const bool sun = m_items & SunItem;
    const bool moon = m_items & MoonItem;
    // The sky faces show the Moon on its path, not in the corner.
    lv_obj_t *const objects[8] = { layout.date,    layout.hijri, layout.sunName, layout.sunTime,
                                   layout.sunLeft, m_moon,       layout.dayName, layout.skyMoon };
    const bool shown[8] = { date, date, sun, sun, sun, moon && m_style < 2, date, moon };
    for ( int i = 0; i < 8; ++i ) {
        if ( objects[i] != nullptr ) {
            lv_obj_set_hidden( objects[i], !shown[i] );
        }
    }
}

void WatchFace::showTime( const Rtc::DateTime &time )
{
    const Layout &layout = m_layouts[m_style];
    if ( layout.time != nullptr ) {
        lv_label_set_text_fmt( layout.time, "%02u:%02u", time.hour, time.minute );
    }
    if ( layout.dial != nullptr ) {
        m_dial.hour = time.hour;
        m_dial.minute = time.minute;
        m_dial.second = time.second;
        lv_obj_invalidate( layout.dial );
    }
    if ( layout.seconds != nullptr ) {
        lv_label_set_text_fmt( layout.seconds, ":%02u", time.second );
    }
    if ( time.month < 1 || time.month > 12 || time.day < 1 || time.day > 31 ) {
        // No valid time (the RTC was not read): no name tables are indexed with it.
        lv_label_set_text( layout.date, "" );
        return;
    }
    // Language counts the week from Monday, weekday() from Sunday.
    const int day = ( weekday( time ) + 6 ) % 7;
    char date[48];
    if ( m_style == 0 ) {
        Language::longDate( date, sizeof( date ), day, time.day, time.month );
    } else if ( m_style == 1 ) {
        Language::shortDate( date, sizeof( date ), day, time.day, time.month, time.year );
    } else {
        // The sky faces: "21 June", the day name on its own.
        Language::longDate( date, sizeof( date ), -1, time.day, time.month );
        lv_label_set_text( layout.dayName, m_style == 2 ? Language::dayName( day ) : Language::shortDay( day ) );
    }
    lv_label_set_text( layout.date, date );
}

// The Sun and the Moon of the sky faces: on their paths (sky arcs) or orbits (dial), placed by
// the time between their rise and their set, and below the horizon on the dial.
void WatchFace::placeSky( const Sky::Day &day, int64_t time )
{
    const Layout &layout = m_layouts[m_style];
    const Corners::Input input = Corners::fromSky( day );
    const int percent = static_cast<int>( day.phase.illumination * 100 + 0.5 );
    const bool waxing = day.phase.elongation < 180;
    Corners::Pass sun = {};
    const bool sunKnown = Corners::pass( input.sun, time, sun );
    Corners::Pass moon = {};
    const bool moonKnown = Corners::pass( input.moon, time, moon );
    if ( m_style == 2 ) {
        const bool sunShown = sunKnown && sun.up;
        if ( sunShown ) {
            const float f = static_cast<float>( time - sun.upStart ) / static_cast<float>( sun.upEnd - sun.upStart );
            const float a = ( 180 + 180 * f ) * static_cast<float>( M_PI ) / 180.0f;
            lv_obj_set_pos( layout.sunDot, s_skyX + static_cast<int>( lroundf( s_skySunRadius * cosf( a ) ) ) - 12,
                            s_skyY + static_cast<int>( lroundf( s_skySunRadius * sinf( a ) ) ) - 12 );
        }
        lv_obj_set_hidden( layout.sunDot, !sunShown );
        const bool moonShown = moonKnown && moon.up && ( m_items & MoonItem );
        if ( moonShown ) {
            const float f = static_cast<float>( time - moon.upStart ) / static_cast<float>( moon.upEnd - moon.upStart );
            const float a = ( 180 + 180 * f ) * static_cast<float>( M_PI ) / 180.0f;
            lv_obj_set_pos( layout.skyMoon,
                            s_skyX + static_cast<int>( lroundf( s_skyMoonRadius * cosf( a ) ) ) - s_skyMoonSize / 2,
                            s_skyY + static_cast<int>( lroundf( s_skyMoonRadius * sinf( a ) ) ) - s_skyMoonSize / 2 );
            if ( m_skyMoonState[0] != percent || m_skyMoonState[1] != 1 ) {
                paintMoon( layout.skyMoon, s_skyMoonSize, percent, waxing, false, lv_color_make( 0x30, 0x30, 0x38 ) );
                m_skyMoonState[0] = percent;
                m_skyMoonState[1] = 1;
            }
        }
        lv_obj_set_hidden( layout.skyMoon, !moonShown );
        return;
    }
    // The dial: each orbit bright over the top for its share of the pass.
    Dial &dial = m_dial;
    const Corners::Pass *passes[2] = { &sun, &moon };
    const bool known[2] = { sunKnown, moonKnown };
    for ( int i = 0; i < 2; ++i ) {
        float rise = 180;
        float set = 360;
        float angle = 0;
        if ( known[i] ) {
            const Corners::Pass &pass = *passes[i];
            const float span = 360.0f * static_cast<float>( pass.upEnd - pass.upStart ) /
                               static_cast<float>( pass.downEnd - pass.upStart );
            rise = 270 - span / 2;
            set = 270 + span / 2;
            if ( pass.up ) {
                angle = rise + ( set - rise ) * static_cast<float>( time - pass.upStart ) /
                                   static_cast<float>( pass.upEnd - pass.upStart );
            } else {
                angle = set + ( rise + 360 - set ) * static_cast<float>( time - pass.upEnd ) /
                                  static_cast<float>( pass.downEnd - pass.upEnd );
            }
        }
        if ( i == 0 ) {
            dial.sunKnown = known[i];
            dial.sunUp = known[i] && sun.up;
            dial.sunRise = rise;
            dial.sunSet = set;
            dial.sunAngle = angle;
        } else {
            dial.moonKnown = known[i];
            dial.moonUp = known[i] && moon.up;
            dial.moonRise = rise;
            dial.moonSet = set;
            dial.moonAngle = angle;
        }
    }
    const bool moonShown = dial.moonKnown && ( m_items & MoonItem );
    if ( moonShown ) {
        const float a = dial.moonAngle * static_cast<float>( M_PI ) / 180.0f;
        lv_obj_set_pos( layout.skyMoon,
                        s_dialX + static_cast<int>( lroundf( s_dialMoonRadius * cosf( a ) ) ) - s_dialMoonSize / 2,
                        s_dialY + static_cast<int>( lroundf( s_dialMoonRadius * sinf( a ) ) ) - s_dialMoonSize / 2 );
        const int up = dial.moonUp ? 1 : 0;
        if ( m_skyMoonState[0] != percent || m_skyMoonState[1] != up ) {
            // Below the horizon dimmed; the unlit part lighter than on the other faces, so that
            // a thin moon still shows its place.
            paintMoon( layout.skyMoon, s_dialMoonSize, percent, waxing, !dial.moonUp, lv_color_make( 0x5C, 0x5C, 0x68 ) );
            m_skyMoonState[0] = percent;
            m_skyMoonState[1] = up;
        }
    }
    lv_obj_set_hidden( layout.skyMoon, !moonShown );
    lv_obj_invalidate( layout.dial );
}

void WatchFace::showSky( const Sky::Day &day, const Sky::Now &now, int utcOffsetMinutes )
{
    const int percent = static_cast<int>( day.phase.illumination * 100 + 0.5 );
    const bool waxing = day.phase.elongation < 180;
    if ( percent != m_moonPercent ) {
        paintMoon( m_moon, s_moonSize, percent, waxing, false, lv_color_make( 0x30, 0x30, 0x38 ) );
        m_moonPercent = percent;
    }
    char hijri[32];
    if ( m_style >= 2 ) {
        // The sky faces leave out the year (the Moon page shows it).
        snprintf( hijri, sizeof( hijri ), "%d %s", day.hijri.day, SkyPages::hijriMonthName( day.hijri.month ) );
    } else {
        snprintf( hijri, sizeof( hijri ), "%d %s %d", day.hijri.day, SkyPages::hijriMonthName( day.hijri.month ),
                  day.hijri.year );
    }
    char rise[8];
    char set[8];
    formatTime( rise, sizeof( rise ), day.sun.riseSet.rising, utcOffsetMinutes );
    formatTime( set, sizeof( set ), day.sun.riseSet.setting, utcOffsetMinutes );
    const char *eventName = Language::text( now.sunUp ? Language::Sunset : Language::Sunrise );
    const char *eventTime = now.sunUp ? set : rise;
    const int minutesLeft = ( now.secondsToSunEvent + 30 ) / 60;
    char left[16] = "";
    if ( now.secondsToSunEvent > 0 ) {
        snprintf( left, sizeof( left ), Language::text( Language::InTime ), minutesLeft / 60, minutesLeft % 60 );
    }

    const Layout &layout = m_layouts[m_style];
    lv_label_set_text( layout.hijri, hijri );
    if ( m_style >= 2 ) {
        placeSky( day, now.time );
    } else if ( m_style == 0 ) {
        lv_label_set_text( layout.sunName, eventName );
        lv_label_set_text( layout.sunTime, eventTime );
        lv_label_set_text( layout.sunLeft, left );
    } else {
        lv_label_set_text_fmt( layout.sunName, "%s %s", eventName, left );
        lv_label_set_text( layout.riseText, rise );
        lv_label_set_text( layout.setText, set );
        // The Sun on the arc by the time between sunrise and sunset; hidden at night.
        const Astronomy::Crossings &sun = day.sun.riseSet;
        const bool onArc = now.sunUp && sun.rising.occurs && sun.setting.occurs;
        if ( onArc ) {
            const double length = static_cast<double>( sun.setting.time - sun.rising.time );
            const double passed = length - now.secondsToSunEvent;
            const double angle = 3.14159265358979323846 * ( 1 - passed / length );
            lv_obj_set_pos( layout.sunDot, static_cast<int>( s_arcX + s_arcRadius * std::cos( angle ) ) - 12,
                            static_cast<int>( s_arcY - s_arcRadius * std::sin( angle ) ) - 12 );
        }
        lv_obj_set_hidden( layout.sunDot, !onArc );
    }
}
