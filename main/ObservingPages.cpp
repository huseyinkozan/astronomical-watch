#include "ObservingPages.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Fonts.h"
#include "Language.h"
#include "Panel.h"
#include "Styles.h"
#include "esp_attr.h"
#include "esp_log.h"

static const char *TAG = "observing";

static const double s_pi = 3.14159265358979323846;
static const lv_color_t s_grey = LV_COLOR_MAKE( 0xA0, 0xA0, 0xA0 );
static const lv_color_t s_dim = LV_COLOR_MAKE( 0x5A, 0x5A, 0x5A );
static const lv_color_t s_faint = LV_COLOR_MAKE( 0x38, 0x38, 0x38 );
static const lv_color_t s_eclipticColour = LV_COLOR_MAKE( 0x90, 0x60, 0x20 );
static const int s_left = 52;
static const int s_right = 358;
// 25 a second: the alidade's readings.
static const int s_tickMs = 40;

enum AstroMode
{
    DialMode,
    SunMode,      // the alidade on the Sun: the local time
    PolarisMode,  // the alidade on Polaris: the latitude
    SundialMode,
    AnalemmaMode,
    ModeCount
};
static const Language::Text s_modeNames[ModeCount] = { Language::AstroDial, Language::AstroSun, Language::AstroPolaris,
                                                       Language::AstroSundial, Language::AstroAnalemma };
// Kept over the always-on wakes (lost at power-off).
RTC_DATA_ATTR static int s_mode = DialMode;

// Dial (part coordinates): the limit circle centred at x 205, y 246 with 146 px, inside the
// corner gauges and under the title.
static const int s_dialX = 205;
static const int s_dialY = 246;
static const int s_dialPx = 146;
static const int s_dialMinutesPerPx = 4;
static const int s_dialMaxMinutes = 24 * 60;
static const int s_axisPx = 20;
// Quadrant: its pivot and radius; the arc from the horizontal to the vertical.
static const int s_pivotX = 96;
static const int s_pivotY = 300;
static const int s_quadrantPx = 196;
// Sundial: the hour lines' foot and length, the labels a little further out.
static const int s_sundialX = 205;
static const int s_sundialY = 318;
static const int s_sundialPx = 150;
static const int s_labelPx = 172;
// Analemma: the equator's line and the scales.
static const int s_analemmaX = 205;
static const int s_analemmaY = 225;
static const double s_pxPerMinute = 6.0;
static const double s_pxPerDegree = 4.6;

static double toRadians( double degrees )
{
    return degrees * s_pi / 180.0;
}

