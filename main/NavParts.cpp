#include "NavParts.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Aiming.h"
#include "Almanac.h"
#include "Language.h"
#include "Magnetic.h"
#include "Styles.h"
#include "esp_attr.h"

enum AimMode
{
    DishMode,
    PanelMode,
    AimModeCount
};

// Kept over the always-on wakes.
RTC_DATA_ATTR static int s_body = Almanac::Sun;
RTC_DATA_ATTR static int s_aimMode = DishMode;
RTC_DATA_ATTR static int s_satellite = 0;

struct Satellite
{
    const char *name;
    int longitude;  // tenths of a degree, east positive
};
static const Satellite s_satellites[] = { { "Türksat", 420 }, { "Türksat", 310 }, { "Astra", 192 },
                                          { "Hotbird", 130 }, { "Astra", 282 },   { "Nilesat", -70 } };
static const int s_satelliteCount = sizeof( s_satellites ) / sizeof( s_satellites[0] );

static const int s_rowCount = 8;
static const int s_hintRow = 7;
static const uint32_t s_dimColour = 0x5A5A5A;
static const int64_t s_dayS = 86400;
// The tap areas: the almanac's whole part; the aiming part's title row and the row under it.
static const int s_tapTop = 40;
static const int s_titleTapHeight = 54;
static const int s_satelliteTapHeight = 46;

static const Language::Text s_bodyNames[Almanac::BodyCount] = { Language::Sun, Language::Moon,
                                                                Language::PlanetVenus, Language::PlanetMars,
                                                                Language::PlanetJupiter, Language::PlanetSaturn };

static TablePart::Layout layout( bool almanac )
{
    TablePart::Layout result = {};
    result.title = "";
    result.rowCount = s_rowCount;
    for ( int i = 0; i < s_rowCount; ++i ) {
        const int first = almanac ? 1 : 2;
        result.kinds[i] = i >= first && i <= 6 ? TablePart::OneValue : TablePart::Line;
    }
    // As the eyepiece and photo parts (OpticsParts).
    result.top = 52;
    result.titleGap = 0;
    result.pitch = 46;
    result.nameX = 40;
    // The almanac's values are the longer ("K 23° 26.2'"), the aiming part's names.
    result.nameWidth = almanac ? 150 : 220;
    result.valueX = almanac ? 190 : 260;
    result.valueWidth = almanac ? 90 : 55;
    return result;
}

static lv_obj_t *tapArea( lv_obj_t *part, int y, int height, lv_event_cb_t onTap, void *user )
{
    lv_obj_t *area = lv_obj_create( part );
    lv_obj_remove_style_all( area );
    lv_obj_set_pos( area, 0, y );
    lv_obj_set_size( area, LV_PCT( 100 ), height );
    lv_obj_set_scrollable( area, false );
    lv_obj_add_event_cb( area, onTap, LV_EVENT_CLICKED, user );
    return area;
}

// "6.1° E" or "6.1° W" (the language's letters).
static void variation( double degrees, char *text, size_t size )
{
    const double rounded = std::round( std::fabs( degrees ) * 10.0 ) / 10.0;
    const char *letter = rounded == 0 ? "" : Language::text( degrees > 0 ? Language::EastLetter : Language::WestLetter );
    snprintf( text, size, "%.1f° %s", rounded, letter );
}

static double normalised( double degrees )
{
    const double value = std::fmod( degrees, 360.0 );
    return value < 0 ? value + 360.0 : value;
}

void NavParts::build( lv_obj_t *almanac, lv_obj_t *aiming, Settings &settings )
{
    m_settings = &settings;
    m_almanacTable.build( almanac, layout( true ) );
    m_aimingTable.build( aiming, layout( false ) );
    m_bodyTap = tapArea( almanac, s_tapTop, 380, onTapEvent, this );
    m_modeTap = tapArea( aiming, s_tapTop, s_titleTapHeight, onTapEvent, this );
    m_satelliteTap = tapArea( aiming, s_tapTop + s_titleTapHeight, s_satelliteTapHeight, onTapEvent, this );
    m_aimingMinute = -1;
    m_aimingSatellite = -999;
    showAlmanac();
    showAiming();
}

