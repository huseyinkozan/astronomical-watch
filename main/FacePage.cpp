#include "FacePage.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Fonts.h"
#include "Language.h"
#include "AnalogClock.h"
#include "Panel.h"
#include "SevenSegment.h"

static const lv_color_t s_grey = LV_COLOR_MAKE( 0xA0, 0xA0, 0xA0 );
static const lv_color_t s_amber = LV_COLOR_MAKE( 0xFF, 0xB0, 0x40 );
static const lv_color_t s_notchOff = LV_COLOR_MAKE( 0x40, 0x40, 0x40 );
static const lv_color_t s_moonDark = LV_COLOR_MAKE( 0x30, 0x30, 0x38 );
static const lv_color_t s_moonLit = LV_COLOR_MAKE( 0xE6, 0xE8, 0xF0 );
static const uint32_t s_slideMs = 250;

// The Large template's clock.
static const float s_segmentHeight = 122;
static const float s_analogRadius = 140;
static const lv_color_t s_minorMark = LV_COLOR_MAKE( 0x70, 0x70, 0x70 );

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
    lv_obj_set_style_text_font( label, font, 0 );
    lv_obj_set_style_text_color( label, colour, 0 );
    lv_obj_set_style_text_align( label, LV_TEXT_ALIGN_CENTER, 0 );
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

    m_mono = createLabel( m_root, &jetbrains_mono_light_110, lv_color_white() );
    centreAt( m_mono, FaceConfig::CentreX, FaceConfig::ClockY );
    const int segmentsWidth = static_cast<int>( std::ceil( SevenSegment::timeWidth( s_segmentHeight ) ) );
    m_segments = createBox( m_root, segmentsWidth, static_cast<int>( s_segmentHeight ) );
    centreAt( m_segments, FaceConfig::CentreX, FaceConfig::ClockY );
    lv_obj_add_event_cb( m_segments, onDrawSegments, LV_EVENT_DRAW_MAIN, this );
    // A margin for the hands' round ends.
    const int analogSize = 2 * static_cast<int>( s_analogRadius ) + 16;
    m_analog = createBox( m_root, analogSize, analogSize );
    centreAt( m_analog, FaceConfig::CentreX, FaceConfig::ClockY );
    lv_obj_add_event_cb( m_analog, onDrawAnalog, LV_EVENT_DRAW_MAIN, this );

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
    if ( !timeValid ) {
        lv_label_set_text( m_mono, "" );
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

// The lit part in pale white, the dark part dim, the terminator half an ellipse; 4 x 4 samples
// per pixel smooth the edges.
void FacePage::paintMoon()
{
    if ( m_moon == nullptr || m_moonPercent < 0 ) {
        return;
    }
    m_moonPainted = true;
    const double radius = s_moonSize / 2.0 - 1;
    const double centre = s_moonSize / 2.0;
    const double fraction = m_moonPercent / 100.0;
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
                    if ( m_waxing ? dx >= terminator : -dx >= terminator ) {
                        ++litSamples;
                    } else {
                        ++darkSamples;
                    }
                }
            }
            const lv_color_t colour = lv_color_mix( s_moonLit, lv_color_black(), static_cast<uint8_t>( litSamples * 255 / 16 ) );
            lv_canvas_set_px( m_moon, x, y, lv_color_mix( s_moonDark, colour, static_cast<uint8_t>( darkSamples * 255 / 16 ) ),
                              LV_OPA_COVER );
        }
    }
    lv_obj_invalidate( m_moon );
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
    AnalogClock::Segment segments[AnalogClock::MaxSegments];
    const int count = AnalogClock::segments( face->m_page.analog, s_analogRadius, timeValid ? face->m_hour : 0,
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
        line.p1.x = cx + segment.x0;
        line.p1.y = cy + segment.y0;
        line.p2.x = cx + segment.x1;
        line.p2.y = cy + segment.y1;
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
    const int x = static_cast<int>( cx );
    const int y = static_cast<int>( cy );
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
    if ( face->m_hour < 0 ) {
        return;
    }
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t area;
    lv_obj_get_coords( face->m_segments, &area );
    float digitX[4] = {};
    float colonX = 0;
    SevenSegment::layout( s_segmentHeight, digitX, colonX );
    const int digits[4] = { face->m_hour / 10, face->m_hour % 10, face->m_minute / 10, face->m_minute % 10 };
    for ( int d = 0; d < 4; ++d ) {
        SevenSegment::Hexagon hexagons[7];
        const int count = SevenSegment::segments( digits[d], s_segmentHeight, hexagons );
        for ( int s = 0; s < count; ++s ) {
            drawSegment( layer, hexagons[s], area.x1 + digitX[d], area.y1 );
        }
    }
    // The colon: two squares at 0.3 and 0.7 of the height.
    const float side = SevenSegment::colonSize( s_segmentHeight );
    lv_draw_rect_dsc_t square;
    lv_draw_rect_dsc_init( &square );
    square.bg_color = lv_color_white();
    square.bg_opa = LV_OPA_COVER;
    for ( int i = 0; i < 2; ++i ) {
        const float centreY = s_segmentHeight * ( i == 0 ? 0.3f : 0.7f );
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
