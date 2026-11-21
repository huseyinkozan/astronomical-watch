#include "OpticsParts.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Language.h"
#include "Optics.h"
#include "Styles.h"
#include "esp_attr.h"

enum Calculator
{
    SharpStars,
    NdFilter,
    StarTrails,
    CalculatorCount
};

// Kept over the always-on wakes.
RTC_DATA_ATTR static int s_eyepiece = 0;
RTC_DATA_ATTR static int s_calculator = SharpStars;

static const int s_rowCount = 8;
static const int s_hintRow = 7;
static const uint32_t s_dimColour = 0x5A5A5A;
// The photo part's tap area: its title and inputs.
static const int s_titleTapHeight = 92;

static const Settings::Item s_eyepieceFocals[4] = { Settings::Eyepiece1Focal, Settings::Eyepiece2Focal,
                                                    Settings::Eyepiece3Focal, Settings::Eyepiece4Focal };
static const Settings::Item s_eyepieceFields[4] = { Settings::Eyepiece1Field, Settings::Eyepiece2Field,
                                                    Settings::Eyepiece3Field, Settings::Eyepiece4Field };

static TablePart::Layout layout( bool eyepiece )
{
    TablePart::Layout result = {};
    result.title = "";
    result.rowCount = s_rowCount;
    for ( int i = 0; i < s_rowCount; ++i ) {
        const int lastValue = eyepiece ? 6 : 4;
        result.kinds[i] = i >= 2 && i <= lastValue ? TablePart::OneValue : TablePart::Line;
    }
    // Lower than the other parts' titles: the first line is the longest ("Teleskop 1000 mm f/6.3").
    result.top = 52;
    result.titleGap = 0;
    result.pitch = 46;
    result.nameX = 40;
    // The eyepiece's names are longer, the photo's values ("17dk 04sn").
    result.nameWidth = eyepiece ? 205 : 188;
    result.valueX = eyepiece ? 245 : 228;
    result.valueWidth = eyepiece ? 62 : 71;
    return result;
}

static lv_obj_t *tapArea( lv_obj_t *part, int height, lv_event_cb_t onTap, void *user )
{
    lv_obj_t *area = lv_obj_create( part );
    lv_obj_remove_style_all( area );
    lv_obj_set_pos( area, 0, 40 );
    lv_obj_set_size( area, LV_PCT( 100 ), height );
    lv_obj_set_scrollable( area, false );
    lv_obj_add_event_cb( area, onTap, LV_EVENT_CLICKED, user );
    return area;
}

// "25" or "6.5" from tenths.
static void tenths( int value, char *text, size_t size )
{
    if ( value % 10 == 0 ) {
        snprintf( text, size, "%d", value / 10 );
    } else {
        snprintf( text, size, "%d.%d", value / 10, value % 10 );
    }
}

// An exposure: "1/60 s", "8.0 s", "17 s", "4m 16s", "2h 05m".
static void duration( double seconds, char *text, size_t size )
{
    if ( seconds < 0.95 ) {
        snprintf( text, size, "1/%d s", static_cast<int>( std::lround( 1.0 / seconds ) ) );
    } else if ( seconds < 9.95 ) {
        snprintf( text, size, "%.1f s", std::round( seconds * 10.0 ) / 10.0 );
    } else if ( seconds < 59.5 ) {
        snprintf( text, size, "%d s", static_cast<int>( std::lround( seconds ) ) );
    } else if ( seconds < 3600 ) {
        const int whole = static_cast<int>( std::lround( seconds ) );
        snprintf( text, size, Language::text( Language::DurationMinSec ), whole / 60, whole % 60 );
    } else {
        const int minutes = static_cast<int>( std::lround( seconds / 60.0 ) );
        snprintf( text, size, Language::text( Language::DurationHourMin ), minutes / 60, minutes % 60 );
    }
}

// A whole base shutter speed without its tenth: "1/60 s", "2 s".
static void shutter( double seconds, char *text, size_t size )
{
    if ( seconds >= 1.0 ) {
        snprintf( text, size, "%d s", static_cast<int>( std::lround( seconds ) ) );
    } else {
        duration( seconds, text, size );
    }
}

