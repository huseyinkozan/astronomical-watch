#include "PlanetPages.h"
#include "Fonts.h"
#include "Language.h"

#include <cmath>
#include <cstdio>

namespace {

// Colours of Planets::Body; the Earth blue. The tables write the names in them, so they are the
// key to the two drawings.
const uint32_t s_colours[Planets::BodyCount] = { 0xAAAAAA, 0xF0E1AA, 0x5A8CFF, 0xE65A3C, 0xE1BE8C, 0xDCBE6E };
const uint8_t s_orbitDotRadius[Planets::BodyCount] = { 3, 4, 5, 4, 7, 6 };
const uint8_t s_skyDotRadius[Planets::BodyCount] = { 3, 4, 0, 4, 6, 5 };
const uint32_t s_sunColour = 0xFFD040;
const uint32_t s_moonColour = 0xDCE1EB;
const uint32_t s_lineColour = 0x5A5A5A;
const uint32_t s_dimLineColour = 0x373737;
const uint32_t s_grey = 0xA0A0A0;

// Mean distances (AU): the orbits on a square-root scale, Saturn's 180 px, so that Mercury's
// still shows (36 px) and the order and spacing read right.
const double s_meanDistance[Planets::BodyCount] = { 0.387, 0.723, 1.0, 1.524, 5.203, 9.537 };
const double s_saturnOrbitPx = 180;
// The sky: the sphere seen from the south, as a circle; its centre where the orbits' is.
const double s_skyRadius = 160;
const int s_centreY = 253;  // in the part, below its top padding: 265 on the screen

// The planets in the tables, in Planets::Body order without the Earth.
const Planets::Body s_planets[5] = { Planets::Mercury, Planets::Venus, Planets::Mars, Planets::Jupiter, Planets::Saturn };
const Language::Text s_names[5] = { Language::PlanetMercury, Language::PlanetVenus, Language::PlanetMars,
                                    Language::PlanetJupiter, Language::PlanetSaturn };

uint32_t dimmed( uint32_t rgb )
{
    // A body below the horizon: about a third of its colour, as the dim half of the orbits.
    const uint32_t r = ( ( rgb >> 16 ) & 0xFF ) * 35 / 100;
    const uint32_t g = ( ( rgb >> 8 ) & 0xFF ) * 35 / 100;
    const uint32_t b = ( rgb & 0xFF ) * 35 / 100;
    return ( r << 16 ) | ( g << 8 ) | b;
}

TablePart::Layout tableLayout( const char *title )
{
    // As the information page's tables: a 160 px name and two 105 px values, centred.
    TablePart::Layout layout = { title, 6, {}, -1, 38, 38, 20, 160, 180, 105, 0, 32 + 9 * 38 };
    for ( int i = 0; i < 6; ++i ) {
        layout.kinds[i] = TablePart::TwoValues;
    }
    return layout;
}

void drawDot( lv_layer_t *layer, int x, int y, int radius, uint32_t colour )
{
    lv_draw_rect_dsc_t disc;
    lv_draw_rect_dsc_init( &disc );
    disc.bg_color = lv_color_hex( colour );
    disc.radius = LV_RADIUS_CIRCLE;
    const lv_area_t area = { x - radius, y - radius, x + radius, y + radius };
    lv_draw_rect( layer, &disc, &area );
}

void drawCircle( lv_layer_t *layer, int cx, int cy, int radius, int from, int to, uint32_t colour, int width )
{
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init( &arc );
    arc.center.x = cx;
    arc.center.y = cy;
    arc.radius = static_cast<uint16_t>( radius );
    arc.start_angle = from;
    arc.end_angle = to;
    arc.width = width;
    arc.color = lv_color_hex( colour );
    lv_draw_arc( layer, &arc );
}

// "HH:MM" of a Unix time in local time, rounded to the minute, or "--:--".
void formatTime( char *text, size_t size, const Astronomy::Event &event, int utcOffsetMinutes )
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

bool sameObserver( const Astronomy::Observer &a, const Astronomy::Observer &b )
{
    return a.latitude == b.latitude && a.longitude == b.longitude;
}

}  // namespace

