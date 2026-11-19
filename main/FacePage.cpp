#include "FacePage.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Styles.h"
#include "Fonts.h"
#include "Language.h"
#include "AnalogClock.h"
#include "MilkyWay.h"
#include "Panel.h"
#include "SevenSegment.h"

static const lv_color_t s_grey = LV_COLOR_MAKE( 0xA0, 0xA0, 0xA0 );
static const lv_color_t s_amber = LV_COLOR_MAKE( 0xFF, 0xB0, 0x40 );
static const lv_color_t s_notchOff = LV_COLOR_MAKE( 0x40, 0x40, 0x40 );
static const lv_color_t s_moonDark = LV_COLOR_MAKE( 0x30, 0x30, 0x38 );
static const lv_color_t s_moonLit = LV_COLOR_MAKE( 0xE6, 0xE8, 0xF0 );
static const uint32_t s_slideMs = 250;

static const lv_color_t s_minorMark = LV_COLOR_MAKE( 0x70, 0x70, 0x70 );

// The arc templates (the mock-up's places, docs/superpowers/plans/2026-11-18-face-templates-phase3.md):
// the orbits around the centre, the paths over the horizon.
static const int s_orbitY = 251;
static const float s_orbitRadius[2] = { 150, 122 };  // the Sun, the Moon
static const int s_horizonY = 232;
static const int s_horizonLeft = 40;
static const int s_horizonRight = 370;
static const float s_pathRadius[2] = { 150, 105 };
static const float s_bandClip = 160;  // the band shows inside this half disc
static const int s_bandWidth = 16;
static const lv_color_t s_band = LV_COLOR_MAKE( 0x96, 0xAA, 0xFF );
static const lv_color_t s_sunDim = LV_COLOR_MAKE( 0x60, 0x44, 0x1A );
static const lv_color_t s_moonDim = LV_COLOR_MAKE( 0x50, 0x54, 0x60 );
static const lv_color_t s_horizon = LV_COLOR_MAKE( 0x60, 0x60, 0x60 );
static const int s_skyMoonSize = 25;

static const int s_moonSize = 37;
static const int s_weekdayRadius = 20;
static const int s_weekdayWidth = 4;
static const float s_notchGap = 8;  // degrees between two notches

static void centreAt( lv_obj_t *object, int x, int y )
{
    lv_obj_align( object, LV_ALIGN_CENTER, x - Panel::WIDTH / 2, y - Panel::HEIGHT / 2 );
}

static lv_obj_t *createLabel( lv_obj_t *parent, const lv_font_t *font, lv_color_t colour )
{
    lv_obj_t *label = lv_label_create( parent );
    Styles::text( label, font, colour );
    Styles::centred( label );
    lv_label_set_text( label, "" );
    return label;
}

static lv_obj_t *createBox( lv_obj_t *parent, int width, int height )
{
    lv_obj_t *box = lv_obj_create( parent );
    lv_obj_remove_style_all( box );
    lv_obj_set_size( box, width, height );
    lv_obj_set_clickable( box, false );
    lv_obj_set_scrollable( box, false );
    return box;
}

void FacePage::build( lv_obj_t *tile )
{
    // Not clickable: presses and swipes go to the tile, which takes the vertical swipe.
    m_root = createBox( tile, Panel::WIDTH, Panel::HEIGHT );

    // Under the clock: the arcs, and the Moon on them.
    m_arcs = createBox( m_root, Panel::WIDTH, Panel::HEIGHT );
    lv_obj_add_event_cb( m_arcs, onDrawArcs, LV_EVENT_DRAW_MAIN, this );
    m_skyMoon = lv_canvas_create( m_root );
    lv_canvas_set_draw_buf( m_skyMoon, lv_draw_buf_create( s_skyMoonSize, s_skyMoonSize, LV_COLOR_FORMAT_RGB565, 0 ) );
    lv_canvas_fill_bg( m_skyMoon, lv_color_black(), LV_OPA_COVER );
    lv_obj_set_hidden( m_skyMoon, true );

    // The clock objects get their size and place from the template (apply()).
    m_mono = createLabel( m_root, &jetbrains_mono_light_110, lv_color_white() );
    m_segments = createBox( m_root, 1, 1 );
    lv_obj_add_event_cb( m_segments, onDrawSegments, LV_EVENT_DRAW_MAIN, this );
    m_analog = createBox( m_root, 1, 1 );
    lv_obj_add_event_cb( m_analog, onDrawAnalog, LV_EVENT_DRAW_MAIN, this );
    m_noTime = createLabel( m_root, &jetbrains_mono_26, lv_color_white() );
    lv_label_set_text( m_noTime, "--:--" );
    lv_obj_set_hidden( m_noTime, true );

    for ( int i = 0; i < FaceConfig::TextSlotCount; ++i ) {
        m_texts[i] = createLabel( m_root, &jetbrains_mono_26, i == FaceConfig::TopEdge || i == FaceConfig::BottomEdge
                                                                  ? lv_color_white()
                                                                  : s_grey );
        centreAt( m_texts[i], FaceConfig::CentreX, FaceConfig::TextY[i] );
    }
    for ( int i = 0; i < 4; ++i ) {
        m_cornerTexts[i] = createLabel( m_root, &jetbrains_mono_26, lv_color_white() );
        centreAt( m_cornerTexts[i], FaceConfig::CornerX[i], FaceConfig::CornerY[i] );
    }

    m_moon = lv_canvas_create( m_root );
    lv_canvas_set_draw_buf( m_moon, lv_draw_buf_create( s_moonSize, s_moonSize, LV_COLOR_FORMAT_RGB565, 0 ) );
    lv_canvas_fill_bg( m_moon, lv_color_black(), LV_OPA_COVER );
    const int weekdaySize = 2 * s_weekdayRadius + 2;
    m_weekday = createBox( m_root, weekdaySize, weekdaySize );
    lv_obj_add_event_cb( m_weekday, onDrawWeekday, LV_EVENT_DRAW_MAIN, this );
    m_weekdayLetter = createLabel( m_weekday, &jetbrains_mono_26, lv_color_white() );
    lv_obj_center( m_weekdayLetter );
    apply();
}

