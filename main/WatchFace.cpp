#include "WatchFace.h"

#include <cmath>
#include <cstdio>

#include "Fonts.h"
#include "SkyPages.h"

static const int s_width = 410;
static const int s_height = 502;
static const int s_moonSize = 52;
static const lv_color_t s_grey = LV_COLOR_MAKE( 0xA0, 0xA0, 0xA0 );
static const lv_color_t s_amber = LV_COLOR_MAKE( 0xFF, 0xB0, 0x40 );

static const char *s_dayNames[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *s_shortDays[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *s_monthNames[12] = { "January", "February", "March",     "April",   "May",      "June",
                                        "July",    "August",   "September", "October", "November", "December" };

static const uint32_t s_slideMs = 250;

// The day arc: centre and radius.
static const int s_arcX = 205;
static const int s_arcY = 200;
static const int s_arcRadius = 160;

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
    return ( days + 4 ) % 7;
}

void WatchFace::buildClassic( lv_obj_t *page )
{
    Layout &layout = m_layouts[0];
    layout.root = createRoot( page );
    layout.battery = label( layout.root, &jetbrains_mono_26, s_grey, Centre, 205, 40 );
    layout.time = label( layout.root, &jetbrains_mono_light_110, lv_color_white(), Centre, 205, 150 );
    layout.seconds = label( layout.root, &jetbrains_mono_26, s_grey, Centre, 205, 224 );
    layout.date = label( layout.root, &jetbrains_mono_26, lv_color_white(), Centre, 205, 280 );
    layout.hijri = label( layout.root, &jetbrains_mono_26, s_grey, Centre, 205, 316 );
    layout.moonX = 110 - s_moonSize / 2;
    layout.moonY = 410 - s_moonSize / 2;
    layout.moonText = label( layout.root, &jetbrains_mono_26, s_grey, Centre, 110, 460 );
    layout.sunName = label( layout.root, &jetbrains_mono_26, s_amber, Centre, 290, 396 );
    layout.sunTime = label( layout.root, &jetbrains_mono_26, lv_color_white(), Centre, 290, 430 );
    layout.sunLeft = label( layout.root, &jetbrains_mono_26, s_grey, Centre, 290, 464 );
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
    layout.moonX = 40;
    layout.moonY = 396 - s_moonSize / 2;
    layout.moonText = label( layout.root, &jetbrains_mono_26, s_grey, Left, 40 + s_moonSize + 12, 396 );
    layout.battery = label( layout.root, &jetbrains_mono_26, s_grey, Right, s_width - 40, 396 );
    layout.sunName = label( layout.root, &jetbrains_mono_26, s_amber, Centre, 205, 458 );
}

void WatchFace::buildStacked( lv_obj_t *page )
{
    Layout &layout = m_layouts[2];
    layout.root = createRoot( page );
    // `hijri` holds the day name here, over the date.
    layout.time = label( layout.root, &jetbrains_mono_thin_170, lv_color_white(), Centre, 205, 125 );
    layout.minutes = label( layout.root, &jetbrains_mono_thin_170, lv_color_white(), Centre, 205, 280 );
    layout.hijri = label( layout.root, &jetbrains_mono_26, s_grey, Left, 36, 410 );
    layout.date = label( layout.root, &jetbrains_mono_26, lv_color_white(), Left, 36, 444 );
    layout.moonX = 205 - s_moonSize / 2;
    layout.moonY = 427 - s_moonSize / 2;
    layout.sunTime = label( layout.root, &jetbrains_mono_26, s_amber, Right, s_width - 36, 410 );
    layout.battery = label( layout.root, &jetbrains_mono_26, s_grey, Right, s_width - 36, 444 );
}

void WatchFace::build( lv_obj_t *page )
{
    buildClassic( page );
    buildArc( page );
    buildStacked( page );
    m_moon = lv_canvas_create( page );
    lv_canvas_set_draw_buf( m_moon, lv_draw_buf_create( s_moonSize, s_moonSize, LV_COLOR_FORMAT_RGB565, 0 ) );
    lv_canvas_fill_bg( m_moon, lv_color_black(), LV_OPA_COVER );
    setStyle( 0, DateItem | SunItem | MoonItem | BatteryItem );
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
    lv_obj_set_pos( m_moon, layout.moonX, layout.moonY );
    applyItems();
}

void WatchFace::applyItems()
{
    const Layout &layout = m_layouts[m_style];
    const bool date = m_items & DateItem;
    const bool sun = m_items & SunItem;
    const bool moon = m_items & MoonItem;
    const bool battery = m_items & BatteryItem;
    lv_obj_t *const objects[9] = { layout.date, layout.hijri,    layout.sunName, layout.sunTime, layout.sunLeft,
                                   m_moon,      layout.moonText, layout.battery, nullptr };
    const bool shown[9] = { date, date, sun, sun, sun, moon, moon, battery, false };
    for ( int i = 0; i < 8; ++i ) {
        if ( objects[i] != nullptr ) {
            lv_obj_set_hidden( objects[i], !shown[i] );
        }
    }
}

void WatchFace::showTime( const Rtc::DateTime &time, int batteryPercent, bool usb )
{
    const Layout &layout = m_layouts[m_style];
    if ( layout.minutes != nullptr ) {
        lv_label_set_text_fmt( layout.time, "%02u", time.hour );
        lv_label_set_text_fmt( layout.minutes, "%02u", time.minute );
    } else {
        lv_label_set_text_fmt( layout.time, "%02u:%02u", time.hour, time.minute );
    }
    if ( layout.seconds != nullptr ) {
        lv_label_set_text_fmt( layout.seconds, ":%02u", time.second );
    }
    const int day = weekday( time );
    char month[4];
    snprintf( month, sizeof( month ), "%.3s", s_monthNames[time.month - 1] );
    char date[40];
    if ( m_style == 0 ) {
        snprintf( date, sizeof( date ), "%s %u %s", s_dayNames[day], time.day, s_monthNames[time.month - 1] );
    } else if ( m_style == 1 ) {
        snprintf( date, sizeof( date ), "%s %u %s %u", s_shortDays[day], time.day, month, time.year );
    } else {
        snprintf( date, sizeof( date ), "%u %s", time.day, month );
        // Over the date: the day name (this layout has no Hijri line).
        lv_label_set_text( layout.hijri, s_shortDays[day] );
    }
    lv_label_set_text( layout.date, date );
    lv_label_set_text_fmt( layout.battery, "%d %%%s", batteryPercent, usb ? "+" : "" );
}

void WatchFace::drawMoon( int percent, bool waxing )
{
    // The lit part in pale white, the dark part dim, the terminator half an ellipse; 4 x 4
    // samples per pixel smooth the edges.
    const lv_color_t lit = lv_color_make( 0xE6, 0xE8, 0xF0 );
    const lv_color_t dark = lv_color_make( 0x30, 0x30, 0x38 );
    const double radius = s_moonSize / 2.0 - 1;
    const double centre = s_moonSize / 2.0;
    const double fraction = percent / 100.0;
    for ( int y = 0; y < s_moonSize; ++y ) {
        for ( int x = 0; x < s_moonSize; ++x ) {
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
            lv_canvas_set_px( m_moon, x, y, lv_color_mix( dark, colour, static_cast<uint8_t>( darkSamples * 255 / 16 ) ),
                              LV_OPA_COVER );
        }
    }
}

void WatchFace::showSky( const Sky::Day &day, const Sky::Now &now, int utcOffsetMinutes )
{
    const int percent = static_cast<int>( day.phase.illumination * 100 + 0.5 );
    const bool waxing = day.phase.elongation < 180;
    if ( percent != m_moonPercent ) {
        drawMoon( percent, waxing );
        m_moonPercent = percent;
    }
    char hijri[32];
    snprintf( hijri, sizeof( hijri ), "%d %s %d", day.hijri.day, SkyPages::hijriMonthName( day.hijri.month ),
              day.hijri.year );
    char rise[8];
    char set[8];
    formatTime( rise, sizeof( rise ), day.sun.riseSet.rising, utcOffsetMinutes );
    formatTime( set, sizeof( set ), day.sun.riseSet.setting, utcOffsetMinutes );
    const char *eventName = now.sunUp ? "Sunset" : "Sunrise";
    const char *eventTime = now.sunUp ? set : rise;
    const int minutesLeft = ( now.secondsToSunEvent + 30 ) / 60;
    char left[16] = "";
    if ( now.secondsToSunEvent > 0 ) {
        snprintf( left, sizeof( left ), "in %d:%02d", minutesLeft / 60, minutesLeft % 60 );
    }

    const Layout &layout = m_layouts[m_style];
    if ( m_style != 2 ) {
        lv_label_set_text( layout.hijri, hijri );
    }
    if ( layout.moonText != nullptr ) {
        lv_label_set_text_fmt( layout.moonText, "%d %%", percent );
    }
    if ( m_style == 0 ) {
        lv_label_set_text( layout.sunName, eventName );
        lv_label_set_text( layout.sunTime, eventTime );
        lv_label_set_text( layout.sunLeft, left );
    } else if ( m_style == 1 ) {
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
    } else {
        lv_label_set_text_fmt( layout.sunTime, "%s %s", now.sunUp ? "Set" : "Rise", eventTime );
    }
}