static lv_obj_t *createDrawing( lv_obj_t *part, lv_event_cb_t onDraw, void *user )
{
    lv_obj_t *object = lv_obj_create( part );
    if ( object == nullptr ) {
        return nullptr;
    }
    lv_obj_remove_style_all( object );
    lv_obj_set_size( object, LV_PCT( 100 ), LV_PCT( 100 ) );
    lv_obj_set_clickable( object, false );
    lv_obj_set_scrollable( object, false );
    lv_obj_add_event_cb( object, onDraw, LV_EVENT_DRAW_MAIN, user );
    return object;
}

void PlanetPages::build( lv_obj_t *tile )
{
    m_stack.build( tile, PartCount, Language::text( Language::PlanetsPageName ) );
    m_orbits = createDrawing( m_stack.part( OrbitsPart ), onDrawOrbits, this );
    m_sky = createDrawing( m_stack.part( SkyPart ), onDrawSky, this );

    m_riseSet.text().set( 0, 1, Language::text( Language::Rise ) );
    m_riseSet.text().set( 0, 2, Language::text( Language::Set ) );
    m_riseSet.text().setColours( 0, TableText::Grey, TableText::Grey );
    m_distance.text().set( 0, 1, Language::text( Language::AstronomicalUnit ) );
    m_distance.text().set( 0, 2, Language::text( Language::LightTime ) );
    m_distance.text().setColours( 0, TableText::Grey, TableText::Grey );
    for ( int i = 0; i < 5; ++i ) {
        const uint32_t colour = s_colours[s_planets[i]];
        m_riseSet.text().set( 1 + i, 0, Language::text( s_names[i] ) );
        m_riseSet.text().setColours( 1 + i, colour, TableText::White );
        m_distance.text().set( 1 + i, 0, Language::text( s_names[i] ) );
        m_distance.text().setColours( 1 + i, colour, TableText::White );
    }
    m_riseSet.build( m_stack.part( RiseSetPart ), tableLayout( Language::text( Language::RiseAndSet ) ) );
    m_distance.build( m_stack.part( DistancePart ), tableLayout( Language::text( Language::Distance ) ) );
    m_built = true;
    m_placedMinute = -1;
    fillRiseSet();
    if ( m_now != 0 ) {
        place();
    }
}

void PlanetPages::forget()
{
    m_stack.forget();
    m_riseSet.forget();
    m_distance.forget();
    m_orbits = nullptr;
    m_sky = nullptr;
    m_built = false;
}

StackPage &PlanetPages::stack()
{
    return m_stack;
}

void PlanetPages::show( int64_t now, const Astronomy::Observer &observer, int utcOffsetMinutes, bool located )
{
    const bool moved = !sameObserver( observer, m_observer ) || located != m_located || utcOffsetMinutes != m_offsetMinutes;
    m_now = now;
    m_observer = observer;
    m_located = located;
    if ( utcOffsetMinutes != m_offsetMinutes ) {
        m_offsetMinutes = utcOffsetMinutes;
        fillRiseSet();
    }
    if ( m_built && ( moved || now / 60 != m_placedMinute ) ) {
        place();
    }
}