void OpticsParts::build( lv_obj_t *eyepiece, lv_obj_t *photo, Settings &settings )
{
    m_settings = &settings;
    m_eyepieceTable.build( eyepiece, layout( true ) );
    m_photoTable.build( photo, layout( false ) );
    // Over the tables: a tap anywhere on the eyepiece part, on the photo part's title.
    m_eyepieceTap = tapArea( eyepiece, 380, onTap, this );
    m_photoTap = tapArea( photo, s_titleTapHeight, onTap, this );
    refresh();
}

void OpticsParts::forget()
{
    m_eyepieceTable.forget();
    m_photoTable.forget();
    m_eyepieceTap = nullptr;
    m_photoTap = nullptr;
    m_settings = nullptr;
}

void OpticsParts::refresh()
{
    if ( m_settings == nullptr ) {
        return;
    }
    showEyepiece();
    showPhoto();
}

void OpticsParts::onTap( lv_event_t *event )
{
    OpticsParts *self = static_cast<OpticsParts *>( lv_event_get_user_data( event ) );
    lv_indev_t *indev = lv_indev_active();
    if ( indev != nullptr && lv_indev_get_gesture_dir( indev ) != LV_DIR_NONE ) {
        return;
    }
    const lv_obj_t *target = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    self->press( target == self->m_eyepieceTap ? "eyepiece" : "calc" );
}

void OpticsParts::showEyepiece()
{
    TableText &table = m_eyepieceTable.text();
    const int focal = m_settings->value( Settings::ScopeFocal );
    const int aperture = m_settings->value( Settings::ScopeAperture );
    char ratio[12];
    tenths( static_cast<int>( std::lround( Optics::focalRatio( focal, aperture ) * 10.0 ) ), ratio, sizeof( ratio ) );
    char text[48];
    snprintf( text, sizeof( text ), Language::text( Language::ScopeValue ), focal, ratio );
    table.setLine( 0, text );
    table.setColours( 0, TableText::Grey, TableText::Grey );
    // The eyepieces set, from the one shown on.
    int index = -1;
    for ( int i = 0; i < 4 && index < 0; ++i ) {
        const int candidate = ( s_eyepiece + i ) % 4;
        if ( m_settings->value( s_eyepieceFocals[candidate] ) > 0 ) {
            index = candidate;
        }
    }
    static const Language::Text names[5] = { Language::Magnification, Language::ExitPupil, Language::TrueField,
                                             Language::LimitMagnitude, Language::Resolution };
    for ( int i = 0; i < 5; ++i ) {
        table.set( 2 + i, 0, Language::text( names[i] ) );
        table.setColours( 2 + i, TableText::Grey, TableText::White );
    }
    table.setColours( 1, Styles::Amber, Styles::Amber );
    table.setLine( s_hintRow, Language::text( Language::EyepieceHint ) );
    table.setColours( s_hintRow, s_dimColour, s_dimColour );
    if ( index < 0 ) {
        table.setLine( 1, Language::text( Language::NoEyepiece ) );
        for ( int i = 0; i < 5; ++i ) {
            table.set( 2 + i, 1, "-" );
        }
        m_eyepieceTable.refresh();
        return;
    }
    s_eyepiece = index;
    const int eyepieceTenths = m_settings->value( s_eyepieceFocals[index] );
    const int field = m_settings->value( s_eyepieceFields[index] );
    char length[12];
    tenths( eyepieceTenths, length, sizeof( length ) );
    snprintf( text, sizeof( text ), Language::text( Language::EyepieceValue ), length, field );
    table.setLine( 1, text );
    const double magnification = Optics::magnification( focal, eyepieceTenths / 10.0 );
    snprintf( text, sizeof( text ), "%d×", static_cast<int>( std::lround( magnification ) ) );
    table.set( 2, 1, text );
    snprintf( text, sizeof( text ), "%.1f mm", Optics::exitPupil( aperture, magnification ) );
    table.set( 3, 1, text );
    const double trueField = Optics::trueField( field, magnification );
    snprintf( text, sizeof( text ), trueField < 10 ? "%.2f°" : "%.1f°", trueField );
    table.set( 4, 1, text );
    snprintf( text, sizeof( text ), "%.1f", Optics::limitingMagnitude( aperture ) );
    table.set( 5, 1, text );
    snprintf( text, sizeof( text ), "%.2f\"", Optics::dawesLimit( aperture ) );
    table.set( 6, 1, text );
    m_eyepieceTable.refresh();
}