static lv_obj_t *label( lv_obj_t *parent, const lv_font_t *font, lv_color_t colour, int y, const char *text )
{
    lv_obj_t *object = lv_label_create( parent );
    Styles::text( object, font, colour );
    lv_label_set_text( object, text );
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

// Sets the text colour only when it changed: a style set redraws the object even with the same
// value, and the level and the alidade update 25 times a second.
static void setColour( lv_obj_t *object, lv_color_t colour )
{
    if ( !lv_color_eq( lv_obj_get_style_text_color( object, LV_PART_MAIN ), colour ) ) {
        lv_obj_set_style_text_color( object, colour, 0 );
    }
}

// A drawing object at (`x`, `y`) of `part`, `w` x `h`, drawn by `onDraw` with `user`.
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

static void drawDot( lv_layer_t *layer, int x, int y, int radius, lv_color_t colour )
{
    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init( &dot );
    dot.bg_color = colour;
    dot.radius = LV_RADIUS_CIRCLE;
    lv_area_t area = { x - radius, y - radius, x + radius, y + radius };
    lv_draw_rect( layer, &dot, &area );
}

static lv_point_t centreOf( lv_obj_t *object )
{
    lv_area_t area = {};
    lv_obj_get_coords( object, &area );
    return { ( area.x1 + area.x2 + 1 ) / 2, ( area.y1 + area.y2 + 1 ) / 2 };
}

void ObservingPages::build( lv_obj_t *tile, Settings &settings, Motion *motion )
{
    m_tile = tile;
    m_settings = &settings;
    m_motion = motion;
    m_stack.build( tile, PartCount, Language::text( Language::ObservingPageName ) );
    lv_obj_t *part = m_stack.part( AstrolabePart );
    m_points = static_cast<Pixel *>( lv_malloc( sizeof( Pixel ) * ( PlateLines * LinePoints + AnalemmaPoints ) ) );
    m_plateLatitude = -999;
    m_analemmaYear = -1;

    m_mode = lv_obj_create( part );
    lv_obj_set_pos( m_mode, s_left, 36 );
    lv_obj_set_size( m_mode, s_right - s_left, 48 );
    lv_obj_set_scrollable( m_mode, false );
    lv_obj_set_press_lock( m_mode, false );
    Styles::button( m_mode );
    lv_obj_add_event_cb( m_mode, onEvent, LV_EVENT_CLICKED, this );
    m_modeLabel = label( m_mode, &jetbrains_mono_26, lv_color_white(), 0, "" );
    lv_obj_align( m_modeLabel, LV_ALIGN_CENTER, 0, 0 );

    // Dial: takes the drags itself (not passed to the tile; a vertical swipe still steps parts).
    m_dial = drawing( part, s_dialX - s_dialPx - 8, s_dialY - s_dialPx - 8, 2 * s_dialPx + 16, 2 * s_dialPx + 16,
                      onDrawDial, this );
    lv_obj_set_clickable( m_dial, true );
    const lv_event_code_t codes[5] = { LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST,
                                       LV_EVENT_CLICKED };
    for ( int i = 0; i < 5; ++i ) {
        lv_obj_add_event_cb( m_dial, onDialEvent, codes[i], this );
    }
    // Alidade: the quadrant, and a tap anywhere under the title holds the angle.
    m_quadrant = drawing( part, s_pivotX - 8, s_pivotY - s_quadrantPx - 8, s_quadrantPx + 16, s_quadrantPx + 16,
                          onDrawQuadrant, this );
    m_area = lv_obj_create( part );
    lv_obj_remove_style_all( m_area );
    lv_obj_set_pos( m_area, s_left, 92 );
    lv_obj_set_size( m_area, s_right - s_left, 300 );
    lv_obj_set_scrollable( m_area, false );
    lv_obj_add_event_cb( m_area, onEvent, LV_EVENT_PRESSED, this );
    lv_obj_add_event_cb( m_area, onEvent, LV_EVENT_CLICKED, this );
    m_angle = label( part, &jetbrains_mono_48, lv_color_white(), 308, "" );
    m_sundial = drawing( part, s_sundialX - s_labelPx - 20, s_sundialY - s_labelPx - 20, 2 * s_labelPx + 40,
                         s_labelPx + 40, onDrawSundial, this );
    static const char *hours[5] = { "6", "9", "12", "15", "18" };
    for ( int i = 0; i < 5; ++i ) {
        m_hourLabels[i] = label( part, &jetbrains_mono_26, s_grey, 0, hours[i] );
        // Centred on the point it is placed at.
        lv_obj_set_style_translate_x( m_hourLabels[i], lv_pct( -50 ), 0 );
        lv_obj_set_style_translate_y( m_hourLabels[i], lv_pct( -50 ), 0 );
    }
    m_analemma = drawing( part, s_analemmaX - 120, s_analemmaY - 120, 240, 240, onDrawAnalemma, this );
    for ( int i = 0; i < 3; ++i ) {
        m_rows[i] = label( part, &jetbrains_mono_26, lv_color_white(), 0, "" );
    }
    m_hint = label( part, &jetbrains_mono_26, s_dim, 0, "" );
    Styles::centred( m_hint );

    m_tick = lv_timer_create( onTick, s_tickMs, this );
    m_shownMinute = -1;
    showMode();

    m_sky.build( m_stack.part( PolarPart ), m_stack.part( MilkyWayPart ), m_stack.part( DeepSkyPart ), m_stack );
    m_optics.build( m_stack.part( EyepiecePart ), m_stack.part( PhotoPart ), settings );
}

void ObservingPages::forget()
{
    setShown( -1 );
    m_sky.forget();
    m_optics.forget();
    m_stack.forget();
    if ( m_tick != nullptr ) {
        lv_timer_delete( m_tick );
        m_tick = nullptr;
    }
    if ( m_points != nullptr ) {
        lv_free( m_points );
        m_points = nullptr;
    }
    m_tile = nullptr;
    m_mode = nullptr;
    m_modeLabel = nullptr;
    for ( int i = 0; i < 3; ++i ) {
        m_rows[i] = nullptr;
    }
    m_hint = nullptr;
    m_area = nullptr;
    m_dial = nullptr;
    m_quadrant = nullptr;
    m_angle = nullptr;
    m_sundial = nullptr;
    for ( int i = 0; i < 5; ++i ) {
        m_hourLabels[i] = nullptr;
    }
    m_analemma = nullptr;
    m_pressed = false;
    m_held = false;
}

StackPage &ObservingPages::stack()
{
    return m_stack;
}

bool ObservingPages::swipesSideways() const
{
    return m_dial != nullptr && s_mode == DialMode;
}

void ObservingPages::setShown( int part )
{
    if ( part != m_shownPart && ( part == EyepiecePart || part == PhotoPart ) ) {
        // The portal may have changed the equipment meanwhile.
        m_optics.refresh();
    }
    m_shownPart = part;
    m_sky.setShown( part >= PolarPart && part <= DeepSkyPart ? part - PolarPart : -1 );
    updateSensor();
}

void ObservingPages::updateSensor()
{
    const bool wanted = m_shownPart == AstrolabePart && m_mode != nullptr && ( s_mode == SunMode || s_mode == PolarisMode );
    if ( m_motion != nullptr && wanted != m_motion->streaming() ) {
        m_tilt.reset();
        m_motion->setStreaming( wanted );
    }
}

void ObservingPages::onTick( lv_timer_t *timer )
{
    ObservingPages *self = static_cast<ObservingPages *>( lv_timer_get_user_data( timer ) );
    if ( self->m_motion != nullptr && self->m_motion->streaming() ) {
        self->readTilt();
    }
}

void ObservingPages::readTilt()
{
    int16_t mg[3] = {};
    if ( m_motion->readLatest( mg ) != ESP_OK ) {
        return;
    }
    m_tilt.add( Tilt::fromSensor( mg[0], mg[1], mg[2], m_settings->value( Settings::Mirror ) ) );
    showAlidade();
}

void ObservingPages::show( int64_t now, const Astronomy::Observer &observer, int utcOffsetMinutes, bool located )
{
    m_now = now;
    m_utcOffsetMinutes = utcOffsetMinutes;
    m_located = located;
    const bool moved = observer.latitude != m_observer.latitude || observer.longitude != m_observer.longitude;
    m_observer = observer;
    if ( moved ) {
        m_starsPrecessed = false;
    }
    m_sky.show( now, observer, utcOffsetMinutes, located );
    if ( m_mode == nullptr || now / 60 == m_shownMinute ) {
        return;
    }
    m_shownMinute = now / 60;
    showMode();
}

void ObservingPages::stepMode()
{
    s_mode = ( s_mode + 1 ) % ModeCount;
    m_held = false;
    m_dragMinutes = 0;
    showMode();
    updateSensor();
    // Display asks again whether the page swipe stops (the dial's drags).
    lv_obj_send_event( m_tile, LV_EVENT_VALUE_CHANGED, nullptr );
    ESP_LOGI( TAG, "Astrolabe mode %d", s_mode );
}

void ObservingPages::onEvent( lv_event_t *event )
{
    ObservingPages *self = static_cast<ObservingPages *>( lv_event_get_user_data( event ) );
    lv_obj_t *target = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    if ( target == self->m_mode ) {
        self->stepMode();
        return;
    }
    if ( target != self->m_area ) {
        return;
    }
    if ( lv_event_get_code( event ) == LV_EVENT_PRESSED ) {
        // Before the tap shakes the watch.
        self->m_pressAltitude = Tilt::edgeAltitude( self->m_tilt.value() );
        return;
    }
    self->m_held = !self->m_held && self->m_tilt.valid();
    self->m_heldAltitude = self->m_pressAltitude;
    self->showAlidade();
}

// ---- Placing the modes ----

void ObservingPages::showMode()
{
    if ( m_mode == nullptr ) {
        return;
    }
    setText( m_modeLabel, Language::text( s_modeNames[s_mode] ) );
    const bool alidade = s_mode == SunMode || s_mode == PolarisMode;
    lv_obj_set_hidden( m_dial, s_mode != DialMode );
    lv_obj_set_hidden( m_quadrant, !alidade );
    lv_obj_set_hidden( m_area, !alidade );
    lv_obj_set_hidden( m_angle, !alidade );
    lv_obj_set_hidden( m_sundial, s_mode != SundialMode );
    for ( int i = 0; i < 5; ++i ) {
        lv_obj_set_hidden( m_hourLabels[i], s_mode != SundialMode );
    }
    lv_obj_set_hidden( m_analemma, s_mode != AnalemmaMode );
    // The rows under the drawing: their places per mode, -1 hidden.
    static const int rowY[ModeCount][4] = {
        { 398, -1, -1, 430 },   // dial: the time; the hint
        { 368, -1, -1, 402 },   // alidade: the answer; the hint (two lines)
        { 368, -1, -1, 402 },
        { 344, 376, -1, -1 },   // sundial: solar time, clock
        { 350, 382, 414, -1 },  // analemma: equation of time, declination, noon altitude
    };
    lv_obj_t *rows[4] = { m_rows[0], m_rows[1], m_rows[2], m_hint };
    for ( int i = 0; i < 4; ++i ) {
        lv_obj_set_hidden( rows[i], rowY[s_mode][i] < 0 );
        if ( rowY[s_mode][i] >= 0 ) {
            lv_obj_align( rows[i], LV_ALIGN_TOP_MID, 0, rowY[s_mode][i] );
        }
    }
    if ( s_mode == DialMode ) {
        showDial();
    } else if ( alidade ) {
        showAlidade();
    } else if ( s_mode == SundialMode ) {
        showSundial();
    } else {
        showAnalemma();
    }
}

// ---- Dial ----

static double dialScale()
{
    return s_dialPx / Astrolabe::limitRadius();
}

static ObservingPages::Pixel toPixel( double x, double y )
{
    const double scale = dialScale();
    return { static_cast<int16_t>( std::lround( x * scale ) ), static_cast<int16_t>( std::lround( -y * scale ) ) };
}

void ObservingPages::computePlate()
{
    const double latitude = m_observer.latitude;
    if ( m_points == nullptr || latitude == m_plateLatitude ) {
        return;
    }
    m_plateLatitude = latitude;
    const double limit = Astrolabe::limitRadius();
    for ( int i = 0; i < PlateLines; ++i ) {
        // The horizon, then the almucantars every 15 degrees.
        const Astrolabe::Circle circle = Astrolabe::almucantar( i * 15.0, latitude );
        double start = 0;
        double end = 0;
        m_lineCounts[i] = 0;
        if ( !Astrolabe::arcInside( circle, limit, start, end ) ) {
            continue;
        }
        // Points about 8 px apart along the arc, at most LinePoints.
        const double lengthPx = toRadians( end - start ) * circle.radius * dialScale();
        int count = static_cast<int>( lengthPx / 8.0 ) + 2;
        count = count > LinePoints ? LinePoints : ( count < 8 ? 8 : count );
        Pixel *points = m_points + i * LinePoints;
        for ( int k = 0; k < count; ++k ) {
            const double t = toRadians( start + ( end - start ) * k / ( count - 1 ) );
            points[k] = toPixel( circle.x + circle.radius * std::cos( t ), circle.y + circle.radius * std::sin( t ) );
        }
        m_lineCounts[i] = count;
    }
    const Astrolabe::Point zenith = Astrolabe::project( 0, latitude, latitude );
    m_zenith = toPixel( zenith.x, zenith.y );
}

void ObservingPages::computeRete()
{
    const int64_t time = m_now + m_dragMinutes * 60;
    const double latitude = m_observer.latitude;
    // Again for a new place's first time or a month later.
    if ( !m_starsPrecessed || std::llabs( m_now - m_precessedAt ) > 30 * 86400 ) {
        m_precessedAt = m_now;
        for ( int i = 0; i < Astrolabe::StarCount; ++i ) {
            Astronomy::precess( Astrolabe::Stars[i].raHours, Astrolabe::Stars[i].decDegrees, m_now, m_starRa[i], m_starDec[i] );
        }
        m_starsPrecessed = true;
    }
    const double sidereal = Astronomy::greenwichSiderealTime( time ) + m_observer.longitude;
    m_ecliptic = Astrolabe::ecliptic( sidereal, Astronomy::obliquityOfDate( time ), latitude );
    const double limit = Astrolabe::limitRadius();
    const double sinPhi = std::sin( toRadians( latitude ) );
    const double cosPhi = std::cos( toRadians( latitude ) );
    for ( int i = 0; i < Astrolabe::StarCount; ++i ) {
        const double h = sidereal - m_starRa[i];
        const Astrolabe::Point p = Astrolabe::project( h, m_starDec[i], latitude );
        m_starShown[i] = std::hypot( p.x, p.y ) <= limit;
        m_stars[i] = toPixel( p.x, p.y );
        const double sinAltitude = sinPhi * std::sin( toRadians( m_starDec[i] ) ) +
                                   cosPhi * std::cos( toRadians( m_starDec[i] ) ) * std::cos( toRadians( h ) );
        m_starUp[i] = sinAltitude > 0;
    }
    double ra = 0;
    double dec = 0;
    Astronomy::sunEquatorial( time, ra, dec );
    const Astrolabe::Point sun = Astrolabe::project( sidereal - ra, dec, latitude );
    m_sun = toPixel( sun.x, sun.y );
    m_sunUp = Astronomy::sunPosition( time, m_observer ).altitude > 0;
}

void ObservingPages::showDial()
{
    computePlate();
    computeRete();
    lv_obj_invalidate( m_dial );
    const int64_t local = m_now + m_dragMinutes * 60 + m_utcOffsetMinutes * 60;
    const int minuteOfDay = static_cast<int>( ( ( local / 60 ) % 1440 + 1440 ) % 1440 );
    char text[40];
    if ( m_dragMinutes == 0 ) {
        snprintf( text, sizeof( text ), "%02d:%02d", minuteOfDay / 60, minuteOfDay % 60 );
    } else {
        const int offset = abs( m_dragMinutes );
        snprintf( text, sizeof( text ), "%02d:%02d  %s%d:%02d", minuteOfDay / 60, minuteOfDay % 60,
                  m_dragMinutes > 0 ? "+" : "-", offset / 60, offset % 60 );
    }
    setText( m_rows[0], text );
    setColour( m_rows[0], m_dragMinutes == 0 ? lv_color_white() : lv_color_hex( Styles::Amber ) );
    setText( m_hint, Language::text( m_dragMinutes == 0 ? Language::DialHint : Language::DialNowHint ) );
}

void ObservingPages::onDrawDial( lv_event_t *event )
{
    ObservingPages *self = static_cast<ObservingPages *>( lv_event_get_user_data( event ) );
    lv_obj_t *object = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    if ( self->m_points == nullptr ) {
        return;
    }
    const lv_point_t c = centreOf( object );
    lv_layer_t *layer = lv_event_get_layer( event );
    const double scale = dialScale();
    // The circles centred on the pole: the limit, the equator and the near tropic.
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init( &arc );
    arc.center.x = c.x;
    arc.center.y = c.y;
    arc.start_angle = 0;
    arc.end_angle = 360;
    arc.width = 2;
    arc.color = s_dim;
    arc.radius = s_dialPx + 1;
    lv_draw_arc( layer, &arc );
    arc.width = 1;
    arc.color = s_faint;
    arc.radius = static_cast<int>( std::lround( scale ) );
    lv_draw_arc( layer, &arc );
    arc.radius = static_cast<int>( std::lround( scale * Astrolabe::declinationRadius( Astrolabe::PlateObliquity, 1 ) ) );
    lv_draw_arc( layer, &arc );

    // The plate: almucantars, then the horizon over them.
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init( &line );
    line.round_start = 1;
    line.round_end = 1;
    for ( int pass = 0; pass < 2; ++pass ) {
        for ( int i = pass == 0 ? 1 : 0; i < ( pass == 0 ? PlateLines : 1 ); ++i ) {
            line.color = i == 0 ? s_grey : s_dim;
            line.width = i == 0 ? 2 : 1;
            const Pixel *points = self->m_points + i * LinePoints;
            for ( int k = 1; k < self->m_lineCounts[i]; ++k ) {
                drawLine( layer, line, c.x + points[k - 1].x, c.y + points[k - 1].y, c.x + points[k].x, c.y + points[k].y );
            }
        }
    }
    line.color = s_grey;
    line.width = 2;
    const Pixel z = self->m_zenith;
    drawLine( layer, line, c.x + z.x - 5, c.y + z.y, c.x + z.x + 5, c.y + z.y );
    drawLine( layer, line, c.x + z.x, c.y + z.y - 5, c.x + z.x, c.y + z.y + 5 );

    // The rete: the ecliptic, the stars, the Sun.
    arc.width = 2;
    arc.color = s_eclipticColour;
    arc.center.x = c.x + static_cast<int>( std::lround( self->m_ecliptic.x * scale ) );
    arc.center.y = c.y - static_cast<int>( std::lround( self->m_ecliptic.y * scale ) );
    arc.radius = static_cast<int>( std::lround( self->m_ecliptic.radius * scale ) );
    lv_draw_arc( layer, &arc );
    for ( int i = 0; i < Astrolabe::StarCount; ++i ) {
        if ( !self->m_starShown[i] ) {
            continue;
        }
        const double magnitude = Astrolabe::Stars[i].magnitude;
        const int radius = magnitude < 0 ? 4 : ( magnitude < 1 ? 3 : 2 );
        drawDot( layer, c.x + self->m_stars[i].x, c.y + self->m_stars[i].y, radius,
                 self->m_starUp[i] ? lv_color_white() : s_dim );
    }
    drawDot( layer, c.x + self->m_sun.x, c.y + self->m_sun.y, 7,
             self->m_sunUp ? lv_color_hex( Styles::Amber ) : lv_color_hex( 0x805820 ) );
}

void ObservingPages::onDialEvent( lv_event_t *event )
{
    ObservingPages *self = static_cast<ObservingPages *>( lv_event_get_user_data( event ) );
    const lv_event_code_t code = lv_event_get_code( event );
    lv_point_t point = {};
    lv_indev_t *indev = lv_indev_active();
    if ( indev != nullptr ) {
        lv_indev_get_point( indev, &point );
    }
    if ( code == LV_EVENT_PRESSED ) {
        self->m_pressed = true;
        self->m_axisChosen = false;
        self->m_sideways = false;
        self->m_pressX = point.x;
        self->m_pressY = point.y;
        self->m_dragStart = self->m_dragMinutes;
        return;
    }
    if ( code == LV_EVENT_PRESSING && self->m_pressed ) {
        const int dx = point.x - self->m_pressX;
        const int dy = point.y - self->m_pressY;
        if ( !self->m_axisChosen ) {
            if ( abs( dx ) < s_axisPx && abs( dy ) < s_axisPx ) {
                return;
            }
            self->m_axisChosen = true;
            self->m_sideways = abs( dx ) > abs( dy );
        }
        if ( self->m_sideways ) {
            // To the right the time runs on (the sky turns clockwise).
            int minutes = self->m_dragStart + dx * s_dialMinutesPerPx;
            minutes = minutes > s_dialMaxMinutes ? s_dialMaxMinutes : ( minutes < -s_dialMaxMinutes ? -s_dialMaxMinutes : minutes );
            if ( minutes != self->m_dragMinutes ) {
                self->m_dragMinutes = minutes;
                self->showDial();
            }
        }
        return;
    }
    if ( code == LV_EVENT_CLICKED ) {
        // A tap without a drag: back to now.
        if ( !self->m_axisChosen && self->m_dragMinutes != 0 ) {
            self->m_dragMinutes = 0;
            self->showDial();
        }
        return;
    }
    // Released, or the press was lost.
    self->m_pressed = false;
}

// ---- Alidade ----

void ObservingPages::showAlidade()
{
    if ( m_angle == nullptr || ( s_mode != SunMode && s_mode != PolarisMode ) ) {
        return;
    }
    const bool valid = m_tilt.valid() || m_held;
    const double altitude = m_held ? m_heldAltitude : Tilt::edgeAltitude( m_tilt.value() );
    char text[48];
    if ( valid ) {
        snprintf( text, sizeof( text ), "%.1f°", std::round( altitude * 10.0 ) / 10.0 );
    } else {
        snprintf( text, sizeof( text ), "%s", "-" );
    }
    setText( m_angle, text );
    setColour( m_angle, m_held ? lv_color_hex( Styles::Amber ) : lv_color_white() );
    // The quadrant's line moves in steps of 0.2 degree: no redraw for the noise below.
    const double drawn = valid ? altitude : -1;
    if ( std::fabs( drawn - m_drawnAltitude ) >= 0.2 ) {
        m_drawnAltitude = drawn;
        lv_obj_invalidate( m_quadrant );
    }
    text[0] = '\0';
    if ( valid && s_mode == SunMode && m_located && m_now != 0 ) {
        int64_t time = 0;
        if ( Astrolabe::timeFromSunAltitude( altitude, m_now, m_observer, time ) ) {
            const int64_t local = time + m_utcOffsetMinutes * 60;
            const int minute = static_cast<int>( ( ( local / 60 ) % 1440 + 1440 ) % 1440 );
            snprintf( text, sizeof( text ), Language::text( Language::AlidadeTime ), minute / 60, minute % 60 );
        } else {
            snprintf( text, sizeof( text ), "%s", "-" );
        }
    } else if ( valid && s_mode == PolarisMode && m_now != 0 ) {
        double latitude = 0;
        if ( Astrolabe::latitudeFromPolaris( altitude, m_now, m_observer.longitude, latitude ) ) {
            snprintf( text, sizeof( text ), Language::text( Language::AlidadeLatitude ), latitude );
        } else {
            snprintf( text, sizeof( text ), "%s", "-" );
        }
    }
    setText( m_rows[0], text );
    setColour( m_rows[0], lv_color_hex( Styles::Amber ) );
    setText( m_hint, Language::text( m_held ? Language::SightHeld
                                            : ( s_mode == SunMode ? Language::SunAlidadeHint : Language::PolarisHint ) ) );
}

void ObservingPages::onDrawQuadrant( lv_event_t *event )
{
    ObservingPages *self = static_cast<ObservingPages *>( lv_event_get_user_data( event ) );
    lv_obj_t *object = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    lv_area_t area = {};
    lv_obj_get_coords( object, &area );
    // The pivot: 8 px inside the object's lower left corner.
    const int px = area.x1 + 8;
    const int py = area.y2 - 7;
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init( &arc );
    arc.center.x = px;
    arc.center.y = py;
    arc.radius = s_quadrantPx;
    arc.width = 2;
    arc.start_angle = 270;
    arc.end_angle = 360;
    arc.color = s_dim;
    lv_draw_arc( layer, &arc );
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init( &line );
    line.color = s_dim;
    line.width = 2;
    drawLine( layer, line, px, py, px + s_quadrantPx, py );
    drawLine( layer, line, px, py, px, py - s_quadrantPx );
    // Ticks every 10 degrees, longer every 30.
    for ( int degrees = 0; degrees <= 90; degrees += 10 ) {
        const double a = toRadians( degrees );
        const int inner = s_quadrantPx - ( degrees % 30 == 0 ? 24 : 12 );
        drawLine( layer, line, px + static_cast<int>( std::lround( inner * std::cos( a ) ) ),
                  py - static_cast<int>( std::lround( inner * std::sin( a ) ) ),
                  px + static_cast<int>( std::lround( s_quadrantPx * std::cos( a ) ) ),
                  py - static_cast<int>( std::lround( s_quadrantPx * std::sin( a ) ) ) );
    }
    if ( self->m_drawnAltitude < 0 ) {
        return;
    }
    const double a = toRadians( self->m_drawnAltitude );
    line.color = self->m_held ? lv_color_hex( Styles::Amber ) : lv_color_white();
    line.width = 4;
    line.round_start = 1;
    line.round_end = 1;
    drawLine( layer, line, px, py, px + static_cast<int>( std::lround( s_quadrantPx * std::cos( a ) ) ),
              py - static_cast<int>( std::lround( s_quadrantPx * std::sin( a ) ) ) );
}

// ---- Sundial ----

// The shadow's direction on the screen for an hour line: up is the noon line, the afternoon to
// the right north of the equator (looking north at a horizontal dial) and to the left south of it.
static void sundialPoint( double latitude, double hourAngle, int radius, int &x, int &y )
{
    const double a = toRadians( Astrolabe::sundialAngle( latitude, hourAngle ) );
    const int side = latitude < 0 ? -1 : 1;
    x = static_cast<int>( std::lround( side * radius * std::sin( a ) ) );
    y = static_cast<int>( std::lround( -radius * std::cos( a ) ) );
}

void ObservingPages::showSundial()
{
    double ra = 0;
    double dec = 0;
    Astronomy::sunEquatorial( m_now, ra, dec );
    m_sunHourAngle = Astrolabe::hourAngle( ra, m_now, m_observer.longitude );
    m_sunAltitude = Astronomy::sunPosition( m_now, m_observer ).altitude;
    for ( int i = 0; i < 5; ++i ) {
        int x = 0;
        int y = 0;
        sundialPoint( m_observer.latitude, -90.0 + 45.0 * i, s_labelPx, x, y );
        lv_obj_align( m_hourLabels[i], LV_ALIGN_TOP_LEFT, s_sundialX + x, s_sundialY + y );
    }
    lv_obj_invalidate( m_sundial );
    char text[48];
    if ( m_sunAltitude > 0 ) {
        // Apparent solar time: 12 h at the Sun's transit.
        const int minute = static_cast<int>( std::lround( 720.0 + m_sunHourAngle * 4.0 ) + 1440 ) % 1440;
        snprintf( text, sizeof( text ), Language::text( Language::SolarTimeValue ), minute / 60, minute % 60 );
    } else {
        snprintf( text, sizeof( text ), "%s", Language::text( Language::SunBelow ) );
    }
    setText( m_rows[0], text );
    setColour( m_rows[0], lv_color_hex( Styles::Amber ) );
    const int64_t local = m_now + m_utcOffsetMinutes * 60;
    const int minute = static_cast<int>( ( ( local / 60 ) % 1440 + 1440 ) % 1440 );
    snprintf( text, sizeof( text ), Language::text( Language::ClockTimeValue ), minute / 60, minute % 60 );
    setText( m_rows[1], text );
    setColour( m_rows[1], s_grey );
}

void ObservingPages::onDrawSundial( lv_event_t *event )
{
    ObservingPages *self = static_cast<ObservingPages *>( lv_event_get_user_data( event ) );
    lv_obj_t *object = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    lv_area_t area = {};
    lv_obj_get_coords( object, &area );
    // The foot of the hour lines: the object's middle, 20 px above its bottom.
    const int fx = ( area.x1 + area.x2 + 1 ) / 2;
    const int fy = area.y2 - 19;
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init( &line );
    line.round_start = 1;
    line.round_end = 1;
    for ( int hour = 6; hour <= 18; ++hour ) {
        int x = 0;
        int y = 0;
        sundialPoint( self->m_observer.latitude, ( hour - 12 ) * 15.0, s_sundialPx, x, y );
        line.color = hour == 12 ? s_grey : s_dim;
        line.width = hour % 3 == 0 ? 2 : 1;
        drawLine( layer, line, fx, fy, fx + x, fy + y );
    }
    if ( self->m_sunAltitude <= 0 ) {
        return;
    }
    int x = 0;
    int y = 0;
    sundialPoint( self->m_observer.latitude, self->m_sunHourAngle, s_sundialPx, x, y );
    line.color = lv_color_hex( Styles::Amber );
    line.width = 5;
    drawLine( layer, line, fx, fy, fx + x, fy + y );
    drawDot( layer, fx, fy, 5, lv_color_hex( Styles::Amber ) );
}

// ---- Analemma ----

void ObservingPages::computeAnalemma()
{
    // One curve a year: from January 1, every four days, at noon UTC.
    const int year = static_cast<int>( 1970 + m_now / 31556952 );
    if ( m_points == nullptr || year == m_analemmaYear ) {
        return;
    }
    m_analemmaYear = year;
    // January 1 of the year (days since 1970, Gregorian leap years).
    int64_t days = 0;
    for ( int y = 1970; y < year; ++y ) {
        days += ( y % 4 == 0 && ( y % 100 != 0 || y % 400 == 0 ) ) ? 366 : 365;
    }
    Pixel *points = m_points + PlateLines * LinePoints;
    for ( int i = 0; i < AnalemmaPoints; ++i ) {
        double declination = 0;
        double equation = 0;
        Astronomy::sunDeclinationAndEquation( ( days + i * 4 ) * 86400 + 43200, declination, equation );
        points[i] = { static_cast<int16_t>( std::lround( equation * s_pxPerMinute ) ),
                      static_cast<int16_t>( std::lround( -declination * s_pxPerDegree ) ) };
    }
}

void ObservingPages::showAnalemma()
{
    computeAnalemma();
    double declination = 0;
    double equation = 0;
    Astronomy::sunDeclinationAndEquation( m_now, declination, equation );
    m_today = { static_cast<int16_t>( std::lround( equation * s_pxPerMinute ) ),
                static_cast<int16_t>( std::lround( -declination * s_pxPerDegree ) ) };
    lv_obj_invalidate( m_analemma );
    char text[48];
    const int seconds = static_cast<int>( std::lround( std::fabs( equation ) * 60.0 ) );
    snprintf( text, sizeof( text ), Language::text( Language::EquationValue ), equation < 0 ? "-" : "+", seconds / 60,
              seconds % 60 );
    setText( m_rows[0], text );
    setColour( m_rows[0], lv_color_hex( Styles::Amber ) );
    snprintf( text, sizeof( text ), Language::text( Language::DeclinationValue ), declination );
    setText( m_rows[1], text );
    setColour( m_rows[1], lv_color_white() );
    // The Sun's altitude at its transit today.
    const double noon = 90.0 - std::fabs( m_observer.latitude - declination );
    snprintf( text, sizeof( text ), Language::text( Language::NoonAltitudeValue ), noon );
    setText( m_rows[2], text );
    setColour( m_rows[2], s_grey );
}

void ObservingPages::onDrawAnalemma( lv_event_t *event )
{
    ObservingPages *self = static_cast<ObservingPages *>( lv_event_get_user_data( event ) );
    lv_obj_t *object = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    if ( self->m_points == nullptr ) {
        return;
    }
    const lv_point_t c = centreOf( object );
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init( &line );
    line.color = s_faint;
    line.width = 1;
    // The equator and the line of a zero equation of time.
    drawLine( layer, line, c.x - 105, c.y, c.x + 105, c.y );
    drawLine( layer, line, c.x, c.y - 115, c.x, c.y + 115 );
    line.color = s_grey;
    line.width = 2;
    line.round_start = 1;
    line.round_end = 1;
    const Pixel *points = self->m_points + PlateLines * LinePoints;
    for ( int i = 1; i <= AnalemmaPoints; ++i ) {
        // Closed: the last point joins the first.
        const Pixel &a = points[i - 1];
        const Pixel &b = points[i % AnalemmaPoints];
        drawLine( layer, line, c.x + a.x, c.y + a.y, c.x + b.x, c.y + b.y );
    }
    drawDot( layer, c.x + self->m_today.x, c.y + self->m_today.y, 6, lv_color_hex( Styles::Amber ) );
}

// ---- Development ----

bool ObservingPages::press( const char *name )
{
    if ( m_mode == nullptr ) {
        return false;
    }
    if ( m_sky.press( name ) || m_optics.press( name ) ) {
        return true;
    }
    if ( strcmp( name, "mode" ) == 0 ) {
        stepMode();
    } else if ( strncmp( name, "drag ", 5 ) == 0 ) {
        m_dragMinutes = atoi( name + 5 );
        showDial();
    } else if ( strcmp( name, "now" ) == 0 ) {
        m_dragMinutes = 0;
        showDial();
    } else if ( strcmp( name, "hold" ) == 0 ) {
        m_held = !m_held;
        m_heldAltitude = Tilt::edgeAltitude( m_tilt.value() );
        showAlidade();
    } else {
        return false;
    }
    return true;
}
