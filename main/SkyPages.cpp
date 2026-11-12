#include "SkyPages.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <ctime>

static const int s_compassCentreX = 205;
static const int s_compassCentreY = 230;
static const int s_arrowLength = 110;

static const char *s_phaseNames[8] = { "New Moon",  "Waxing crescent", "First quarter", "Waxing gibbous",
                                       "Full Moon", "Waning gibbous",  "Last quarter",  "Waning crescent" };
static const char *s_hijriMonths[12] = { "Muharram", "Safar",   "Rabi I",  "Rabi II",  "Jumada I", "Jumada II",
                                         "Rajab",    "Shaban",  "Ramadan", "Shawwal",  "Dhu al-Qadah",
                                         "Dhu al-Hijjah" };
static const char *s_seasonNames[4] = { "Equinox", "Solstice", "Equinox", "Solstice" };
static const char *s_monthNames[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static const int s_nameWidth = 160;
static const int s_valueWidth = 105;
static const lv_color_t s_grey = LV_COLOR_MAKE( 0xA0, 0xA0, 0xA0 );

// A page of lines in a centred column: the title, then rows or full-width lines.
static void preparePage( lv_obj_t *page, const char *title )
{
    lv_obj_set_flex_flow( page, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_flex_align( page, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_row( page, 6, 0 );
    lv_obj_set_scrollbar_mode( page, LV_SCROLLBAR_MODE_OFF );
    lv_obj_t *heading = lv_label_create( page );
    lv_obj_set_style_text_font( heading, &lv_font_montserrat_28, 0 );
    lv_obj_set_style_text_color( heading, s_grey, 0 );
    lv_label_set_text( heading, title );
}

static lv_obj_t *createCell( lv_obj_t *parent, int width, lv_color_t color, lv_text_align_t align, const char *text )
{
    lv_obj_t *label = lv_label_create( parent );
    lv_obj_set_width( label, width );
    lv_obj_set_style_text_font( label, &lv_font_montserrat_28, 0 );
    lv_obj_set_style_text_color( label, color, 0 );
    lv_obj_set_style_text_align( label, align, 0 );
    lv_label_set_long_mode( label, LV_LABEL_LONG_MODE_CLIP );
    lv_label_set_text( label, text );
    return label;
}

// A full-width centred line.
static lv_obj_t *createLine( lv_obj_t *page )
{
    return createCell( page, s_nameWidth + 2 * s_valueWidth, lv_color_white(), LV_TEXT_ALIGN_CENTER, "" );
}

// LVGL's own printf has no floating point (LV_SPRINTF_USE_FLOAT): format with the C library.
static void setText( lv_obj_t *label, const char *format, ... )
{
    char text[256];
    va_list arguments;
    va_start( arguments, format );
    vsnprintf( text, sizeof( text ), format, arguments );
    va_end( arguments );
    lv_label_set_text( label, text );
}

// "HH:MM" of a Unix time in local time, or "--:--".
static void formatTime( char *text, size_t size, bool occurs, int64_t time, int utcOffsetMinutes )
{
    if ( !occurs ) {
        snprintf( text, size, "--:--" );
        return;
    }
    int64_t seconds = ( time + static_cast<int64_t>( utcOffsetMinutes ) * 60 ) % 86400;
    if ( seconds < 0 ) {
        seconds += 86400;
    }
    // Rounded to the nearest minute.
    const int minutes = static_cast<int>( ( seconds + 30 ) / 60 ) % ( 24 * 60 );
    snprintf( text, size, "%02d:%02d", minutes / 60, minutes % 60 );
}

// "H:MM" of a duration.
static void formatDuration( char *text, size_t size, int seconds )
{
    const int minutes = ( seconds + 30 ) / 60;
    snprintf( text, size, "%d:%02d", minutes / 60, minutes % 60 );
}

SkyPages::Row SkyPages::createRow( lv_obj_t *page, const char *name, bool twoValues )
{
    lv_obj_t *row = lv_obj_create( page );
    lv_obj_remove_style_all( row );
    lv_obj_set_clickable( row, false );
    lv_obj_set_scrollable( row, false );
    lv_obj_set_size( row, s_nameWidth + 2 * s_valueWidth, LV_SIZE_CONTENT );
    lv_obj_set_flex_flow( row, LV_FLEX_FLOW_ROW );
    Row cells = {};
    cells.name = createCell( row, s_nameWidth, s_grey, LV_TEXT_ALIGN_LEFT, name );
    const int firstWidth = twoValues ? s_valueWidth : 2 * s_valueWidth;
    cells.values[0] = createCell( row, firstWidth, lv_color_white(), LV_TEXT_ALIGN_RIGHT, "" );
    if ( twoValues ) {
        cells.values[1] = createCell( row, s_valueWidth, lv_color_white(), LV_TEXT_ALIGN_RIGHT, "" );
    }
    return cells;
}

void SkyPages::build( lv_obj_t *sunPage, lv_obj_t *twilightPage, lv_obj_t *moonPage, lv_obj_t *compassPage )
{
    preparePage( sunPage, "Sun" );
    const char *sunNames[SunRows] = { "Rise", "Set", "Noon", "Day", "", "Now" };
    for ( int i = 0; i < SunRows; ++i ) {
        m_sun[i] = createRow( sunPage, sunNames[i], true );
    }

    preparePage( twilightPage, "Twilight" );
    const Row header = createRow( twilightPage, "", true );
    lv_label_set_text( header.values[0], "Dawn" );
    lv_label_set_text( header.values[1], "Dusk" );
    lv_obj_set_style_text_color( header.values[0], s_grey, 0 );
    lv_obj_set_style_text_color( header.values[1], s_grey, 0 );
    const char *twilightNames[TwilightRows] = { "Astro", "Nautical", "Civil", "Blue hour",
                                                "  until", "Golden", "  until", "" };
    for ( int i = 0; i < TwilightRows; ++i ) {
        m_twilight[i] = createRow( twilightPage, twilightNames[i], i < TwilightRows - 1 );
    }

    preparePage( moonPage, "Moon" );
    m_moonPhase = createLine( moonPage );
    const char *moonNames[MoonRows] = { "Lit", "Rise", "Set", "Now", "Tides", "Dark sky", "Crescent" };
    const bool moonTwoValues[MoonRows] = { true, true, true, true, false, true, false };
    for ( int i = 0; i < MoonRows; ++i ) {
        m_moon[i] = createRow( moonPage, moonNames[i], moonTwoValues[i] );
    }
    m_hijri = createLine( moonPage );

    lv_obj_set_scrollbar_mode( compassPage, LV_SCROLLBAR_MODE_OFF );
    lv_obj_t *heading = lv_label_create( compassPage );
    lv_obj_set_style_text_font( heading, &lv_font_montserrat_28, 0 );
    lv_obj_set_style_text_color( heading, lv_color_hex( 0xA0A0A0 ), 0 );
    lv_label_set_text( heading, "Sun compass" );
    lv_obj_align( heading, LV_ALIGN_TOP_MID, 0, 40 );
    m_compassArrow = lv_line_create( compassPage );
    lv_obj_set_style_line_width( m_compassArrow, 8, 0 );
    lv_obj_set_style_line_color( m_compassArrow, lv_color_white(), 0 );
    lv_obj_set_style_line_rounded( m_compassArrow, true, 0 );
    m_compassNorth = lv_label_create( compassPage );
    lv_obj_set_style_text_font( m_compassNorth, &lv_font_montserrat_28, 0 );
    lv_obj_set_style_text_color( m_compassNorth, lv_color_white(), 0 );
    lv_label_set_text( m_compassNorth, "N" );
    m_compassText = lv_label_create( compassPage );
    lv_obj_set_style_text_font( m_compassText, &lv_font_montserrat_28, 0 );
    lv_obj_set_style_text_color( m_compassText, lv_color_hex( 0xA0A0A0 ), 0 );
    lv_obj_set_style_text_align( m_compassText, LV_TEXT_ALIGN_CENTER, 0 );
    lv_obj_align( m_compassText, LV_ALIGN_BOTTOM_MID, 0, -40 );
}

// An event's local time in a cell.
static void setTime( lv_obj_t *label, const Astronomy::Event &event, int utcOffsetMinutes )
{
    char text[8];
    formatTime( text, sizeof( text ), event.occurs, event.time, utcOffsetMinutes );
    lv_label_set_text( label, text );
}

void SkyPages::show( const Sky::Day &day, const Sky::Now &now, int utcOffsetMinutes )
{
    if ( m_hijri == nullptr ) {
        return;
    }
    const Astronomy::SunDay &sun = day.sun;
    char text[24];
    setTime( m_sun[0].values[0], sun.riseSet.rising, utcOffsetMinutes );
    setTime( m_sun[1].values[0], sun.riseSet.setting, utcOffsetMinutes );
    setTime( m_sun[2].values[0], sun.noon, utcOffsetMinutes );
    setText( m_sun[2].values[1], "%.0f\xC2\xB0", sun.noonAltitude );
    formatDuration( text, sizeof( text ), sun.dayLengthSeconds );
    lv_label_set_text( m_sun[3].values[0], text );
    const int change = day.dayLengthChangeSeconds;
    const int changeAbs = change < 0 ? -change : change;
    setText( m_sun[3].values[1], "%c%d:%02d", change < 0 ? '-' : '+', changeAbs / 60, changeAbs % 60 );
    lv_label_set_text( m_sun[4].name, now.sunUp ? "Sunset in" : "Sunrise in" );
    formatDuration( text, sizeof( text ), now.secondsToSunEvent );
    lv_label_set_text( m_sun[4].values[0], now.secondsToSunEvent > 0 ? text : "--" );
    lv_label_set_text( m_sun[4].values[1], "" );
    setText( m_sun[5].values[0], "%.0f\xC2\xB0", now.sun.altitude );
    setText( m_sun[5].values[1], "%.0f\xC2\xB0", now.sun.azimuth );

    // Twilight: when each limit is crossed at dawn and at dusk; the blue hour runs from -6 to -4
    // degrees, the golden hour from -4 to +6.
    const Astronomy::Event *dawn[TwilightRows - 1] = { &sun.astronomical.rising, &sun.nautical.rising,
                                                          &sun.civil.rising,        &sun.civil.rising,
                                                          &sun.blueHour.rising,     &sun.blueHour.rising,
                                                          &sun.goldenHour.rising };
    const Astronomy::Event *dusk[TwilightRows - 1] = { &sun.astronomical.setting, &sun.nautical.setting,
                                                          &sun.civil.setting,        &sun.blueHour.setting,
                                                          &sun.civil.setting,        &sun.goldenHour.setting,
                                                          &sun.blueHour.setting };
    for ( int i = 0; i < TwilightRows - 1; ++i ) {
        setTime( m_twilight[i].values[0], *dawn[i], utcOffsetMinutes );
        setTime( m_twilight[i].values[1], *dusk[i], utcOffsetMinutes );
    }
    const time_t season = static_cast<time_t>( day.nextSeason + static_cast<int64_t>( utcOffsetMinutes ) * 60 );
    tm seasonDate = {};
    gmtime_r( &season, &seasonDate );
    lv_label_set_text( m_twilight[TwilightRows - 1].name, s_seasonNames[day.season] );
    setText( m_twilight[TwilightRows - 1].values[0], "%s %d", s_monthNames[seasonDate.tm_mon], seasonDate.tm_mday );

    // Moon.
    lv_label_set_text( m_moonPhase, s_phaseNames[Sky::phaseIndex( day.phase )] );
    setText( m_moon[0].values[0], "%.0f %%", day.phase.illumination * 100 );
    setText( m_moon[0].values[1], "%.1f d", day.phase.ageDays );
    setTime( m_moon[1].values[0], day.moon.rising, utcOffsetMinutes );
    setTime( m_moon[2].values[0], day.moon.setting, utcOffsetMinutes );
    setText( m_moon[3].values[0], "%.0f\xC2\xB0", now.moon.altitude );
    setText( m_moon[3].values[1], "%.0f\xC2\xB0", now.moon.azimuth );
    const Sky::Tide tide = Sky::tide( day.phase );
    lv_label_set_text( m_moon[4].values[0], tide == Sky::TideSpring ? "spring" : ( tide == Sky::TideNeap ? "neap" : "middle" ) );
    if ( day.darkWindow ) {
        setTime( m_moon[5].values[0], { true, day.darkStart }, utcOffsetMinutes );
        setTime( m_moon[5].values[1], { true, day.darkEnd }, utcOffsetMinutes );
    } else {
        lv_label_set_text( m_moon[5].values[0], "none" );
        lv_label_set_text( m_moon[5].values[1], "" );
    }
    static const char *zones[5] = { "--", "naked eye", "binoculars", "telescope", "not visible" };
    lv_label_set_text( m_moon[6].values[0], zones[day.crescent.zone] );
    setText( m_hijri, "%d %s %d", day.hijri.day, s_hijriMonths[day.hijri.month - 1], day.hijri.year );

    // Sun compass: with the top of the watch towards the Sun, north lies the Sun's azimuth
    // anticlockwise from the top.
    const bool usable = now.sun.altitude > 0 && now.sun.altitude < 75;
    if ( usable ) {
        const double angle = -now.sun.azimuth * 3.14159265358979323846 / 180.0;
        const double dx = std::sin( angle );
        const double dy = -std::cos( angle );
        const double tipX = s_compassCentreX + s_arrowLength * dx;
        const double tipY = s_compassCentreY + s_arrowLength * dy;
        // Arrow head: two strokes back from the tip, 30 degrees either side.
        const double backX = -dx * 30;
        const double backY = -dy * 30;
        const double sideX = -dy * 18;
        const double sideY = dx * 18;
        m_arrowPoints[0] = { static_cast<lv_value_precise_t>( s_compassCentreX - s_arrowLength * dx ),
                             static_cast<lv_value_precise_t>( s_compassCentreY - s_arrowLength * dy ) };
        m_arrowPoints[1] = { static_cast<lv_value_precise_t>( tipX ), static_cast<lv_value_precise_t>( tipY ) };
        m_arrowPoints[2] = { static_cast<lv_value_precise_t>( tipX + backX + sideX ),
                             static_cast<lv_value_precise_t>( tipY + backY + sideY ) };
        m_arrowPoints[3] = m_arrowPoints[1];
        m_arrowPoints[4] = { static_cast<lv_value_precise_t>( tipX + backX - sideX ),
                             static_cast<lv_value_precise_t>( tipY + backY - sideY ) };
        lv_line_set_points( m_compassArrow, m_arrowPoints, 5 );
        lv_obj_align( m_compassNorth, LV_ALIGN_TOP_LEFT, static_cast<int>( tipX + dx * 30 ) - 10,
                      static_cast<int>( tipY + dy * 30 ) - 16 );
        setText( m_compassText, "Top towards the Sun\nSun %.0f\xC2\xB0 high, %.0f\xC2\xB0",
                               now.sun.altitude, now.sun.azimuth );
    } else {
        lv_label_set_text( m_compassText, now.sun.altitude <= 0 ? "Sun below the horizon" : "Sun too high" );
    }
    lv_obj_set_style_opa( m_compassArrow, usable ? LV_OPA_COVER : LV_OPA_TRANSP, 0 );
    lv_obj_set_style_opa( m_compassNorth, usable ? LV_OPA_COVER : LV_OPA_TRANSP, 0 );
}