// The bodies' places on both drawings and the distances, once a minute.
void PlanetPages::place()
{
    m_placedMinute = m_now / 60;
    const double degree = M_PI / 180;
    const double cx = 205;
    for ( int i = 0; i < Planets::BodyCount; ++i ) {
        const Planets::Heliocentric helio = Planets::heliocentric( static_cast<Planets::Body>( i ), m_now );
        const double r = std::sqrt( s_meanDistance[i] / s_meanDistance[Planets::Saturn] ) * s_saturnOrbitPx;
        // Seen from the ecliptic's north: anticlockwise, the vernal equinox to the right.
        Dot &dot = m_orbitDots[i];
        dot.x = static_cast<int16_t>( lround( cx + r * std::cos( helio.longitude * degree ) ) );
        dot.y = static_cast<int16_t>( lround( s_centreY - r * std::sin( helio.longitude * degree ) ) );
        dot.radius = s_orbitDotRadius[i];
        dot.colour = s_colours[i];
        dot.ring = i == Planets::Saturn;
    }

    // The sky: the Sun and the Moon first, so that the small planets stay on top.
    Astronomy::Position positions[SkyBodies] = {};
    uint32_t colours[SkyBodies] = {};
    uint8_t radii[SkyBodies] = {};
    int count = 0;
    positions[count] = Astronomy::sunPosition( m_now, m_observer );
    colours[count] = s_sunColour;
    radii[count++] = 8;
    positions[count] = Astronomy::moonPosition( m_now, m_observer );
    colours[count] = s_moonColour;
    radii[count++] = 7;
    char text[24];
    for ( int i = 0; i < 5; ++i ) {
        const Planets::Body body = s_planets[i];
        positions[count] = Planets::position( body, m_now, m_observer );
        colours[count] = s_colours[body];
        radii[count++] = s_skyDotRadius[body];
        // The distance and the light time.
        const Planets::Geocentric geo = Planets::geocentric( body, m_now );
        snprintf( text, sizeof( text ), "%.2f", geo.distance );
        m_distance.text().set( 1 + i, 1, text );
        snprintf( text, sizeof( text ), Language::text( Language::Minutes ),
                  static_cast<int>( lround( geo.distance * 499.004784 / 60 ) ) );
        m_distance.text().set( 1 + i, 2, text );
    }
    for ( int i = 0; i < count; ++i ) {
        const double altitude = positions[i].altitude * degree;
        const double azimuth = positions[i].azimuth * degree;
        Dot &dot = m_skyDots[i];
        // The sphere seen from the south: east on the left, the zenith on top.
        dot.x = static_cast<int16_t>( lround( cx + s_skyRadius * std::cos( altitude ) * std::sin( azimuth - M_PI ) ) );
        dot.y = static_cast<int16_t>( lround( s_centreY - s_skyRadius * std::sin( altitude ) ) );
        dot.radius = m_located ? radii[i] : 0;
        dot.colour = positions[i].altitude > 0 ? colours[i] : dimmed( colours[i] );
        dot.ring = false;
    }
    m_distance.refresh();
    if ( m_orbits != nullptr ) {
        lv_obj_invalidate( m_orbits );
    }
    if ( m_sky != nullptr ) {
        lv_obj_invalidate( m_sky );
    }
}

void PlanetPages::fillRiseSet()
{
    char text[8];
    for ( int i = 0; i < 5; ++i ) {
        const Astronomy::Crossings &crossings = m_day.riseSet[s_planets[i]];
        formatTime( text, sizeof( text ), m_dayValid ? crossings.rising : Astronomy::Event {}, m_offsetMinutes );
        m_riseSet.text().set( 1 + i, 1, text );
        formatTime( text, sizeof( text ), m_dayValid ? crossings.setting : Astronomy::Event {}, m_offsetMinutes );
        m_riseSet.text().set( 1 + i, 2, text );
    }
    m_riseSet.refresh();
}

bool PlanetPages::takeDayRequest( int64_t &localMidnight, Astronomy::Observer &observer )
{
    if ( !m_built || !m_located || m_now == 0 ) {
        return false;
    }
    const int64_t offset = static_cast<int64_t>( m_offsetMinutes ) * 60;
    int64_t day = ( m_now + offset ) / 86400;
    if ( m_now + offset < 0 ) {
        --day;
    }
    const int64_t midnight = day * 86400 - offset;
    if ( m_dayValid && midnight == m_dayMidnight && sameObserver( m_observer, m_dayObserver ) ) {
        return false;
    }
    // Asked once per day and place; the answer comes from the main task.
    if ( m_dayAsked && midnight == m_dayMidnight && sameObserver( m_observer, m_dayObserver ) ) {
        return false;
    }
    m_dayAsked = true;
    m_dayValid = false;
    m_dayMidnight = midnight;
    m_dayObserver = m_observer;
    localMidnight = midnight;
    observer = m_observer;
    return true;
}