void NavParts::forget()
{
    m_almanacTable.forget();
    m_aimingTable.forget();
    m_bodyTap = nullptr;
    m_modeTap = nullptr;
    m_satelliteTap = nullptr;
    m_settings = nullptr;
    m_shown = -1;
}

void NavParts::setShown( int part )
{
    if ( part == 1 && m_shown != 1 ) {
        // The portal may have set another satellite meanwhile.
        m_aimingMinute = -1;
    }
    m_shown = part;
    if ( m_settings == nullptr ) {
        return;
    }
    if ( part == 0 ) {
        showAlmanac();
    } else if ( part == 1 ) {
        showAiming();
    }
}

void NavParts::show( int64_t now, const Astronomy::Observer &observer, int utcOffsetMinutes, bool located )
{
    m_now = now;
    m_observer = observer;
    m_utcOffsetMinutes = utcOffsetMinutes;
    m_located = located;
    if ( m_settings == nullptr ) {
        return;
    }
    if ( m_shown == 0 ) {
        showAlmanac();
    } else if ( m_shown == 1 && now / 60 != m_aimingMinute ) {
        showAiming();
    }
}

void NavParts::onTapEvent( lv_event_t *event )
{
    NavParts *self = static_cast<NavParts *>( lv_event_get_user_data( event ) );
    lv_indev_t *indev = lv_indev_active();
    if ( indev != nullptr && lv_indev_get_gesture_dir( indev ) != LV_DIR_NONE ) {
        return;
    }
    const lv_obj_t *target = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    self->press( target == self->m_bodyTap ? "body" : ( target == self->m_modeTap ? "aim" : "satellite" ) );
}

int64_t NavParts::localMidnight() const
{
    const int64_t offset = static_cast<int64_t>( m_utcOffsetMinutes ) * 60;
    const int64_t local = m_now + offset;
    // Floor division: a local time before 1970 is not expected, but keep it right.
    const int64_t day = local >= 0 ? local / s_dayS : ( local - s_dayS + 1 ) / s_dayS;
    return day * s_dayS - offset;
}

void NavParts::localTime( int64_t time, bool seconds, char *text, size_t size ) const
{
    const int64_t local = time + static_cast<int64_t>( m_utcOffsetMinutes ) * 60;
    const int second = static_cast<int>( ( local % s_dayS + s_dayS ) % s_dayS );
    if ( seconds ) {
        snprintf( text, size, "%02d:%02d:%02d", second / 3600, second / 60 % 60, second % 60 );
    } else {
        snprintf( text, size, "%02d:%02d", second / 3600, second / 60 % 60 );
    }
}