static void setTranslateY( void *object, int32_t y )
{
    lv_obj_set_style_translate_y( static_cast<lv_obj_t *>( object ), y, 0 );
}

void FacePage::show( const FaceConfig::Page &page, int direction )
{
    m_page = page;
    apply();
    // The new page slides in over the place of the old one, which goes at once.
    lv_anim_delete( m_root, setTranslateY );
    lv_obj_set_style_translate_y( m_root, 0, 0 );
    if ( direction != 0 ) {
        lv_anim_t animation;
        lv_anim_init( &animation );
        lv_anim_set_var( &animation, m_root );
        lv_anim_set_exec_cb( &animation, setTranslateY );
        lv_anim_set_values( &animation, direction * Panel::HEIGHT, 0 );
        lv_anim_set_duration( &animation, s_slideMs );
        lv_anim_set_path_cb( &animation, lv_anim_path_ease_out );
        lv_anim_start( &animation );
    }
}

// What the page shows apart from the texts: which clock, which corner icons where.
void FacePage::apply()
{
    if ( m_root == nullptr ) {
        return;
    }
    const FaceConfig::ClockPlace place = FaceConfig::clockPlace( m_page.layout );
    m_digitHeight = place.digitHeight;
    m_analogRadius = place.analogRadius;
    m_clockY = place.centreY;
    lv_obj_set_style_text_font( m_mono, m_digitHeight >= 100 ? &jetbrains_mono_light_110 : &jetbrains_mono_light_64, 0 );
    centreAt( m_mono, FaceConfig::CentreX, m_clockY );
    lv_obj_set_size( m_segments, static_cast<int32_t>( std::ceil( SevenSegment::timeWidth( static_cast<float>( m_digitHeight ) ) ) ),
                     m_digitHeight );
    centreAt( m_segments, FaceConfig::CentreX, m_clockY );
    // A margin for the hands' round ends.
    lv_obj_set_size( m_analog, 2 * m_analogRadius + 16, 2 * m_analogRadius + 16 );
    centreAt( m_analog, FaceConfig::CentreX, m_clockY );
    centreAt( m_noTime, FaceConfig::CentreX, m_clockY );
    lv_obj_set_hidden( m_arcs, m_page.layout == FaceConfig::Large );
    m_skyMinute = -1;
    placeSky();
    const bool digital = m_page.clock == FaceConfig::DigitalClock;
    lv_obj_set_hidden( m_mono, !( digital && m_page.digital == FaceConfig::MonoDigits ) );
    lv_obj_set_hidden( m_segments, !( digital && m_page.digital == FaceConfig::SevenSegment ) );
    lv_obj_set_hidden( m_analog, m_page.clock != FaceConfig::AnalogClock );
    bool moon = false;
    bool weekday = false;
    for ( int i = 0; i < 4; ++i ) {
        const int item = m_page.items[FaceConfig::TopLeft + i];
        // One icon of each kind: the first corner that asks for it.
        if ( item == FaceConfig::MoonIcon && !moon ) {
            moon = true;
            centreAt( m_moon, FaceConfig::CornerX[i], FaceConfig::CornerY[i] );
        }
        if ( item == FaceConfig::WeekdayIcon && !weekday ) {
            weekday = true;
            centreAt( m_weekday, FaceConfig::CornerX[i], FaceConfig::CornerY[i] );
        }
        // The altitudes in the colour of their body (the font has no sun or moon sign).
        lv_color_t colour = lv_color_white();
        if ( item == FaceConfig::SunAltitudeCorner ) {
            colour = s_amber;
        } else if ( item == FaceConfig::MoonAltitudeCorner ) {
            colour = s_moonLit;
        }
        lv_obj_set_style_text_color( m_cornerTexts[i], colour, 0 );
    }
    lv_obj_set_hidden( m_moon, !moon );
    lv_obj_set_hidden( m_weekday, !weekday );
    // The clock and the texts again for the new page.
    m_hour = -1;
    update( m_inputs );
}

