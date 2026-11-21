#include "SkyParts.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Dates.h"
#include "Fonts.h"
#include "Language.h"
#include "Styles.h"
#include "esp_attr.h"
// The layer's clip area: the list draws only the rows the band shows.
#include "lvgl_private.h"

static const double s_pi = 3.14159265358979323846;
static const lv_color_t s_grey = LV_COLOR_MAKE( 0xA0, 0xA0, 0xA0 );
static const lv_color_t s_dim = LV_COLOR_MAKE( 0x5A, 0x5A, 0x5A );
static const lv_color_t s_faint = LV_COLOR_MAKE( 0x38, 0x38, 0x38 );

// The polar scope's view, turned by half a turn by default; kept over the always-on wakes.
RTC_DATA_ATTR static bool s_inverted = true;

// Reticle (part coordinates): its centre and the ring the star sits on.
static const int s_reticleX = 205;
static const int s_reticleY = 200;
static const int s_ringPx = 116;
static const int s_numberPx = 92;
static const int s_tapTop = 40;
static const int s_tapHeight = 400;
// The night's bar: 16:00 to 08:00 local over 300 px.
static const int s_barX = 50;
static const int s_barY = 96;
static const int s_barWidth = 300;
static const int s_barHeight = 30;
static const int s_barStartHour = 16;
static const int s_barHours = 16;
// The Milky Way computation waits until the part has slid in.
static const int s_milkyDelayMs = 400;
static const int s_searchNights = 120;
// Deep-sky list: under the title, rows of two lines.
static const int s_listTop = 80;
static const int s_listHeight = 378;
static const int s_rowX = 40;
static const int s_rowWidth = 330;
static const int s_rowPitch = 80;
static const int s_rowHeight = 72;
static const int s_rowPad = 12;
// A drag this long past the list's end steps the part.
static const int s_endSwipePx = 60;

static double toRadians( double degrees )
{
    return degrees * s_pi / 180.0;
}

static lv_obj_t *label( lv_obj_t *parent, lv_color_t colour, int y )
{
    lv_obj_t *object = lv_label_create( parent );
    Styles::text( object, &jetbrains_mono_26, colour );
    lv_label_set_text( object, "" );
    lv_obj_align( object, LV_ALIGN_TOP_MID, 0, y );
    lv_obj_set_clickable( object, false );
    return object;
}

static void setText( lv_obj_t *object, const char *text )
{
    if ( object != nullptr && strcmp( lv_label_get_text( object ), text ) != 0 ) {
        lv_label_set_text( object, text );
    }
}

static lv_obj_t *drawing( lv_obj_t *part, int x, int y, int w, int h, lv_event_cb_t onDraw, void *user )
{
    lv_obj_t *object = lv_obj_create( part );
    lv_obj_remove_style_all( object );
    lv_obj_set_pos( object, x, y );
    lv_obj_set_size( object, w, h );
    lv_obj_set_scrollable( object, false );
    lv_obj_set_clickable( object, false );
    lv_obj_add_event_cb( object, onDraw, LV_EVENT_DRAW_POST, user );
    return object;
}

static void drawLine( lv_layer_t *layer, lv_draw_line_dsc_t &line, int x1, int y1, int x2, int y2 )
{
    line.p1.x = x1;
    line.p1.y = y1;
    line.p2.x = x2;
    line.p2.y = y2;
    lv_draw_line( layer, &line );
}

static void drawText( lv_layer_t *layer, lv_draw_label_dsc_t &text, int x1, int y1, int x2, const char *value )
{
    // The text is a local buffer: LVGL copies it for the draw task.
    text.text = value;
    text.text_local = 1;
    const lv_area_t area = { x1, y1, x2, y1 + jetbrains_mono_26.line_height - 1 };
    lv_draw_label( layer, &text, &area );
}