void OpticsParts::showPhoto()
{
    TableText &table = m_photoTable.text();
    static const Language::Text titles[CalculatorCount] = { Language::PhotoSharp, Language::PhotoNd,
                                                             Language::PhotoTrails };
    s_calculator = s_calculator < 0 || s_calculator >= CalculatorCount ? SharpStars : s_calculator;
    table.setLine( 0, Language::text( titles[s_calculator] ) );
    table.setColours( 0, TableText::Grey, TableText::Grey );
    table.setColours( 1, Styles::Amber, Styles::Amber );
    for ( int i = 2; i <= 4; ++i ) {
        table.set( i, 0, nullptr );
        table.set( i, 1, nullptr );
        table.setColours( i, TableText::Grey, TableText::White );
    }
    table.setLine( s_hintRow, Language::text( Language::PhotoHint ) );
    table.setColours( s_hintRow, s_dimColour, s_dimColour );
    char text[48];
    char value[24];
    if ( s_calculator == SharpStars ) {
        const int focal = m_settings->value( Settings::CameraFocal );
        const int fTenths = m_settings->value( Settings::CameraFNumber );
        const Optics::Sensor &sensor = Optics::Sensors[m_settings->value( Settings::CameraSensor )];
        char fNumber[12];
        tenths( fTenths, fNumber, sizeof( fNumber ) );
        snprintf( text, sizeof( text ), "%d mm f/%s %s", focal, fNumber, sensor.name );
        table.setLine( 1, text );
        table.set( 2, 0, Language::text( Language::Rule500 ) );
        duration( Optics::rule500( focal, sensor.crop ), value, sizeof( value ) );
        table.set( 2, 1, value );
        table.set( 3, 0, Language::text( Language::RuleNpf ) );
        duration( Optics::npfRule( focal, fTenths / 10.0, sensor.pixelPitch ), value, sizeof( value ) );
        table.set( 3, 1, value );
        table.set( 4, 0, Language::text( Language::EquivalentFocal ) );
        snprintf( value, sizeof( value ), "%d mm", static_cast<int>( std::lround( focal * sensor.crop ) ) );
        table.set( 4, 1, value );
    } else if ( s_calculator == NdFilter ) {
        const double base = Optics::shutterSeconds( m_settings->value( Settings::BaseShutter ) );
        const int stops = m_settings->value( Settings::NdStops );
        char speed[24];
        shutter( base, speed, sizeof( speed ) );
        snprintf( text, sizeof( text ), "%s + ND %d", speed, stops );
        table.setLine( 1, text );
        table.set( 2, 0, Language::text( Language::NdFactor ) );
        snprintf( value, sizeof( value ), "%ld×", static_cast<long>( 1L << stops ) );
        table.set( 2, 1, value );
        table.set( 3, 0, Language::text( Language::NdExposure ) );
        duration( Optics::ndExposure( base, stops ), value, sizeof( value ) );
        table.set( 3, 1, value );
    } else {
        const int minutes = m_settings->value( Settings::TrailMinutes );
        const int interval = m_settings->value( Settings::TrailInterval );
        snprintf( text, sizeof( text ), Language::text( Language::TrailInputs ), minutes, interval );
        table.setLine( 1, text );
        table.set( 2, 0, Language::text( Language::TrailFrames ) );
        snprintf( value, sizeof( value ), "%d", Optics::trailFrames( minutes, interval ) );
        table.set( 2, 1, value );
        table.set( 3, 0, Language::text( Language::TrailArc ) );
        snprintf( value, sizeof( value ), "%.1f°", Optics::trailArc( minutes ) );
        table.set( 3, 1, value );
    }
    m_photoTable.refresh();
}

bool OpticsParts::press( const char *name )
{
    if ( m_settings == nullptr ) {
        return false;
    }
    if ( strcmp( name, "eyepiece" ) == 0 ) {
        s_eyepiece = ( s_eyepiece + 1 ) % 4;
        showEyepiece();
    } else if ( strcmp( name, "calc" ) == 0 ) {
        s_calculator = ( s_calculator + 1 ) % CalculatorCount;
        showPhoto();
    } else {
        return false;
    }
    return true;
}