void FacePage::update( const FaceText::Inputs &inputs )
{
    m_inputs = inputs;
    if ( m_root == nullptr ) {
        return;
    }
    const bool timeValid = inputs.timeValid;
    // Without a valid time (the clock never set, as after the battery was unplugged) the clock
    // shows dashes, so that the face does not look stuck.
    if ( !timeValid ) {
        lv_label_set_text( m_mono, "--:--" );
    } else if ( inputs.hour != m_hour || inputs.minute != m_minute ) {
        lv_label_set_text_fmt( m_mono, "%02d:%02d", inputs.hour, inputs.minute );
    }
    if ( timeValid != ( m_hour >= 0 ) || inputs.hour != m_hour || inputs.minute != m_minute ) {
        lv_obj_invalidate( m_segments );
        lv_obj_invalidate( m_analog );
    } else if ( secondsHand() && inputs.second != m_second ) {
        lv_obj_invalidate( m_analog );
    }
    m_hour = timeValid ? inputs.hour : -1;
    m_minute = timeValid ? inputs.minute : -1;
    m_second = timeValid ? inputs.second : -1;
    lv_obj_set_hidden( m_noTime, timeValid || m_page.clock != FaceConfig::AnalogClock );

    char text[64];
    const int advance = lv_font_get_glyph_width( &jetbrains_mono_26, 'M', 0 );
    for ( int i = 0; i < FaceConfig::TextSlotCount; ++i ) {
        const int width = FaceConfig::textWidth( m_page, i );
        FaceText::text( static_cast<FaceConfig::TextItem>( m_page.items[i] ), m_page.parameters[i], inputs, text,
                        sizeof( text ), width / advance );
        // Cut to whole characters: a slot text never runs into the corner gauges. A space left
        // at the end would move the centred text aside.
        int bytes = FaceText::fit( text, advance, width );
        while ( bytes > 0 && text[bytes - 1] == ' ' ) {
            --bytes;
        }
        text[bytes] = '\0';
        if ( strcmp( lv_label_get_text( m_texts[i] ), text ) != 0 ) {
            lv_label_set_text( m_texts[i], text );
        }
    }
    for ( int i = 0; i < 4; ++i ) {
        FaceText::corner( static_cast<FaceConfig::CornerItem>( m_page.items[FaceConfig::TopLeft + i] ), inputs, text,
                          sizeof( text ) );
        if ( strcmp( lv_label_get_text( m_cornerTexts[i] ), text ) != 0 ) {
            lv_label_set_text( m_cornerTexts[i], text );
        }
    }
    const int weekday = timeValid ? inputs.weekday : -1;
    if ( weekday != m_weekdayShown ) {
        m_weekdayShown = weekday;
        if ( weekday < 0 ) {
            lv_label_set_text( m_weekdayLetter, "" );
        } else {
            // The first letter of the short name ("Ç" of "Çar" is two bytes).
            const char *name = Language::shortDay( weekday );
            const int bytes = FaceText::fit( name, 1, 1 );
            snprintf( text, sizeof( text ), "%.*s", bytes, name );
            lv_label_set_text( m_weekdayLetter, text );
        }
        lv_obj_invalidate( m_weekday );
    }
}

void FacePage::setMoon( int percent, bool waxing )
{
    if ( m_moonPainted && percent == m_moonPercent && waxing == m_waxing ) {
        return;
    }
    m_moonPercent = percent;
    m_waxing = waxing;
    paintMoon();
}

// A moon of `size` pixels into `canvas`: the lit part in pale white, the dark part dim, the
// terminator half an ellipse; 4 x 4 samples per pixel smooth the edges.
static void paintMoonCanvas( lv_obj_t *canvas, int size, int percent, bool waxing )
{
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
            const lv_color_t colour = lv_color_mix( s_moonLit, lv_color_black(), static_cast<uint8_t>( litSamples * 255 / 16 ) );
            lv_canvas_set_px( canvas, x, y, lv_color_mix( s_moonDark, colour, static_cast<uint8_t>( darkSamples * 255 / 16 ) ),
                              LV_OPA_COVER );
        }
    }
    lv_obj_invalidate( canvas );
}