void NavParts::showAlmanac()
{
    TableText &table = m_almanacTable.text();
    s_body = s_body < 0 || s_body >= Almanac::BodyCount ? Almanac::Sun : s_body;
    const Almanac::Body body = static_cast<Almanac::Body>( s_body );
    const int second = static_cast<int>( ( m_now % s_dayS + s_dayS ) % s_dayS );
    char text[48];
    snprintf( text, sizeof( text ), "%s %02d:%02d:%02d UTC", Language::text( s_bodyNames[body] ), second / 3600,
              second / 60 % 60, second % 60 );
    table.setLine( 0, text );
    table.setColours( 0, Styles::Amber, Styles::Amber );
    const Almanac::Place place = Almanac::place( body, m_now );
    // The rows: GHA and declination, then per body.
    Language::Text names[5] = { Language::AlmanacGha, Language::AlmanacDec, Language::AlmanacSd, Language::AlmanacNoon,
                                Language::AlmanacNoonAltitude };
    if ( body == Almanac::Moon ) {
        names[3] = Language::AlmanacHp;
        names[4] = Language::AlmanacPass;
    } else if ( body != Almanac::Sun ) {
        names[2] = Language::AlmanacSha;
        names[3] = Language::AlmanacPass;
    }
    const int rows = body == Almanac::Sun || body == Almanac::Moon ? 5 : 4;
    char values[5][32] = {};
    Almanac::formatAngle( place.gha, values[0], sizeof( values[0] ) );
    Almanac::formatDeclination( place.declination, Language::text( Language::North ),
                                Language::text( Language::SouthLetter ), values[1], sizeof( values[1] ) );
    if ( body == Almanac::Sun || body == Almanac::Moon ) {
        snprintf( values[2], sizeof( values[2] ), "%.1f'", place.semiDiameter );
    } else {
        Almanac::formatAngle( place.sha, values[2], sizeof( values[2] ) );
    }
    if ( body == Almanac::Moon ) {
        snprintf( values[3], sizeof( values[3] ), "%.1f'", place.parallax );
    }
    // The meridian passage of the local day at the location: the Sun's noon, the others' pass.
    int64_t pass = 0;
    const bool passes = m_located && Almanac::meridianPassage( body, localMidnight(), m_observer.longitude, pass );
    char *passText = body == Almanac::Sun ? values[3] : ( body == Almanac::Moon ? values[4] : values[3] );
    if ( passes ) {
        localTime( pass, body == Almanac::Sun, passText, sizeof( values[0] ) );
    } else {
        snprintf( passText, sizeof( values[0] ), "-" );
    }
    if ( body == Almanac::Sun ) {
        const double altitude =
            passes ? Almanac::meridianAltitude( m_observer.latitude, Almanac::place( body, pass ).declination ) : -1;
        if ( altitude >= 0 ) {
            Almanac::formatAngle( altitude, values[4], sizeof( values[4] ) );
        } else {
            snprintf( values[4], sizeof( values[4] ), "-" );
        }
    }
    for ( int i = 0; i < 5; ++i ) {
        table.set( 1 + i, 0, i < rows ? Language::text( names[i] ) : nullptr );
        table.set( 1 + i, 1, i < rows ? values[i] : nullptr );
        table.setColours( 1 + i, TableText::Grey, TableText::White );
    }
    table.setLine( s_hintRow, Language::text( Language::AlmanacHint ) );
    table.setColours( s_hintRow, s_dimColour, s_dimColour );
    m_almanacTable.refresh();
}

static bool isPreset( int tenths )
{
    for ( int i = 0; i < s_satelliteCount; ++i ) {
        if ( s_satellites[i].longitude == tenths ) {
            return true;
        }
    }
    return false;
}

int NavParts::satelliteCount() const
{
    return s_satelliteCount + ( isPreset( m_settings->value( Settings::SatelliteLongitude ) ) ? 0 : 1 );
}

double NavParts::satellite( char *name, size_t size ) const
{
    const int own = m_settings->value( Settings::SatelliteLongitude );
    const int count = satelliteCount();
    const int index = ( s_satellite % count + count ) % count;
    const bool custom = count > s_satelliteCount && index == 0;
    const int preset = count > s_satelliteCount ? index - 1 : index;
    const int tenths = custom ? own : s_satellites[preset].longitude;
    const char *label = custom ? Language::text( Language::AimSatellite ) : s_satellites[preset].name;
    const int magnitude = tenths < 0 ? -tenths : tenths;
    const char *letter = "";
    if ( magnitude != 0 && magnitude != 1800 ) {
        letter = Language::text( tenths > 0 ? Language::EastLetter : Language::WestLetter );
    }
    snprintf( name, size, "%s %d.%d° %s", label, magnitude / 10, magnitude % 10, letter );
    return tenths / 10.0;
}

