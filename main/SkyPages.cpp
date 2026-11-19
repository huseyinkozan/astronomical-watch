#include "SkyPages.h"
#include "Styles.h"
#include "Fonts.h"
#include "Language.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <ctime>

static const int s_compassCentreX = 205;
static const int s_compassCentreY = 230;
static const int s_arrowLength = 110;


// The columns of today's label rows: a 160 px name and two 105 px values, centred. A long place
// may wrap onto a second line while the column stays within the tallest table (the title and
// nine rows), clear of the lower corners.
static const TablePart::Layout s_tableLayout = { nullptr, 0, {}, -1, 38, 38, 20, 160, 180, 105, 0, 32 + 9 * 38 };
static const uint32_t s_amberRgb = 0xFFB040;

static TablePart::Layout tableLayout( const char *title, int rowCount, const TablePart::RowKind *kinds )
{
    TablePart::Layout layout = s_tableLayout;
    layout.title = title;
    layout.rowCount = rowCount;
    for ( int i = 0; i < rowCount && i < TableText::MaxRows; ++i ) {
        layout.kinds[i] = kinds[i];
    }
    return layout;
}

void SkyPages::setPlace( const char *place )
{
    snprintf( m_place, sizeof( m_place ), "%s", place );
    m_sun.text().setLine( SunRows, m_place );
    m_sun.refresh();
}

const char *SkyPages::hijriMonthName( int month )
{
    return Language::hijriMonth( month );
}

// LVGL's own printf has no floating point (LV_SPRINTF_USE_FLOAT): format with the C library.
static void setText( TablePart &table, int row, int cell, const char *format, ... )
{
    char text[TableText::CellSize * 2];
    va_list arguments;
    va_start( arguments, format );
    vsnprintf( text, sizeof( text ), format, arguments );
    va_end( arguments );
    table.text().set( row, cell, text );
}

// A full-width line of a table, formatted.
static void setLineText( TablePart &table, int row, const char *format, ... )
{
    char text[TableText::Cells * TableText::CellSize];
    va_list arguments;
    va_start( arguments, format );
    vsnprintf( text, sizeof( text ), format, arguments );
    va_end( arguments );
    table.text().setLine( row, text );
}

static void setLabelText( lv_obj_t *label, const char *format, ... )
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

static const char *prayerName( int prayer )
{
    static const Language::Text s_names[Sky::PrayerCount] = { Language::Imsak, Language::PrayerSunrise, Language::Dhuhr,
                                                              Language::Asr,   Language::Maghrib,       Language::Isha };
    return Language::text( s_names[prayer] );
}