void FacePage::paintMoon()
{
    if ( m_moon == nullptr || m_moonPercent < 0 ) {
        return;
    }
    m_moonPainted = true;
    paintMoonCanvas( m_moon, s_moonSize, m_moonPercent, m_waxing );
    paintMoonCanvas( m_skyMoon, s_skyMoonSize, m_moonPercent, m_waxing );
}

void FacePage::setSky( const Corners::Input &corners, const Sky::Now &now, bool located )
{
    m_corners = corners;
    m_sky = now;
    if ( located != m_located || now.time / 60 != m_skyMinute ) {
        m_located = located;
        m_skyMinute = now.time / 60;
        placeSky();
    }
}

// The bodies' places (the old Dial and Sky arcs) and the band, once a minute; the Moon's object
// moves here, outside the drawing.
void FacePage::placeSky()
{
    if ( m_root == nullptr ) {
        return;
    }
    const bool orbits = m_page.layout == FaceConfig::FullArcs;
    const bool paths = m_page.layout == FaceConfig::HalfArcs;
    const Corners::Events *events[2] = { &m_corners.sun, &m_corners.moon };
    for ( int i = 0; i < 2; ++i ) {
        m_bodies[i] = {};
        if ( m_located && m_corners.valid ) {
            m_bodies[i] = Corners::arcPlace( *events[i], m_sky.time, orbits, m_sky.south );
        }
    }
    // The Moon's picture on its orbit or path while it is up.
    const Corners::ArcPlace &moon = m_bodies[1];
    const bool moonShown = ( orbits || paths ) && moon.known && moon.up;
    lv_obj_set_hidden( m_skyMoon, !moonShown );
    if ( moonShown ) {
        const float radius = orbits ? s_orbitRadius[1] : s_pathRadius[1];
        const int centreY = orbits ? s_orbitY : s_horizonY;
        const float a = moon.angle * static_cast<float>( M_PI ) / 180.0f;
        lv_obj_set_pos( m_skyMoon, static_cast<int32_t>( lroundf( FaceConfig::CentreX + radius * cosf( a ) ) ) - s_skyMoonSize / 2,
                        static_cast<int32_t>( lroundf( centreY + radius * sinf( a ) ) ) - s_skyMoonSize / 2 );
    }
    const bool milkyWay = paths && m_located && ( m_page.flags & FaceConfig::MilkyWayFlag ) != 0;
    const MilkyWay::Band band = MilkyWay::band( m_sky.galacticCentre.altitude, m_sky.galacticCentre.azimuth,
                                                m_sky.sun.altitude, m_sky.south, FaceConfig::CentreX, s_horizonY );
    m_bandShown = milkyWay && band.shown;
    m_bandBright = band.bright;
    m_band = band;
    m_dialBand = MilkyWay::dialBand( m_sky.galacticCentre.altitude, m_sky.sun.altitude,
                                     MilkyWay::dialAngle( m_sky.galacticHourAngle ), FaceConfig::CentreX, s_orbitY );
    m_dialBand.shown = m_dialBand.shown && orbits && m_located && ( m_page.flags & FaceConfig::MilkyWayFlag ) != 0;
    lv_obj_invalidate( m_arcs );
}

// A segment without seams: the box between its four shoulder points as a rectangle, the two
// pointed ends as triangles whose bases reach a pixel into it (the anti-aliased edges of
// triangles that only touch left a dark line). Points 0 and 3 are the tips.
static void drawSegment( lv_layer_t *layer, const SevenSegment::Hexagon &hexagon, float x, float y )
{
    lv_point_t p[6];
    for ( int i = 0; i < 6; ++i ) {
        p[i].x = static_cast<int32_t>( lroundf( x + hexagon.p[i].x ) );
        p[i].y = static_cast<int32_t>( lroundf( y + hexagon.p[i].y ) );
    }
    const bool horizontal = p[0].y == p[3].y;
    lv_draw_rect_dsc_t box;
    lv_draw_rect_dsc_init( &box );
    box.bg_color = lv_color_white();
    box.bg_opa = LV_OPA_COVER;
    lv_area_t area;
    if ( horizontal ) {
        area.x1 = p[1].x;
        area.x2 = p[2].x - 1;
        area.y1 = p[1].y;
        area.y2 = p[5].y - 1;
    } else {
        area.x1 = p[5].x;
        area.x2 = p[1].x - 1;
        area.y1 = p[1].y;
        area.y2 = p[2].y - 1;
    }
    lv_draw_rect( layer, &box, &area );
    const int dx = horizontal ? 1 : 0;
    const int dy = horizontal ? 0 : 1;
    lv_draw_triangle_dsc_t end;
    lv_draw_triangle_dsc_init( &end );
    end.color = lv_color_white();
    end.opa = LV_OPA_COVER;
    const int ends[2][3] = { { 5, 0, 1 }, { 2, 3, 4 } };
    for ( int e = 0; e < 2; ++e ) {
        const int inward = e == 0 ? 1 : -1;
        for ( int c = 0; c < 3; ++c ) {
            const int index = ends[e][c];
            const bool base = c != 1;
            end.p[c].x = p[index].x + ( base ? inward * dx : 0 );
            end.p[c].y = p[index].y + ( base ? inward * dy : 0 );
        }
        lv_draw_triangle( layer, &end );
    }
}