void SkyParts::build( lv_obj_t *polar, lv_obj_t *milkyWay, lv_obj_t *deepSky, StackPage &stack )
{
    m_stack = &stack;
    m_minute = -1;

    // Polar alignment: a tap anywhere below the page name flips the view.
    m_reticle = drawing( polar, s_reticleX - s_ringPx - 14, s_reticleY - s_ringPx - 14, 2 * s_ringPx + 28,
                         2 * s_ringPx + 28, onDrawReticle, this );
    lv_obj_t *tap = lv_obj_create( polar );
    lv_obj_remove_style_all( tap );
    lv_obj_set_pos( tap, 0, s_tapTop );
    lv_obj_set_size( tap, LV_PCT( 100 ), s_tapHeight );
    lv_obj_set_scrollable( tap, false );
    lv_obj_add_event_cb( tap, onPolarTap, LV_EVENT_CLICKED, this );
    m_poleRows[0] = label( polar, lv_color_white(), 336 );
    m_poleRows[1] = label( polar, s_grey, 370 );
    m_poleRows[2] = label( polar, s_dim, 404 );

    // Milky Way window.
    m_milkyTitle = label( milkyWay, s_grey, 44 );
    lv_label_set_text_static( m_milkyTitle, Language::text( Language::MilkyWayTitle ) );
    m_nightBar = drawing( milkyWay, s_barX, s_barY, s_barWidth + 10, s_barHeight + 40, onDrawNightBar, this );
    static const int rowY[4] = { 180, 216, 252, 300 };
    for ( int i = 0; i < 4; ++i ) {
        m_milkyRows[i] = label( milkyWay, i == 0 ? lv_color_white() : ( i == 3 ? lv_color_hex( Styles::Amber ) : s_grey ),
                                rowY[i] );
    }
    m_milkyTimer = lv_timer_create( onMilkyWayTimer, s_milkyDelayMs, this );
    lv_timer_pause( m_milkyTimer );

    // Deep-sky finder: a scrolling container holding one drawn object as tall as the rows.
    m_entries = static_cast<Messier::Entry *>( lv_malloc( sizeof( Messier::Entry ) * Messier::Count ) );
    m_entryCount = 0;
    m_listMinute = -1;
    m_deepTitle = label( deepSky, s_grey, 44 );
    m_deepNone = label( deepSky, s_grey, 200 );
    m_list = lv_obj_create( deepSky );
    lv_obj_remove_style_all( m_list );
    lv_obj_set_pos( m_list, 0, s_listTop );
    lv_obj_set_size( m_list, LV_PCT( 100 ), s_listHeight );
    lv_obj_set_scroll_dir( m_list, LV_DIR_VER );
    lv_obj_set_scrollbar_mode( m_list, LV_SCROLLBAR_MODE_OFF );
    // No give at the ends: a drag there steps the part instead (onListEvent).
    lv_obj_set_scroll_elastic( m_list, false );
    lv_obj_add_event_cb( m_list, onListEvent, LV_EVENT_PRESSED, this );
    lv_obj_add_event_cb( m_list, onListEvent, LV_EVENT_RELEASED, this );
    m_listContent = drawing( m_list, s_rowX, 0, s_rowWidth, s_rowPitch, onDrawList, this );
}

void SkyParts::forget()
{
    if ( m_milkyTimer != nullptr ) {
        lv_timer_delete( m_milkyTimer );
        m_milkyTimer = nullptr;
    }
    if ( m_entries != nullptr ) {
        lv_free( m_entries );
        m_entries = nullptr;
    }
    m_stack = nullptr;
    m_shown = -1;
    m_reticle = nullptr;
    for ( int i = 0; i < 3; ++i ) {
        m_poleRows[i] = nullptr;
    }
    m_milkyTitle = nullptr;
    m_nightBar = nullptr;
    for ( int i = 0; i < 4; ++i ) {
        m_milkyRows[i] = nullptr;
    }
    m_deepTitle = nullptr;
    m_deepNone = nullptr;
    m_list = nullptr;
    m_listContent = nullptr;
    m_entryCount = 0;
}