void SkyPages::build( lv_obj_t *sunPage, lv_obj_t *twilightPage, lv_obj_t *prayerPage, lv_obj_t *moonPage,
                      lv_obj_t *compassPage, lv_obj_t *timePage )
{
    // Prayer times: the six times, the next one with the time left, the method.
    TablePart::RowKind kinds[TableText::MaxRows] = {};
    for ( int i = 0; i < Sky::PrayerCount; ++i ) {
        kinds[i] = TablePart::OneValue;
        m_prayer.text().set( i, 0, prayerName( i ) );
    }
    kinds[Sky::PrayerCount] = TablePart::Line;
    kinds[Sky::PrayerCount + 1] = TablePart::Line;
    m_prayer.text().setColours( Sky::PrayerCount, TableText::Grey, s_amberRgb );
    m_prayer.text().setColours( Sky::PrayerCount + 1, TableText::Grey, TableText::Grey );
    m_prayer.build( prayerPage, tableLayout( Language::text( Language::PrayerTimes ), Sky::PrayerCount + 2, kinds ) );

    // Time: the clocks of this moment and the day's numbers.
    const char *timeNames[TimeRows] = { Language::text( Language::Local ),      "UTC",
                                        Language::text( Language::Solar ),      Language::text( Language::EquationOfTime ),
                                        Language::text( Language::Ezani ),      Language::text( Language::Qibla ),
                                        Language::text( Language::Sidereal ),   Language::text( Language::WeekAndDay ) };
    for ( int i = 0; i < TimeRows; ++i ) {
        kinds[i] = i == 5 ? TablePart::TwoValues : TablePart::OneValue;
        m_time.text().set( i, 0, timeNames[i] );
    }
    m_time.build( timePage, tableLayout( Language::text( Language::Time ), TimeRows, kinds ) );

    const char *sunNames[SunRows] = { Language::text( Language::Rise ), Language::text( Language::Set ),
                                      Language::text( Language::Noon ), Language::text( Language::Day ),
                                      "",                               Language::text( Language::Now ) };
    for ( int i = 0; i < SunRows; ++i ) {
        kinds[i] = TablePart::TwoValues;
        m_sun.text().set( i, 0, sunNames[i] );
    }
    // Where these times hold: a place changed by travel shows here.
    kinds[SunRows] = TablePart::Line;
    m_sun.text().setColours( SunRows, TableText::Grey, TableText::Grey );
    m_sun.text().setLine( SunRows, m_place );
    m_sun.build( sunPage, tableLayout( Language::text( Language::Sun ), SunRows + 1, kinds ) );

    // Twilight: a header row, then the limits at dawn and at dusk, then the next season.
    const char *twilightNames[TwilightRows] = { Language::text( Language::Astronomical ), Language::text( Language::Nautical ),
                                                Language::text( Language::Civil ),        Language::text( Language::BlueHour ),
                                                Language::text( Language::Until ),        Language::text( Language::GoldenHour ),
                                                Language::text( Language::Until ),        "" };
    kinds[0] = TablePart::TwoValues;
    m_twilight.text().set( 0, 1, Language::text( Language::Dawn ) );
    m_twilight.text().set( 0, 2, Language::text( Language::Dusk ) );
    m_twilight.text().setColours( 0, TableText::Grey, TableText::Grey );
    for ( int i = 0; i < TwilightRows; ++i ) {
        kinds[1 + i] = i < TwilightRows - 1 ? TablePart::TwoValues : TablePart::OneValue;
        m_twilight.text().set( 1 + i, 0, twilightNames[i] );
    }
    m_twilight.build( twilightPage, tableLayout( Language::text( Language::Twilight ), TwilightRows + 1, kinds ) );

    // Moon: the phase, the rows, the Hijri date.
    const char *moonNames[MoonRows] = { Language::text( Language::Lit ),   Language::text( Language::Rise ),
                                        Language::text( Language::Set ),   Language::text( Language::Now ),
                                        Language::text( Language::Tides ), Language::text( Language::DarkSky ),
                                        Language::text( Language::CrescentRow ) };
    const bool moonTwoValues[MoonRows] = { true, true, true, true, false, true, false };
    kinds[0] = TablePart::Line;
    for ( int i = 0; i < MoonRows; ++i ) {
        kinds[1 + i] = moonTwoValues[i] ? TablePart::TwoValues : TablePart::OneValue;
        m_moon.text().set( 1 + i, 0, moonNames[i] );
    }
    kinds[1 + MoonRows] = TablePart::Line;
    m_moon.build( moonPage, tableLayout( Language::text( Language::Moon ), MoonRows + 2, kinds ) );
    m_built = true;

    lv_obj_set_scrollbar_mode( compassPage, LV_SCROLLBAR_MODE_OFF );
    lv_obj_t *heading = lv_label_create( compassPage );
    Styles::text( heading, &jetbrains_mono_26, lv_color_hex( 0xA0A0A0 ) );
    lv_label_set_text( heading, Language::text( Language::SunCompass ) );
    lv_obj_align( heading, LV_ALIGN_TOP_MID, 0, 40 );
    m_compassArrow = lv_line_create( compassPage );
    lv_obj_set_style_line_width( m_compassArrow, 8, 0 );
    lv_obj_set_style_line_color( m_compassArrow, lv_color_white(), 0 );
    lv_obj_set_style_line_rounded( m_compassArrow, true, 0 );
    m_compassNorth = lv_label_create( compassPage );
    Styles::text( m_compassNorth, &jetbrains_mono_26, lv_color_white() );
    lv_label_set_text( m_compassNorth, Language::text( Language::North ) );
    m_compassText = lv_label_create( compassPage );
    Styles::text( m_compassText, &jetbrains_mono_26, lv_color_hex( 0xA0A0A0 ) );
    Styles::centred( m_compassText );
    lv_obj_align( m_compassText, LV_ALIGN_BOTTOM_MID, 0, -40 );
}

// An event's local time in a cell.
static void setTime( TablePart &table, int row, int cell, const Astronomy::Event &event, int utcOffsetMinutes )
{
    char text[8];
    formatTime( text, sizeof( text ), event.occurs, event.time, utcOffsetMinutes );
    table.text().set( row, cell, text );
}