bool FacePage::secondsHand() const
{
    return m_page.clock == FaceConfig::AnalogClock && ( m_page.flags & FaceConfig::SecondsHandFlag ) != 0;
}

// The analog clock of the page's style: the marks always, the hands with a valid time.
void FacePage::onDrawAnalog( lv_event_t *event )
{
    const FacePage *face = static_cast<FacePage *>( lv_event_get_user_data( event ) );
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t area;
    lv_obj_get_coords( face->m_analog, &area );
    const float cx = ( area.x1 + area.x2 + 1 ) / 2.0f;
    const float cy = ( area.y1 + area.y2 + 1 ) / 2.0f;
    const bool timeValid = face->m_hour >= 0;
    // Not on the stack: only the LVGL task draws, and 1.5 KB more on its stack overflowed it
    // (a crash while swiping to the analog page).
    static AnalogClock::Segment segments[AnalogClock::MaxSegments];
    const int count = AnalogClock::segments( face->m_page.analog, static_cast<float>( face->m_analogRadius ), timeValid ? face->m_hour : 0,
                                             timeValid ? face->m_minute : 0, timeValid ? face->m_second : 0,
                                             timeValid && face->secondsHand(), segments, AnalogClock::MaxSegments );
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init( &line );
    for ( int i = 0; i < count; ++i ) {
        const AnalogClock::Segment &segment = segments[i];
        const bool mark = segment.role == AnalogClock::MajorMark || segment.role == AnalogClock::MinorMark;
        if ( !mark && !timeValid ) {
            continue;
        }
        line.color = lv_color_white();
        if ( segment.role == AnalogClock::MinorMark ) {
            line.color = s_minorMark;
        } else if ( segment.role == AnalogClock::SecondHand || segment.role == AnalogClock::OneHand ) {
            line.color = s_amber;
        }
        line.width = static_cast<int32_t>( segment.width );
        line.round_start = mark ? 0 : 1;
        line.round_end = mark ? 0 : 1;
        // Rounded, not cut: LVGL keeps whole pixels here, and sinf( pi ) is a hair below 0, which
        // leaned the hand at half past and the 6 o'clock mark by a pixel.
        line.p1.x = static_cast<int32_t>( lroundf( cx + segment.x0 ) );
        line.p1.y = static_cast<int32_t>( lroundf( cy + segment.y0 ) );
        line.p2.x = static_cast<int32_t>( lroundf( cx + segment.x1 ) );
        line.p2.y = static_cast<int32_t>( lroundf( cy + segment.y1 ) );
        lv_draw_line( layer, &line );
    }
    if ( !timeValid ) {
        return;
    }
    // The centre: white, amber in the middle under a seconds hand.
    lv_draw_rect_dsc_t disc;
    lv_draw_rect_dsc_init( &disc );
    disc.bg_color = lv_color_white();
    disc.radius = LV_RADIUS_CIRCLE;
    const int x = static_cast<int>( lroundf( cx ) );
    const int y = static_cast<int>( lroundf( cy ) );
    const lv_area_t outer = { x - 6, y - 6, x + 6, y + 6 };
    lv_draw_rect( layer, &disc, &outer );
    if ( face->secondsHand() ) {
        disc.bg_color = s_amber;
        const lv_area_t inner = { x - 3, y - 3, x + 3, y + 3 };
        lv_draw_rect( layer, &disc, &inner );
    }
}