void SkyParts::setShown( int part )
{
    const bool changed = part != m_shown;
    m_shown = part;
    if ( !changed || m_reticle == nullptr ) {
        return;
    }
    if ( part == MilkyWayShown ) {
        showMilkyWay();
    } else if ( part == DeepSkyShown ) {
        m_listMinute = -1;
        showDeepSky();
    }
}

void SkyParts::show( int64_t now, const Astronomy::Observer &observer, int utcOffsetMinutes, bool located )
{
    m_now = now;
    m_observer = observer;
    m_utcOffsetMinutes = utcOffsetMinutes;
    m_located = located;
    if ( m_reticle == nullptr || now / 60 == m_minute ) {
        return;
    }
    m_minute = now / 60;
    showPolar();
    if ( m_shown == MilkyWayShown ) {
        showMilkyWay();
    } else if ( m_shown == DeepSkyShown ) {
        showDeepSky();
    }
}

int64_t SkyParts::tonightNoon() const
{
    const int64_t offset = static_cast<int64_t>( m_utcOffsetMinutes ) * 60;
    const int64_t local = m_now + offset;
    const int64_t noon = local / 86400 * 86400 + 43200 - offset;
    if ( m_now >= noon ) {
        return noon;
    }
    const Observing::Night last = Observing::night( noon - 86400, m_observer );
    return last.dark && m_now < last.dawn ? noon - 86400 : noon;
}

int SkyParts::localMinute( int64_t time ) const
{
    const int64_t local = time + static_cast<int64_t>( m_utcOffsetMinutes ) * 60;
    return static_cast<int>( ( local / 60 ) % 1440 );
}

// ---- Polar alignment ----

void SkyParts::showPolar()
{
    m_south = m_observer.latitude < 0;
    m_poleHourAngle = Observing::poleStarHourAngle( m_south, m_now, m_observer.longitude );
    m_poleMinutes = Observing::reticleMinutes( m_poleHourAngle, m_south, s_inverted );
    lv_obj_invalidate( m_reticle );
    char text[48];
    if ( !m_located ) {
        setText( m_poleRows[0], Language::text( Language::NoLocation ) );
        setText( m_poleRows[1], "" );
    } else {
        // The clock as it reads: 12 at the top, not 0.
        const int hour = m_poleMinutes / 60 == 0 ? 12 : m_poleMinutes / 60;
        snprintf( text, sizeof( text ), Language::text( m_south ? Language::SigmaOctAt : Language::PoleStarAt ), hour,
                  m_poleMinutes % 60 );
        setText( m_poleRows[0], text );
        const int minutes = static_cast<int>( std::lround( m_poleHourAngle * 4.0 ) ) % 1440;
        snprintf( text, sizeof( text ), Language::text( Language::HourAngleValue ), minutes / 60, minutes % 60 );
        setText( m_poleRows[1], text );
    }
    setText( m_poleRows[2], Language::text( s_inverted ? Language::ViewInverted : Language::ViewUpright ) );
}

void SkyParts::onPolarTap( lv_event_t *event )
{
    SkyParts *self = static_cast<SkyParts *>( lv_event_get_user_data( event ) );
    // Not after a swipe that ended on it.
    lv_indev_t *indev = lv_indev_active();
    if ( indev != nullptr && lv_indev_get_gesture_dir( indev ) != LV_DIR_NONE ) {
        return;
    }
    s_inverted = !s_inverted;
    self->showPolar();
}