void SkyPages::show( const Sky::Day &day, const Sky::Now &now, int utcOffsetMinutes )
{
    if ( !m_built ) {
        return;
    }
    const Astronomy::SunDay &sun = day.sun;
    char text[24];
    // Prayer times: the next one in amber.
    for ( int i = 0; i < Sky::PrayerCount; ++i ) {
        setTime( m_prayer, i, 1, { day.prayers.valid[i], day.prayers.times[i] }, utcOffsetMinutes );
        const bool next = i == now.nextPrayer;
        m_prayer.text().setColours( i, next ? s_amberRgb : TableText::Grey, next ? s_amberRgb : TableText::White );
    }
    if ( now.nextPrayer >= 0 ) {
        formatDuration( text, sizeof( text ), now.secondsToPrayer );
        setLineText( m_prayer, Sky::PrayerCount, Language::text( Language::PrayerIn ), prayerName( now.nextPrayer ), text );
    } else {
        m_prayer.text().setLine( Sky::PrayerCount, "" );
    }
    setLineText( m_prayer, Sky::PrayerCount + 1, Language::text( day.prayers.approximate ? Language::DiyanetApprox : Language::Diyanet ),
             m_place );

    // Time.
    const int64_t local = now.time + static_cast<int64_t>( utcOffsetMinutes ) * 60;
    const int localSeconds = static_cast<int>( ( local % 86400 + 86400 ) % 86400 );
    const int utcSeconds = static_cast<int>( ( now.time % 86400 + 86400 ) % 86400 );
    setText( m_time, 0, 1, "%02d:%02d:%02d", localSeconds / 3600, localSeconds / 60 % 60, localSeconds % 60 );
    setText( m_time, 1, 1, "%02d:%02d:%02d", utcSeconds / 3600, utcSeconds / 60 % 60, utcSeconds % 60 );
    setText( m_time, 2, 1, "%02d:%02d:%02d", now.solarSeconds / 3600, now.solarSeconds / 60 % 60, now.solarSeconds % 60 );
    const int equation = day.equationOfTime < 0 ? -day.equationOfTime : day.equationOfTime;
    setText( m_time, 3, 1, "%c%d:%02d", day.equationOfTime < 0 ? '-' : '+', equation / 60, equation % 60 );
    if ( now.ezaniSeconds >= 0 ) {
        // Sunset is 12 o'clock.
        const int hours = now.ezaniSeconds / 3600;
        setText( m_time, 4, 1, "%02d:%02d", hours == 0 ? 12 : hours, now.ezaniSeconds / 60 % 60 );
    } else {
        m_time.text().set( 4, 1, "--:--" );
    }
    setTime( m_time, 5, 1, day.qibla, utcOffsetMinutes );
    setText( m_time, 5, 2, "%.0f\xC2\xB0", day.qiblaBearing );
    setText( m_time, 6, 1, "%02d:%02d", now.siderealSeconds / 3600, now.siderealSeconds / 60 % 60 );
    setText( m_time, 7, 1, "%d / %d", day.isoWeek, day.dayOfYear );

    setTime( m_sun, 0, 1, sun.riseSet.rising, utcOffsetMinutes );
    setTime( m_sun, 1, 1, sun.riseSet.setting, utcOffsetMinutes );
    setTime( m_sun, 2, 1, sun.noon, utcOffsetMinutes );
    setText( m_sun, 2, 2, "%.0f\xC2\xB0", sun.noonAltitude );
    formatDuration( text, sizeof( text ), sun.dayLengthSeconds );
    m_sun.text().set( 3, 1, text );
    const int change = day.dayLengthChangeSeconds;
    const int changeAbs = change < 0 ? -change : change;
    setText( m_sun, 3, 2, "%c%d:%02d", change < 0 ? '-' : '+', changeAbs / 60, changeAbs % 60 );
    m_sun.text().set( 4, 0, Language::text( now.sunUp ? Language::SunsetIn : Language::SunriseIn ) );
    formatDuration( text, sizeof( text ), now.secondsToSunEvent );
    m_sun.text().set( 4, 1, now.secondsToSunEvent > 0 ? text : "--" );
    m_sun.text().set( 4, 2, "" );
    setText( m_sun, 5, 1, "%.0f\xC2\xB0", now.sun.altitude );
    setText( m_sun, 5, 2, "%.0f\xC2\xB0", now.sun.azimuth );

    // Twilight: when each limit is crossed at dawn and at dusk; the blue hour runs from -6 to -4
    // degrees, the golden hour from -4 to +6. Row 0 is the header.
    const Astronomy::Event *dawn[TwilightRows - 1] = { &sun.astronomical.rising, &sun.nautical.rising,
                                                          &sun.civil.rising,        &sun.civil.rising,
                                                          &sun.blueHour.rising,     &sun.blueHour.rising,
                                                          &sun.goldenHour.rising };
    const Astronomy::Event *dusk[TwilightRows - 1] = { &sun.astronomical.setting, &sun.nautical.setting,
                                                          &sun.civil.setting,        &sun.blueHour.setting,
                                                          &sun.civil.setting,        &sun.goldenHour.setting,
                                                          &sun.blueHour.setting };
    for ( int i = 0; i < TwilightRows - 1; ++i ) {
        setTime( m_twilight, 1 + i, 1, *dawn[i], utcOffsetMinutes );
        setTime( m_twilight, 1 + i, 2, *dusk[i], utcOffsetMinutes );
    }
    const time_t season = static_cast<time_t>( day.nextSeason + static_cast<int64_t>( utcOffsetMinutes ) * 60 );
    tm seasonDate = {};
    gmtime_r( &season, &seasonDate );
    m_twilight.text().set( TwilightRows, 0, Language::text( day.season % 2 == 0 ? Language::Equinox : Language::Solstice ) );
    if ( Language::current() == Language::Turkish ) {
        setText( m_twilight, TwilightRows, 1, "%d %s", seasonDate.tm_mday, Language::shortMonth( seasonDate.tm_mon + 1 ) );
    } else {
        setText( m_twilight, TwilightRows, 1, "%s %d", Language::shortMonth( seasonDate.tm_mon + 1 ), seasonDate.tm_mday );
    }

    // Moon: row 0 is the phase, rows 1 to 7 the table, row 8 the Hijri date.
    m_moon.text().setLine( 0, Language::text( static_cast<Language::Text>( Language::PhaseNew + Sky::phaseIndex( day.phase ) ) ) );
    setText( m_moon, 1, 1, "%.0f %%", day.phase.illumination * 100 );
    setText( m_moon, 1, 2, Language::text( Language::MoonAge ), day.phase.ageDays );
    setTime( m_moon, 2, 1, day.moon.rising, utcOffsetMinutes );
    setTime( m_moon, 3, 1, day.moon.setting, utcOffsetMinutes );
    setText( m_moon, 4, 1, "%.0f\xC2\xB0", now.moon.altitude );
    setText( m_moon, 4, 2, "%.0f\xC2\xB0", now.moon.azimuth );
    const Sky::Tide tide = Sky::tide( day.phase );
    m_moon.text().set( 5, 1, Language::text( tide == Sky::TideSpring ? Language::TideSpring
                                            : ( tide == Sky::TideNeap ? Language::TideNeap : Language::TideMiddle ) ) );
    if ( day.darkWindow ) {
        setTime( m_moon, 6, 1, { true, day.darkStart }, utcOffsetMinutes );
        setTime( m_moon, 6, 2, { true, day.darkEnd }, utcOffsetMinutes );
    } else {
        m_moon.text().set( 6, 1, Language::text( Language::None ) );
        m_moon.text().set( 6, 2, "" );
    }
    m_moon.text().set( 7, 1, Language::text( static_cast<Language::Text>( Language::CrescentNone + day.crescent.zone ) ) );
    setLineText( m_moon, 1 + MoonRows, "%d %s %d", day.hijri.day, Language::hijriMonth( day.hijri.month ), day.hijri.year );

    m_prayer.refresh();
    m_time.refresh();
    m_sun.refresh();
    m_twilight.refresh();
    m_moon.refresh();

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
        setLabelText( m_compassText, Language::text( Language::CompassHint ), now.sun.altitude, now.sun.azimuth );
    } else {
        lv_label_set_text( m_compassText, Language::text( now.sun.altitude <= 0 ? Language::SunBelow : Language::SunTooHigh ) );
    }
    lv_obj_set_style_opa( m_compassArrow, usable ? LV_OPA_COVER : LV_OPA_TRANSP, 0 );
    lv_obj_set_style_opa( m_compassNorth, usable ? LV_OPA_COVER : LV_OPA_TRANSP, 0 );
}

void SkyPages::forget()
{
    m_built = false;
    m_sun.forget();
    m_twilight.forget();
    m_prayer.forget();
    m_moon.forget();
    m_time.forget();
    m_compassArrow = nullptr;
    m_compassNorth = nullptr;
    m_compassText = nullptr;
}