// The time as seven-segment digits.
void FacePage::onDrawSegments( lv_event_t *event )
{
    const FacePage *face = static_cast<FacePage *>( lv_event_get_user_data( event ) );
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t area;
    lv_obj_get_coords( face->m_segments, &area );
    const float height = static_cast<float>( face->m_digitHeight );
    float digitX[4] = {};
    float colonX = 0;
    SevenSegment::layout( height, digitX, colonX );
    int digits[4] = { SevenSegment::Dash, SevenSegment::Dash, SevenSegment::Dash, SevenSegment::Dash };
    if ( face->m_hour >= 0 ) {
        digits[0] = face->m_hour / 10;
        digits[1] = face->m_hour % 10;
        digits[2] = face->m_minute / 10;
        digits[3] = face->m_minute % 10;
    }
    for ( int d = 0; d < 4; ++d ) {
        SevenSegment::Hexagon hexagons[7];
        const int count = SevenSegment::segments( digits[d], height, hexagons );
        for ( int s = 0; s < count; ++s ) {
            drawSegment( layer, hexagons[s], area.x1 + digitX[d], area.y1 );
        }
    }
    // The colon: two squares at 0.3 and 0.7 of the height.
    const float side = SevenSegment::colonSize( height );
    lv_draw_rect_dsc_t square;
    lv_draw_rect_dsc_init( &square );
    square.bg_color = lv_color_white();
    square.bg_opa = LV_OPA_COVER;
    for ( int i = 0; i < 2; ++i ) {
        const float centreY = height * ( i == 0 ? 0.3f : 0.7f );
        lv_area_t dot;
        dot.x1 = area.x1 + static_cast<int32_t>( lroundf( colonX - side / 2 ) );
        dot.y1 = area.y1 + static_cast<int32_t>( lroundf( centreY - side / 2 ) );
        dot.x2 = dot.x1 + static_cast<int32_t>( lroundf( side ) ) - 1;
        dot.y2 = dot.y1 + static_cast<int32_t>( lroundf( side ) ) - 1;
        lv_draw_rect( layer, &square, &dot );
    }
}

// Seven notches around the weekday's letter, from 12 o'clock clockwise: the first n lit on the
// n-th day of the week (Monday one, Sunday seven).
void FacePage::onDrawWeekday( lv_event_t *event )
{
    const FacePage *face = static_cast<FacePage *>( lv_event_get_user_data( event ) );
    if ( face->m_weekdayShown < 0 ) {
        return;
    }
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t area;
    lv_obj_get_coords( face->m_weekday, &area );
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init( &arc );
    arc.center.x = ( area.x1 + area.x2 + 1 ) / 2;
    arc.center.y = ( area.y1 + area.y2 + 1 ) / 2;
    arc.radius = s_weekdayRadius;
    arc.width = s_weekdayWidth;
    const float step = 360.0f / 7;
    for ( int i = 0; i < 7; ++i ) {
        // LVGL counts angles clockwise from 3 o'clock.
        arc.start_angle = 270 + i * step + s_notchGap / 2;
        arc.end_angle = 270 + ( i + 1 ) * step - s_notchGap / 2;
        arc.color = i <= face->m_weekdayShown ? s_amber : s_notchOff;
        lv_draw_arc( layer, &arc );
    }
}

// Whether `box` meets the part of the screen being drawn: LVGL draws the arcs object band by band,
// and every rectangle it is given costs a draw task, also outside the band.
static bool inClip( const lv_layer_t *layer, const lv_area_t &box )
{
    const lv_area_t &clip = layer->_clip_area;
    return box.x2 >= clip.x1 && box.x1 <= clip.x2 && box.y2 >= clip.y1 && box.y1 <= clip.y2;
}

static void drawDot( lv_layer_t *layer, float x, float y, lv_color_t colour, int radius = 1 )
{
    const int32_t cx = static_cast<int32_t>( lroundf( x ) );
    const int32_t cy = static_cast<int32_t>( lroundf( y ) );
    const lv_area_t box = { cx - radius, cy - radius, cx + radius, cy + radius };
    if ( !inClip( layer, box ) ) {
        return;
    }
    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init( &dot );
    dot.bg_color = colour;
    // A 3 x 3 dot needs no round mask (each one costs a mask in LVGL's software renderer).
    dot.radius = radius > 1 ? LV_RADIUS_CIRCLE : 0;
    lv_draw_rect( layer, &dot, &box );
}

// Dots every `step` pixels along the circle of `radius` around (cx, cy) from angle a0 to a1
// (degrees, clockwise from 3 o'clock).
static void drawDottedArc( lv_layer_t *layer, float cx, float cy, float radius, float a0, float a1, float step,
                           lv_color_t colour )
{
    const int count = static_cast<int>( static_cast<float>( M_PI ) / 180.0f * ( a1 - a0 ) * radius / step );
    for ( int k = 0; k <= count; ++k ) {
        const float a = ( a0 + ( a1 - a0 ) * k / ( count > 0 ? count : 1 ) ) * static_cast<float>( M_PI ) / 180.0f;
        drawDot( layer, cx + radius * cosf( a ), cy + radius * sinf( a ), colour );
    }
}