void SkyParts::onDrawReticle( lv_event_t *event )
{
    SkyParts *self = static_cast<SkyParts *>( lv_event_get_user_data( event ) );
    lv_obj_t *object = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    lv_area_t area = {};
    lv_obj_get_coords( object, &area );
    const int cx = ( area.x1 + area.x2 + 1 ) / 2;
    const int cy = ( area.y1 + area.y2 + 1 ) / 2;
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init( &arc );
    arc.center.x = cx;
    arc.center.y = cy;
    arc.start_angle = 0;
    arc.end_angle = 360;
    arc.width = 2;
    arc.color = s_dim;
    arc.radius = s_ringPx;
    lv_draw_arc( layer, &arc );
    arc.width = 1;
    arc.color = s_faint;
    arc.radius = 28;
    lv_draw_arc( layer, &arc );
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init( &line );
    line.color = s_faint;
    line.width = 1;
    drawLine( layer, line, cx - 40, cy, cx + 40, cy );
    drawLine( layer, line, cx, cy - 40, cx, cy + 40 );
    // Ticks every half hour of the clock, longer at the hours and at 12, 3, 6 and 9.
    lv_draw_label_dsc_t text;
    lv_draw_label_dsc_init( &text );
    text.font = &jetbrains_mono_26;
    text.color = s_grey;
    text.align = LV_TEXT_ALIGN_CENTER;
    static const char *numbers[12] = { "12", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11" };
    line.color = s_dim;
    line.width = 2;
    for ( int i = 0; i < 24; ++i ) {
        const double a = toRadians( i * 15.0 );
        const int inner = s_ringPx - ( i % 6 == 0 ? 14 : ( i % 2 == 0 ? 9 : 5 ) );
        drawLine( layer, line, cx + static_cast<int>( std::lround( inner * std::sin( a ) ) ),
                  cy - static_cast<int>( std::lround( inner * std::cos( a ) ) ),
                  cx + static_cast<int>( std::lround( s_ringPx * std::sin( a ) ) ),
                  cy - static_cast<int>( std::lround( s_ringPx * std::cos( a ) ) ) );
        if ( i % 2 == 0 ) {
            const int x = cx + static_cast<int>( std::lround( s_numberPx * std::sin( a ) ) );
            const int y = cy - static_cast<int>( std::lround( s_numberPx * std::cos( a ) ) );
            text.text = numbers[i / 2];
            const lv_area_t box = { x - 20, y - 16, x + 20, y + 15 };
            lv_draw_label( layer, &text, &box );
        }
    }
    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init( &dot );
    dot.radius = LV_RADIUS_CIRCLE;
    dot.bg_color = s_grey;
    lv_area_t centre = { cx - 3, cy - 3, cx + 3, cy + 3 };
    lv_draw_rect( layer, &dot, &centre );
    if ( !self->m_located ) {
        return;
    }
    const double a = toRadians( self->m_poleMinutes / 2.0 );
    const int x = cx + static_cast<int>( std::lround( s_ringPx * std::sin( a ) ) );
    const int y = cy - static_cast<int>( std::lround( s_ringPx * std::cos( a ) ) );
    dot.bg_color = lv_color_hex( Styles::Amber );
    const lv_area_t star = { x - 9, y - 9, x + 9, y + 9 };
    lv_draw_rect( layer, &dot, &star );
}

// ---- Milky Way window ----

void SkyParts::onMilkyWayTimer( lv_timer_t *timer )
{
    SkyParts *self = static_cast<SkyParts *>( lv_timer_get_user_data( timer ) );
    lv_timer_pause( timer );
    self->computeMilkyWay();
    self->showMilkyWay();
}

void SkyParts::computeMilkyWay()
{
    m_milkyNoon = tonightNoon();
    m_milkyObserver = m_observer;
    m_milky = Observing::milkyWay( m_milkyNoon, m_observer );
    m_hasNext = Observing::nextMilkyWayNight( m_milkyNoon, s_searchNights, m_observer, m_nextNoon );
    m_milkyKnown = true;
}

void SkyParts::showMilkyWay()
{
    if ( m_nightBar == nullptr ) {
        return;
    }
    for ( int i = 1; i < 4; ++i ) {
        setText( m_milkyRows[i], "" );
    }
    if ( !m_located ) {
        setText( m_milkyRows[0], Language::text( Language::NoLocation ) );
        m_milkyKnown = false;
        lv_obj_invalidate( m_nightBar );
        return;
    }
    const bool current = m_milkyKnown && m_milkyNoon == tonightNoon() &&
                         m_milkyObserver.latitude == m_observer.latitude && m_milkyObserver.longitude == m_observer.longitude;
    if ( !current ) {
        // A moment after the part slid in: the search can take a few tenths of a second.
        m_milkyKnown = false;
        setText( m_milkyRows[0], "..." );
        lv_obj_invalidate( m_nightBar );
        lv_timer_reset( m_milkyTimer );
        lv_timer_resume( m_milkyTimer );
        return;
    }
    lv_obj_invalidate( m_nightBar );
    char text[48];
    const Observing::MilkyWayNight &night = m_milky;
    const int percent = static_cast<int>( std::lround( night.moonIllumination * 100.0 ) );
    if ( !Observing::centreRises( m_observer.latitude ) ) {
        setText( m_milkyRows[0], Language::text( Language::CentreTooLow ) );
        return;
    }
    if ( !night.night.dark ) {
        setText( m_milkyRows[0], Language::text( Language::NotDarkEnough ) );
    } else if ( night.window ) {
        const int start = localMinute( night.start );
        const int end = localMinute( night.end );
        snprintf( text, sizeof( text ), Language::text( Language::WindowTonight ), start / 60, start % 60, end / 60,
                  end % 60 );
        setText( m_milkyRows[0], text );
        const int highest = localMinute( night.highest );
        snprintf( text, sizeof( text ), Language::text( Language::WindowHighest ), highest / 60, highest % 60,
                  static_cast<int>( std::lround( night.highestAltitude ) ) );
        setText( m_milkyRows[1], text );
    } else {
        setText( m_milkyRows[0], Language::text( Language::NoWindowTonight ) );
    }
    if ( !night.centreUp ) {
        snprintf( text, sizeof( text ), Language::text( Language::MoonValue ), percent );
    } else if ( night.moon == Observing::MoonSets || night.moon == Observing::MoonRises ) {
        const int minute = localMinute( night.moonTime );
        snprintf( text, sizeof( text ),
                  Language::text( night.moon == Observing::MoonSets ? Language::MoonSetsValue : Language::MoonRisesValue ),
                  percent, minute / 60, minute % 60 );
    } else {
        static const Language::Text states[3] = { Language::MoonFaintValue, Language::MoonDownValue,
                                                  Language::MoonUpValue };
        snprintf( text, sizeof( text ), Language::text( states[night.moon] ), percent );
    }
    setText( m_milkyRows[2], text );
    if ( m_hasNext ) {
        const int day = static_cast<int>( ( m_nextNoon + static_cast<int64_t>( m_utcOffsetMinutes ) * 60 ) / 86400 );
        int year = 0;
        int month = 0;
        int dayOfMonth = 0;
        Dates::civil( day, year, month, dayOfMonth );
        snprintf( text, sizeof( text ), Language::text( Language::NextWindow ), dayOfMonth, Language::shortMonth( month ) );
        setText( m_milkyRows[3], text );
    } else {
        setText( m_milkyRows[3], Language::text( Language::NoNextWindow ) );
    }
}

void SkyParts::onDrawNightBar( lv_event_t *event )
{
    SkyParts *self = static_cast<SkyParts *>( lv_event_get_user_data( event ) );
    lv_obj_t *object = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    lv_area_t area = {};
    lv_obj_get_coords( object, &area );
    lv_layer_t *layer = lv_event_get_layer( event );
    const int x0 = area.x1 + 5;
    const int y0 = area.y1;
    lv_draw_rect_dsc_t box;
    lv_draw_rect_dsc_init( &box );
    box.radius = 4;
    // The spans: the dark, the centre up in it, the window (brighter each).
    if ( self->m_milkyKnown && self->m_located ) {
        const Observing::MilkyWayNight &night = self->m_milky;
        const int64_t scaleStart = self->m_milkyNoon + ( s_barStartHour - 12 ) * 3600;
        const int64_t spans[3][2] = { { night.night.dusk, night.night.dawn },
                                      { night.upStart, night.upEnd },
                                      { night.start, night.end } };
        const bool shown[3] = { night.night.dark, night.centreUp, night.window };
        static const uint32_t colours[3] = { 0x1C1C44, 0x34346E, 0x6A6AD0 };
        for ( int i = 0; i < 3; ++i ) {
            if ( !shown[i] ) {
                continue;
            }
            long a = static_cast<long>( ( spans[i][0] - scaleStart ) * s_barWidth / ( s_barHours * 3600 ) );
            long b = static_cast<long>( ( spans[i][1] - scaleStart ) * s_barWidth / ( s_barHours * 3600 ) );
            a = a < 0 ? 0 : ( a > s_barWidth ? s_barWidth : a );
            b = b < 0 ? 0 : ( b > s_barWidth ? s_barWidth : b );
            if ( b <= a ) {
                continue;
            }
            box.bg_color = lv_color_hex( colours[i] );
            const lv_area_t span = { x0 + static_cast<int>( a ), y0, x0 + static_cast<int>( b ) - 1, y0 + s_barHeight - 1 };
            lv_draw_rect( layer, &box, &span );
        }
    }
    box.bg_opa = LV_OPA_TRANSP;
    box.border_color = s_dim;
    box.border_width = 1;
    const lv_area_t frame = { x0, y0, x0 + s_barWidth - 1, y0 + s_barHeight - 1 };
    lv_draw_rect( layer, &box, &frame );
    lv_draw_label_dsc_t text;
    lv_draw_label_dsc_init( &text );
    text.font = &jetbrains_mono_26;
    text.color = s_dim;
    text.align = LV_TEXT_ALIGN_CENTER;
    static const char *hours[5] = { "18", "21", "00", "03", "06" };
    for ( int i = 0; i < 5; ++i ) {
        const int x = x0 + ( 2 + 3 * i ) * s_barWidth / s_barHours;
        text.text = hours[i];
        const lv_area_t box2 = { x - 20, y0 + s_barHeight + 4, x + 20, y0 + s_barHeight + 4 + jetbrains_mono_26.line_height };
        lv_draw_label( layer, &text, &box2 );
    }
}

// ---- Deep-sky finder ----

void SkyParts::showDeepSky()
{
    if ( m_list == nullptr || m_entries == nullptr || m_listMinute == m_now / 60 ) {
        return;
    }
    m_listMinute = m_now / 60;
    m_entryCount = 0;
    if ( m_located ) {
        const Observing::Night night = Observing::night( tonightNoon(), m_observer );
        m_entryCount = Messier::visible( m_now, m_observer, night, 10.0, m_entries );
    }
    char text[40];
    snprintf( text, sizeof( text ), Language::text( Language::DeepSkyTitle ), m_entryCount );
    setText( m_deepTitle, m_located ? text : Language::text( Language::PartDeepSky ) );
    setText( m_deepNone, m_located ? ( m_entryCount == 0 ? Language::text( Language::DeepSkyNone ) : "" )
                                   : Language::text( Language::NoLocation ) );
    const int height = m_entryCount > 0 ? m_entryCount * s_rowPitch : 1;
    if ( lv_obj_get_height( m_listContent ) != height ) {
        lv_obj_set_height( m_listContent, height );
    }
    lv_obj_invalidate( m_listContent );
}

void SkyParts::onDrawList( lv_event_t *event )
{
    SkyParts *self = static_cast<SkyParts *>( lv_event_get_user_data( event ) );
    lv_obj_t *object = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    lv_area_t area = {};
    lv_obj_get_coords( object, &area );
    lv_layer_t *layer = lv_event_get_layer( event );
    const lv_area_t clip = layer->_clip_area;
    lv_draw_rect_dsc_t box;
    lv_draw_rect_dsc_init( &box );
    box.bg_opa = LV_OPA_TRANSP;
    box.border_color = lv_color_hex( 0x3A3A3A );
    box.border_width = 1;
    box.radius = 18;
    lv_draw_label_dsc_t text;
    lv_draw_label_dsc_init( &text );
    text.font = &jetbrains_mono_26;
    // Only the rows the band shows.
    int first = ( clip.y1 - area.y1 ) / s_rowPitch;
    first = first < 0 ? 0 : first;
    for ( int i = first; i < self->m_entryCount; ++i ) {
        const int top = area.y1 + i * s_rowPitch + 4;
        if ( top > clip.y2 ) {
            break;
        }
        const Messier::Entry &entry = self->m_entries[i];
        const lv_area_t row = { area.x1, top, area.x2, top + s_rowHeight - 1 };
        lv_draw_rect( layer, &box, &row );
        char value[40];
        const int number = entry.index + 1;
        snprintf( value, sizeof( value ), "M%d %s", number, Language::messierName( number ) );
        text.color = lv_color_white();
        text.align = LV_TEXT_ALIGN_LEFT;
        drawText( layer, text, area.x1 + s_rowPad, top + 4, area.x2 - s_rowPad, value );
        const int magnitude = Messier::Objects[entry.index].magnitude;
        snprintf( value, sizeof( value ), "%d.%d", magnitude / 10, magnitude % 10 );
        text.color = s_grey;
        text.align = LV_TEXT_ALIGN_RIGHT;
        drawText( layer, text, area.x1 + s_rowPad, top + 4, area.x2 - s_rowPad, value );
        if ( entry.bestMinute > 0 ) {
            const int minute = self->localMinute( static_cast<int64_t>( entry.bestMinute ) * 60 );
            snprintf( value, sizeof( value ), Language::text( Language::DeepSkyBest ), entry.altitude, minute / 60,
                      minute % 60 );
        } else {
            snprintf( value, sizeof( value ), "%d°", entry.altitude );
        }
        text.align = LV_TEXT_ALIGN_LEFT;
        drawText( layer, text, area.x1 + s_rowPad, top + 36, area.x2 - s_rowPad, value );
    }
}

void SkyParts::onListEvent( lv_event_t *event )
{
    SkyParts *self = static_cast<SkyParts *>( lv_event_get_user_data( event ) );
    lv_point_t point = {};
    lv_indev_t *indev = lv_indev_active();
    if ( indev != nullptr ) {
        lv_indev_get_point( indev, &point );
    }
    if ( lv_event_get_code( event ) == LV_EVENT_PRESSED ) {
        // A list that fits does not scroll: LVGL passes its swipes to the page as gestures.
        const int above = lv_obj_get_scroll_y( self->m_list );
        const int below = lv_obj_get_scroll_bottom( self->m_list );
        const bool scrolls = above + below > 0;
        self->m_pressX = point.x;
        self->m_pressY = point.y;
        self->m_pressAtTop = scrolls && above <= 0;
        self->m_pressAtBottom = scrolls && below <= 0;
        return;
    }
    const int dx = point.x - self->m_pressX;
    const int dy = point.y - self->m_pressY;
    if ( self->m_stack == nullptr || abs( dx ) > abs( dy ) ) {
        return;
    }
    if ( dy > s_endSwipePx && self->m_pressAtTop ) {
        self->m_stack->step( -1 );
    } else if ( dy < -s_endSwipePx && self->m_pressAtBottom ) {
        self->m_stack->step( 1 );
    }
}

bool SkyParts::press( const char *name )
{
    if ( m_reticle == nullptr ) {
        return false;
    }
    if ( strcmp( name, "flip" ) == 0 ) {
        s_inverted = !s_inverted;
        showPolar();
    } else if ( strncmp( name, "scroll ", 7 ) == 0 ) {
        lv_obj_scroll_to_y( m_list, atoi( name + 7 ), LV_ANIM_OFF );
    } else {
        return false;
    }
    return true;
}