void NavParts::showAiming()
{
    m_aimingMinute = m_now / 60;
    TableText &table = m_aimingTable.text();
    s_aimMode = s_aimMode < 0 || s_aimMode >= AimModeCount ? DishMode : s_aimMode;
    const bool dish = s_aimMode == DishMode;
    table.setLine( 0, Language::text( dish ? Language::AimDish : Language::AimPanel ) );
    table.setColours( 0, TableText::Grey, TableText::Grey );
    table.setColours( 1, Styles::Amber, Styles::Amber );
    table.setLine( s_hintRow, Language::text( Language::AimHint ) );
    table.setColours( s_hintRow, s_dimColour, s_dimColour );
    char name[40];
    const double longitude = satellite( name, sizeof( name ) );
    lv_obj_set_hidden( m_satelliteTap, !dish );
    if ( !m_located ) {
        table.setLine( 1, Language::text( Language::NoLocation ) );
        for ( int i = 2; i <= 6; ++i ) {
            table.set( i, 0, nullptr );
            table.set( i, 1, nullptr );
        }
        m_aimingTable.refresh();
        return;
    }
    // The variation and the Sun's time again for a new day, place or satellite.
    const int64_t midnight = localMidnight();
    const bool samePlace = m_observer.latitude == m_aimingObserver.latitude &&
                           m_observer.longitude == m_aimingObserver.longitude;
    const Astronomy::Position pointing = Aiming::dish( m_observer, longitude );
    if ( midnight != m_aimingDay || !samePlace || longitude != m_aimingSatellite ) {
        m_aimingDay = midnight;
        m_aimingObserver = m_observer;
        m_aimingSatellite = longitude;
        m_variation = Magnetic::declination( m_observer.latitude, m_observer.longitude, Magnetic::decimalYear( m_now ) );
        m_sunInLine = pointing.altitude > 0 && Aiming::sunAtAzimuth( midnight, m_observer, pointing.azimuth, m_sunTime );
    }
    Language::Text names[5] = { Language::AimElevation, Language::AimAzimuth, Language::AimVariation,
                                Language::AimCompass, Language::AimSun };
    char values[5][24] = {};
    double azimuth = 0;
    if ( dish ) {
        table.setLine( 1, name );
        azimuth = pointing.azimuth;
        snprintf( values[0], sizeof( values[0] ), "%.1f°", pointing.altitude );
        if ( m_sunInLine ) {
            localTime( m_sunTime, false, values[4], sizeof( values[4] ) );
        } else {
            snprintf( values[4], sizeof( values[4] ), "-" );
        }
    } else {
        table.setLine( 1, Language::text( m_observer.latitude >= 0 ? Language::AimFacingSouth
                                                                   : Language::AimFacingNorth ) );
        azimuth = Aiming::panelAzimuth( m_observer.latitude );
        names[0] = Language::AimYearTilt;
        names[4] = Language::AimDayTilt;
        snprintf( values[0], sizeof( values[0] ), "%.1f°", Aiming::yearTilt( m_observer.latitude ) );
        int64_t noon = 0;
        if ( Almanac::meridianPassage( Almanac::Sun, midnight, m_observer.longitude, noon ) ) {
            const double declination = Almanac::place( Almanac::Sun, noon ).declination;
            const double altitude = Almanac::meridianAltitude( m_observer.latitude, declination );
            // Facing the Sun at noon: the panel's side of the zenith is the Sun's.
            const bool sameSide = ( m_observer.latitude >= declination ) == ( m_observer.latitude >= 0 );
            if ( altitude > 0 ) {
                snprintf( values[4], sizeof( values[4] ), sameSide ? "%.1f°" : "-%.1f°", Aiming::dayTilt( altitude ) );
            } else {
                snprintf( values[4], sizeof( values[4] ), "-" );
            }
        } else {
            snprintf( values[4], sizeof( values[4] ), "-" );
        }
    }
    snprintf( values[1], sizeof( values[1] ), "%.1f°", azimuth );
    variation( m_variation, values[2], sizeof( values[2] ) );
    // The compass shows the true bearing less the variation (east positive).
    snprintf( values[3], sizeof( values[3] ), "%.1f°", normalised( azimuth - m_variation ) );
    for ( int i = 0; i < 5; ++i ) {
        table.set( 2 + i, 0, Language::text( names[i] ) );
        table.set( 2 + i, 1, values[i] );
        table.setColours( 2 + i, TableText::Grey, TableText::White );
    }
    m_aimingTable.refresh();
}

bool NavParts::press( const char *name )
{
    if ( m_settings == nullptr ) {
        return false;
    }
    if ( strcmp( name, "body" ) == 0 ) {
        s_body = ( s_body + 1 ) % Almanac::BodyCount;
        showAlmanac();
    } else if ( strcmp( name, "aim" ) == 0 ) {
        s_aimMode = ( s_aimMode + 1 ) % AimModeCount;
        showAiming();
    } else if ( strcmp( name, "satellite" ) == 0 ) {
        s_satellite = ( s_satellite + 1 ) % satelliteCount();
        showAiming();
    } else {
        return false;
    }
    return true;
}