// The Milky Way: the part of the band's circle that MilkyWay::drawn() keeps, as one arc.
static void drawBand( lv_layer_t *layer, int originX, int originY, const MilkyWay::Band &band, bool bright )
{
    // Around the observer's own angle on the circle: the kept part lies within 60 degrees of it
    // and may pass 0 degrees.
    const float observer = std::atan2( s_horizonY - band.cy, FaceConfig::CentreX - band.cx ) * 180.0f / static_cast<float>( M_PI );
    int first = 1000;
    int last = -1000;
    lv_area_t box = { 10000, 10000, -10000, -10000 };
    for ( int step = -60; step <= 60; ++step ) {
        const float a = ( observer + step ) * static_cast<float>( M_PI ) / 180.0f;
        const float x = band.cx + band.radius * cosf( a );
        const float y = band.cy + band.radius * sinf( a );
        if ( !MilkyWay::drawn( band, x, y, FaceConfig::CentreX, s_horizonY, s_bandClip ) ) {
            continue;
        }
        first = LV_MIN( first, step );
        last = LV_MAX( last, step );
        box.x1 = LV_MIN( box.x1, originX + static_cast<int32_t>( x ) - s_bandWidth );
        box.y1 = LV_MIN( box.y1, originY + static_cast<int32_t>( y ) - s_bandWidth );
        box.x2 = LV_MAX( box.x2, originX + static_cast<int32_t>( x ) + s_bandWidth );
        box.y2 = LV_MAX( box.y2, originY + static_cast<int32_t>( y ) + s_bandWidth );
    }
    if ( first > last || !inClip( layer, box ) ) {
        return;
    }
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init( &arc );
    arc.color = s_band;
    arc.opa = bright ? 115 : 51;  // 45 % or 20 %
    arc.width = s_bandWidth;
    arc.center.x = originX + static_cast<int32_t>( lroundf( band.cx ) );
    arc.center.y = originY + static_cast<int32_t>( lroundf( band.cy ) );
    arc.radius = static_cast<uint16_t>( band.radius + s_bandWidth / 2 );
    // LVGL takes an end below the start as an arc through 360.
    arc.start_angle = static_cast<int32_t>( lroundf( std::fmod( observer + first + 720, 360.0f ) ) );
    arc.end_angle = static_cast<int32_t>( lroundf( std::fmod( observer + last + 1 + 720, 360.0f ) ) );
    lv_draw_arc( layer, &arc );
}

// The Full arcs' Milky Way: short arcs along the band's circle, each as strong as
// MilkyWay::dialFade() at its middle (faded towards the clock, one end dim).
static void drawDialBand( lv_layer_t *layer, int originX, int originY, const MilkyWay::DialBand &band )
{
    const float dialX = FaceConfig::CentreX;
    const float dialY = s_orbitY;
    // The circle passes through the dial's centre; the band lies within 100 degrees of it.
    const float through = std::atan2( dialY - band.cy, dialX - band.cx ) * 180.0f / static_cast<float>( M_PI );
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init( &arc );
    arc.color = s_band;
    arc.width = MilkyWay::DialWidth;
    arc.center.x = originX + static_cast<int32_t>( lroundf( band.cx ) );
    arc.center.y = originY + static_cast<int32_t>( lroundf( band.cy ) );
    arc.radius = static_cast<uint16_t>( band.radius + MilkyWay::DialWidth / 2 );
    // 2 degrees a piece (about 7 px along the band): 4 showed the steps of the fade, and the
    // frame time while swiping did not change with the number of pieces.
    const int piece = 2;
    for ( int step = -100; step < 100; step += piece ) {
        const float middle = ( through + step + piece / 2.0f ) * static_cast<float>( M_PI ) / 180.0f;
        const float x = band.cx + band.radius * cosf( middle );
        const float y = band.cy + band.radius * sinf( middle );
        const float fade = MilkyWay::dialFade( band, x, y, dialX, dialY );
        const lv_area_t box = { originX + static_cast<int32_t>( x ) - MilkyWay::DialWidth, originY + static_cast<int32_t>( y ) - MilkyWay::DialWidth,
                                originX + static_cast<int32_t>( x ) + MilkyWay::DialWidth, originY + static_cast<int32_t>( y ) + MilkyWay::DialWidth };
        if ( fade < 0.02f || !inClip( layer, box ) ) {
            continue;
        }
        arc.opa = static_cast<lv_opa_t>( 140 * fade );  // 55 % at full strength
        arc.start_angle = static_cast<int32_t>( lroundf( std::fmod( through + step + 720, 360.0f ) ) );
        arc.end_angle = static_cast<int32_t>( lroundf( std::fmod( through + step + piece + 720, 360.0f ) ) );
        lv_draw_arc( layer, &arc );
    }
}