void PlanetPages::showDay( const Planets::Day &day, int64_t localMidnight, const Astronomy::Observer &observer )
{
    if ( localMidnight != m_dayMidnight || !sameObserver( observer, m_dayObserver ) ) {
        return;
    }
    m_day = day;
    m_dayValid = true;
    m_dayAsked = false;
    fillRiseSet();
}

void PlanetPages::onDrawOrbits( lv_event_t *event )
{
    const PlanetPages *self = static_cast<PlanetPages *>( lv_event_get_user_data( event ) );
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t area;
    lv_obj_get_coords( self->m_orbits, &area );
    const int cx = area.x1 + 205;
    const int cy = area.y1 + s_centreY;
    for ( int i = 0; i < Planets::BodyCount; ++i ) {
        const int r = static_cast<int>( lround( std::sqrt( s_meanDistance[i] / s_meanDistance[Planets::Saturn] ) * s_saturnOrbitPx ) );
        drawCircle( layer, cx, cy, r, 0, 360, s_dimLineColour, 1 );
    }
    drawDot( layer, cx, cy, 7, s_sunColour );
    if ( self->m_placedMinute < 0 ) {
        return;
    }
    for ( int i = 0; i < Planets::BodyCount; ++i ) {
        const Dot &dot = self->m_orbitDots[i];
        const int x = area.x1 + dot.x;
        const int y = area.y1 + dot.y;
        if ( dot.ring ) {
            // Saturn's ring, edge-on: a line through the disc.
            lv_draw_line_dsc_t line;
            lv_draw_line_dsc_init( &line );
            line.color = lv_color_hex( dot.colour );
            line.width = 2;
            line.p1.x = x - 11;
            line.p1.y = y;
            line.p2.x = x + 11;
            line.p2.y = y;
            lv_draw_line( layer, &line );
        }
        drawDot( layer, x, y, dot.radius, dot.colour );
    }
}

void PlanetPages::onDrawSky( lv_event_t *event )
{
    const PlanetPages *self = static_cast<PlanetPages *>( lv_event_get_user_data( event ) );
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t area;
    lv_obj_get_coords( self->m_sky, &area );
    const int cx = area.x1 + 205;
    const int cy = area.y1 + s_centreY;
    const int radius = static_cast<int>( s_skyRadius );
    // Above the horizon a line, below it dots, as the Full arcs face's orbits.
    drawCircle( layer, cx, cy, radius, 180, 360, s_lineColour, 2 );
    for ( int k = 1; k < 60; ++k ) {
        const double angle = k * 3 * M_PI / 180;
        const int x = cx + static_cast<int>( lround( radius * std::cos( angle ) ) );
        const int y = cy + static_cast<int>( lround( radius * std::sin( angle ) ) );
        drawDot( layer, x, y, 1, s_dimLineColour );
    }
    lv_draw_line_dsc_t horizon;
    lv_draw_line_dsc_init( &horizon );
    horizon.color = lv_color_hex( 0x6E6E6E );
    horizon.width = 2;
    horizon.p1.x = cx - radius - 4;
    horizon.p1.y = cy;
    horizon.p2.x = cx + radius + 4;
    horizon.p2.y = cy;
    lv_draw_line( layer, &horizon );
    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init( &label );
    label.font = &jetbrains_mono_26;
    label.color = lv_color_hex( s_grey );
    label.text = Language::text( Language::EastLetter );
    const lv_area_t east = { cx - radius - 24, cy - 16, cx - radius - 6, cy + 16 };
    lv_draw_label( layer, &label, &east );
    label.text = Language::text( Language::WestLetter );
    const lv_area_t west = { cx + radius + 8, cy - 16, cx + radius + 26, cy + 16 };
    lv_draw_label( layer, &label, &west );
    if ( self->m_placedMinute < 0 ) {
        return;
    }
    for ( int i = 0; i < SkyBodies; ++i ) {
        const Dot &dot = self->m_skyDots[i];
        if ( dot.radius > 0 ) {
            drawDot( layer, area.x1 + dot.x, area.y1 + dot.y, dot.radius, dot.colour );
        }
    }
}