// The orbits (Full arcs) or the paths over the horizon (Half arcs), the Sun, and the Moon below
// the horizon; the Moon above it is its own object (m_skyMoon).
void FacePage::onDrawArcs( lv_event_t *event )
{
    const FacePage *face = static_cast<FacePage *>( lv_event_get_user_data( event ) );
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t area;
    lv_obj_get_coords( face->m_arcs, &area );
    const float x0 = static_cast<float>( area.x1 );
    const float y0 = static_cast<float>( area.y1 );
    const lv_color_t colours[2] = { s_amber, s_moonLit };
    const lv_color_t dimColours[2] = { s_sunDim, s_moonDim };
    if ( face->m_page.layout == FaceConfig::FullArcs ) {
        const float cx = x0 + FaceConfig::CentreX;
        const float cy = y0 + s_orbitY;
        if ( face->m_dialBand.shown ) {
            drawDialBand( layer, static_cast<int>( x0 ), static_cast<int>( y0 ), face->m_dialBand );
        }
        for ( int i = 0; i < 2; ++i ) {
            const Corners::ArcPlace &body = face->m_bodies[i];
            const float r = s_orbitRadius[i];
            if ( body.known ) {
                // Dense above the horizon, sparse below, marks at rise and set.
                drawDottedArc( layer, cx, cy, r, body.rise, body.set, 5, colours[i] );
                drawDottedArc( layer, cx, cy, r, body.set, body.rise + 360, 15, dimColours[i] );
                lv_draw_line_dsc_t mark;
                lv_draw_line_dsc_init( &mark );
                mark.color = colours[i];
                mark.width = 2;
                const float edges[2] = { body.rise, body.set };
                for ( int k = 0; k < 2; ++k ) {
                    const float a = edges[k] * static_cast<float>( M_PI ) / 180.0f;
                    mark.p1.x = static_cast<int32_t>( lroundf( cx + ( r - 6 ) * cosf( a ) ) );
                    mark.p1.y = static_cast<int32_t>( lroundf( cy + ( r - 6 ) * sinf( a ) ) );
                    mark.p2.x = static_cast<int32_t>( lroundf( cx + ( r + 6 ) * cosf( a ) ) );
                    mark.p2.y = static_cast<int32_t>( lroundf( cy + ( r + 6 ) * sinf( a ) ) );
                    lv_draw_line( layer, &mark );
                }
            } else {
                // No rise or set near now (no location, or the polar day or night).
                drawDottedArc( layer, cx, cy, r, 0, 360, body.alwaysUp ? 5 : 15, body.alwaysUp ? colours[i] : dimColours[i] );
            }
        }
        const Corners::ArcPlace &sun = face->m_bodies[0];
        if ( sun.known ) {
            const float a = sun.angle * static_cast<float>( M_PI ) / 180.0f;
            drawDot( layer, cx + s_orbitRadius[0] * cosf( a ), cy + s_orbitRadius[0] * sinf( a ), sun.up ? s_amber : s_sunDim, 9 );
        }
        const Corners::ArcPlace &moon = face->m_bodies[1];
        if ( moon.known && !moon.up ) {
            const float a = moon.angle * static_cast<float>( M_PI ) / 180.0f;
            drawDot( layer, cx + s_orbitRadius[1] * cosf( a ), cy + s_orbitRadius[1] * sinf( a ), s_moonDim, 8 );
        }
        return;
    }
    if ( face->m_page.layout != FaceConfig::HalfArcs ) {
        return;
    }
    const float cx = x0 + FaceConfig::CentreX;
    const float cy = y0 + s_horizonY;
    if ( face->m_bandShown ) {
        drawBand( layer, static_cast<int>( x0 ), static_cast<int>( y0 ), face->m_band, face->m_bandBright );
    }
    drawDottedArc( layer, cx, cy, s_pathRadius[0], 180, 360, 6, s_amber );
    drawDottedArc( layer, cx, cy, s_pathRadius[1], 180, 360, 6, s_moonLit );
    lv_draw_line_dsc_t horizon;
    lv_draw_line_dsc_init( &horizon );
    horizon.color = s_horizon;
    horizon.width = 1;
    horizon.p1.x = static_cast<int32_t>( x0 ) + s_horizonLeft;
    horizon.p1.y = static_cast<int32_t>( cy );
    horizon.p2.x = static_cast<int32_t>( x0 ) + s_horizonRight;
    horizon.p2.y = static_cast<int32_t>( cy );
    lv_draw_line( layer, &horizon );
    const Corners::ArcPlace &sun = face->m_bodies[0];
    if ( sun.known && sun.up ) {
        const float a = sun.angle * static_cast<float>( M_PI ) / 180.0f;
        drawDot( layer, cx + s_pathRadius[0] * cosf( a ), cy + s_pathRadius[0] * sinf( a ), s_amber, 8 );
    }
}
