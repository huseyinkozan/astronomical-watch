#include "ClockPages.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Dates.h"
#include "Styles.h"
#include "Fonts.h"
#include "Language.h"
#include "Panel.h"
#include "SkyPages.h"
#include "esp_timer.h"

static const lv_color_t s_grey = LV_COLOR_MAKE( 0xA0, 0xA0, 0xA0 );
static const lv_color_t s_dim = LV_COLOR_MAKE( 0x5A, 0x5A, 0x5A );
static const lv_color_t s_amber = LV_COLOR_MAKE( 0xFF, 0xB0, 0x40 );
// Content stays inside the corner gauges: rows from x 52 to 358.
static const int s_left = 52;
static const int s_right = 358;
static const int s_tickMs = 50;


static lv_obj_t *label( lv_obj_t *parent, const lv_font_t *font, lv_color_t colour, lv_align_t align, int x, int y,
                        const char *text )
{
    lv_obj_t *object = lv_label_create( parent );
    Styles::text( object, font, colour );
    lv_label_set_text( object, text );
    lv_obj_align( object, align, x, y );
    lv_obj_set_clickable( object, false );
    return object;
}

static lv_obj_t *title( lv_obj_t *part, const char *text )
{
    return label( part, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_MID, 0, 34, text );
}

// A button like the settings rows: black with a mid-grey outline, lit while pressed. Returns
// its label; the button is the label's parent.
static lv_obj_t *button( lv_obj_t *part, int x1, int y1, int x2, int y2, const char *text, lv_event_cb_t onClicked,
                         void *user )
{
    lv_obj_t *box = lv_obj_create( part );
    lv_obj_set_pos( box, x1, y1 );
    lv_obj_set_size( box, x2 - x1, y2 - y1 );
    lv_obj_set_scrollable( box, false );
    lv_obj_set_press_lock( box, false );
    Styles::button( box );
    lv_obj_add_event_cb( box, onClicked, LV_EVENT_CLICKED, user );
    return label( box, &jetbrains_mono_26, lv_color_white(), LV_ALIGN_CENTER, 0, 0, text );
}

// "mm:ss.cc", or "h:mm:ss" from one hour on.
static void formatStopwatch( char *text, int size, int64_t ms )
{
    const int64_t seconds = ms / 1000;
    if ( seconds >= 3600 ) {
        snprintf( text, size, "%d:%02d:%02d", static_cast<int>( seconds / 3600 ), static_cast<int>( seconds / 60 % 60 ),
                  static_cast<int>( seconds % 60 ) );
    } else {
        snprintf( text, size, "%02d:%02d.%02d", static_cast<int>( seconds / 60 ), static_cast<int>( seconds % 60 ),
                  static_cast<int>( ms / 10 % 100 ) );
    }
}

// Sets the text only when it changed, so that LVGL redraws nothing else.
static void setText( lv_obj_t *object, const char *text )
{
    if ( strcmp( lv_label_get_text( object ), text ) != 0 ) {
        lv_label_set_text( object, text );
    }
}

void ClockPages::build( lv_obj_t *tile )
{
    m_timers.load();
    m_stack.build( tile, PartCount, Language::text( Language::ClockPageName ) );

    lv_obj_t *world = m_stack.part( WorldPart );
    title( world, Language::text( Language::WorldClock ) );
    label( world, &jetbrains_mono_26, lv_color_white(), LV_ALIGN_TOP_LEFT, s_left, 76, Language::text( Language::Here ) );
    m_homeDate = label( world, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_RIGHT, s_right - Panel::WIDTH, 76, "" );
    m_homeTime = label( world, &jetbrains_mono_48, lv_color_white(), LV_ALIGN_TOP_LEFT, s_left, 104, "" );
    m_homeZone = label( world, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_RIGHT, s_right - Panel::WIDTH, 120, "" );
    lv_obj_t *line = lv_obj_create( world );
    lv_obj_remove_style_all( line );
    lv_obj_set_pos( line, s_left, 166 );
    lv_obj_set_size( line, s_right - s_left, 1 );
    lv_obj_set_style_bg_opa( line, LV_OPA_COVER, 0 );
    lv_obj_set_style_bg_color( line, lv_color_hex( 0x303030 ), 0 );
    m_cityHint = label( world, &jetbrains_mono_26, s_dim, LV_ALIGN_TOP_MID, 0, 230, Language::text( Language::CitiesHint ) );
    lv_obj_set_style_text_align( m_cityHint, LV_TEXT_ALIGN_CENTER, 0 );
    for ( int i = 0; i < WorldClock::ShownCities; ++i ) {
        const int y = 180 + i * 72;
        m_cityNames[i] = label( world, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_LEFT, s_left, y, "" );
        // Left of the time: long names end in dots.
        lv_obj_set_width( m_cityNames[i], 150 );
        lv_label_set_long_mode( m_cityNames[i], LV_LABEL_LONG_MODE_DOTS );
        m_cityDiffs[i] = label( world, &jetbrains_mono_26, s_dim, LV_ALIGN_TOP_LEFT, s_left, y + 28, "" );
        m_cityTimes[i] = label( world, &jetbrains_mono_48, lv_color_white(), LV_ALIGN_TOP_RIGHT, s_right - Panel::WIDTH,
                                y + 2, "" );
    }

    lv_obj_t *stopwatch = m_stack.part( StopwatchPart );
    title( stopwatch, Language::text( Language::Stopwatch ) );
    m_stopwatchTime = label( stopwatch, &jetbrains_mono_48, lv_color_white(), LV_ALIGN_TOP_MID, 0, 84, "" );
    m_stopwatchSince = label( stopwatch, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_MID, 0, 142, "" );
    for ( int i = 0; i < Timers::LapCount; ++i ) {
        m_lapNames[i] = label( stopwatch, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_LEFT, s_left, 190 + i * 36, "" );
        m_lapTimes[i] = label( stopwatch, &jetbrains_mono_26, lv_color_white(), LV_ALIGN_TOP_RIGHT, s_right - Panel::WIDTH,
                               190 + i * 36, "" );
    }
    m_stopwatchLeft = button( stopwatch, s_left, 318, 200, 398, "", onStopwatchButton, this );
    m_stopwatchRight = button( stopwatch, 210, 318, s_right, 398, "", onStopwatchButton, this );

    lv_obj_t *timer = m_stack.part( TimerPart );
    title( timer, Language::text( Language::Timer ) );
    m_timerTime = label( timer, &jetbrains_mono_light_90, lv_color_white(), LV_ALIGN_TOP_MID, 0, 72, "" );
    m_timerInfo = label( timer, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_MID, 0, 182, "" );
    static const char *s_add[3] = { "+1", "+5", "+10" };
    for ( int i = 0; i < 3; ++i ) {
        m_timerButtons[i] = button( timer, s_left + i * 104, 224, s_left + 96 + i * 104, 292, s_add[i], onTimerButton, this );
    }
    m_timerButtons[3] = button( timer, s_left, 318, 200, 398, Language::text( Language::Reset ), onTimerButton, this );
    m_timerButtons[4] = button( timer, 210, 318, s_right, 398, "", onTimerButton, this );

    buildCalendar( m_gregorian, m_stack.part( GregorianPart ) );
    buildCalendar( m_hijri, m_stack.part( HijriPart ) );
    lv_label_set_text( m_hijri.title, "..." );

    lv_obj_t *days = m_stack.part( DaysPart );
    title( days, Language::text( Language::Days ) );
    m_counterHint = label( days, &jetbrains_mono_26, s_dim, LV_ALIGN_TOP_MID, 0, 200, Language::text( Language::DaysHint ) );
    lv_obj_set_style_text_align( m_counterHint, LV_TEXT_ALIGN_CENTER, 0 );
    for ( int i = 0; i < DayCounters::MaxCounters; ++i ) {
        m_counterNames[i] = label( days, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_LEFT, s_left, 90 + i * 74, "" );
        m_counterValues[i] = label( days, &jetbrains_mono_26, lv_color_white(), LV_ALIGN_TOP_RIGHT, s_right - Panel::WIDTH,
                                    118 + i * 74, "" );
    }

    // After StackPage's handler: it takes the vertical swipes, this one the sideways ones.
    lv_obj_add_event_cb( tile, onGesture, LV_EVENT_GESTURE, this );
    lv_timer_create( onTick, s_tickMs, this );
}

StackPage &ClockPages::stack()
{
    return m_stack;
}

void ClockPages::buildCalendar( Calendar &calendar, lv_obj_t *part )
{
    calendar.part = part;
    calendar.title = label( part, &jetbrains_mono_26, lv_color_white(), LV_ALIGN_TOP_MID, 0, 34, "" );
    calendar.grid = lv_obj_create( part );
    lv_obj_remove_style_all( calendar.grid );
    lv_obj_set_size( calendar.grid, Panel::WIDTH, Panel::HEIGHT );
    lv_obj_set_scrollable( calendar.grid, false );
    lv_obj_set_user_data( calendar.grid, &calendar );
    lv_obj_add_event_cb( calendar.grid, onDrawCalendar, LV_EVENT_DRAW_MAIN, this );
    // Taps choose a day; swipes still reach the tile (the month and part steps).
    lv_obj_add_event_cb( calendar.grid, onCalendarTouch, LV_EVENT_PRESSED, this );
    lv_obj_add_event_cb( calendar.grid, onCalendarTouch, LV_EVENT_CLICKED, this );
    // At the bottom, where the page marks show while the pages are swiped: here a sideways swipe
    // moves the month.
    label( part, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_MID, -70, 458, "<" );
    label( part, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_MID, 70, 458, ">" );
    calendar.footer = label( part, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_MID, 0, 410, "" );
}

void ClockPages::onDrawCalendar( lv_event_t *event )
{
    lv_obj_t *grid = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    const Calendar &calendar = *static_cast<const Calendar *>( lv_obj_get_user_data( grid ) );
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t coords = {};
    lv_obj_get_coords( grid, &coords );
    // The day numbers as constant strings: LVGL draws later and keeps only the pointer.
    static const char *s_numbers[31] = { "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9",  "10", "11",
                                         "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22",
                                         "23", "24", "25", "26", "27", "28", "29", "30", "31" };
    lv_draw_label_dsc_t text;
    lv_draw_label_dsc_init( &text );
    text.font = &jetbrains_mono_26;
    text.align = LV_TEXT_ALIGN_CENTER;
    for ( int column = 0; column < 7; ++column ) {
        const int x = coords.x1 + 61 + column * 48;
        const lv_area_t area = { x - 24, coords.y1 + 84, x + 24, coords.y1 + 84 + 30 };
        text.color = column < 5 ? s_grey : s_amber;
        text.text = Language::dayInitials( column );
        lv_draw_label( layer, &text, &area );
    }
    for ( int day = 1; day <= calendar.length && day <= 31; ++day ) {
        const int cell = calendar.firstWeekday + day - 1;
        const int column = cell % 7;
        const int x = coords.x1 + 61 + column * 48;
        const int y = coords.y1 + 128 + cell / 7 * 46;
        lv_draw_rect_dsc_t ring;
        lv_draw_rect_dsc_init( &ring );
        ring.radius = LV_RADIUS_CIRCLE;
        ring.bg_opa = LV_OPA_TRANSP;
        ring.border_width = 2;
        if ( day == calendar.today ) {
            ring.border_color = lv_color_white();
            const lv_area_t box = { x - 22, y - 7, x + 21, y + 36 };
            lv_draw_rect( layer, &ring, &box );
        }
        if ( day == calendar.chosen ) {
            // Around today's ring when both fall on the same day.
            const int grow = day == calendar.today ? 3 : 0;
            ring.border_color = s_amber;
            ring.border_width = 3;
            const lv_area_t box = { x - 22 - grow, y - 7 - grow, x + 21 + grow, y + 36 + grow };
            lv_draw_rect( layer, &ring, &box );
        }
        const lv_area_t area = { x - 24, y, x + 24, y + 30 };
        text.color = column < 5 ? lv_color_white() : s_amber;
        text.text = s_numbers[day - 1];
        lv_draw_label( layer, &text, &area );
    }
}

void ClockPages::fillCalendar( Calendar &calendar, int firstWeekday, int length, int today, int chosen )
{
    calendar.firstWeekday = firstWeekday;
    calendar.length = length;
    calendar.today = today;
    calendar.chosen = chosen;
    lv_obj_invalidate( calendar.grid );
}

int ClockPages::dayAt( const Calendar &calendar, const lv_point_t &point )
{
    lv_area_t coords = {};
    lv_obj_get_coords( calendar.grid, &coords );
    // The cells of onDrawCalendar: 48 px columns centred from x 61, 46 px rows from y 121.
    const int dx = point.x - coords.x1 - 61 + 24;
    const int dy = point.y - coords.y1 - 121;
    if ( dx < 0 || dy < 0 || dx >= 7 * 48 ) {
        return 0;
    }
    const int day = dy / 46 * 7 + dx / 48 - calendar.firstWeekday + 1;
    return day >= 1 && day <= calendar.length ? day : 0;
}

void ClockPages::onCalendarTouch( lv_event_t *event )
{
    ClockPages *self = static_cast<ClockPages *>( lv_event_get_user_data( event ) );
    lv_obj_t *grid = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    const Calendar &calendar = *static_cast<const Calendar *>( lv_obj_get_user_data( grid ) );
    lv_point_t point = {};
    lv_indev_get_point( lv_indev_active(), &point );
    if ( lv_event_get_code( event ) == LV_EVENT_PRESSED ) {
        self->m_pressPoint = point;
        return;
    }
    // A finger that moved was a swipe (the month step), not a tap.
    const int moved = abs( point.x - self->m_pressPoint.x ) + abs( point.y - self->m_pressPoint.y );
    const int day = moved <= 30 ? dayAt( calendar, self->m_pressPoint ) : 0;
    if ( day == 0 || calendar.length == 0 ) {
        return;
    }
    if ( &calendar == &self->m_gregorian ) {
        self->choose( Dates::dayNumber( self->m_gregorianShown[0], self->m_gregorianShown[1], day ) );
    } else if ( !self->m_hijriWanted ) {
        self->choose( self->m_hijriMonth.firstDay + day - 1 );
    }
}

void ClockPages::choose( int day )
{
    m_chosenDay = day;
    m_gregorianStep = 0;
    showGregorian();
    m_hijriStep = 0;
    if ( day >= m_hijriMonth.firstDay && day < m_hijriMonth.firstDay + m_hijriMonth.length ) {
        // Already in the Hijri month shown: only the marks change.
        m_hijriAnchor = day;
        showHijriMarks();
    } else if ( !m_hijriWanted ) {
        m_hijriRequest = { day, 0, 0 };
        m_hijriWanted = true;
        lv_label_set_text( m_hijri.title, "..." );
    } else {
        // update() asks for it once the month being computed has come.
        m_hijriAnchor = -1;
    }
}

int ClockPages::anchorDay() const
{
    return m_chosenDay >= 0 ? m_chosenDay : m_today;
}

void ClockPages::showGregorian()
{
    int year = 0;
    int month = 0;
    int anchor = 0;
    Dates::civil( anchorDay(), year, month, anchor );
    month += m_gregorianStep;
    while ( month > 12 ) {
        month -= 12;
        ++year;
    }
    while ( month < 1 ) {
        month += 12;
        --year;
    }
    const int first = Dates::dayNumber( year, month, 1 );
    const int length = Dates::daysInMonth( year, month );
    const int today = m_today >= first && m_today < first + length ? m_today - first + 1 : 0;
    const int chosen = m_chosenDay >= first && m_chosenDay < first + length ? m_chosenDay - first + 1 : 0;
    const int shown[4] = { year, month, today, chosen };
    if ( memcmp( shown, m_gregorianShown, sizeof( shown ) ) == 0 ) {
        return;
    }
    memcpy( m_gregorianShown, shown, sizeof( shown ) );
    lv_label_set_text_fmt( m_gregorian.title, "%s %d", Language::monthName( month ), year );
    fillCalendar( m_gregorian, Dates::weekday( first ), length, today, chosen );
    // The week and the day of the year of the chosen day, else of today.
    lv_label_set_text_fmt( m_gregorian.footer, Language::text( Language::WeekDay ), Dates::isoWeek( anchorDay() ),
                           Dates::dayOfYear( anchorDay() ) );
}

void ClockPages::showHijriMarks()
{
    const Hijri::Month &month = m_hijriMonth;
    const bool holdsToday = m_today >= month.firstDay && m_today < month.firstDay + month.length;
    const bool holdsChosen = m_chosenDay >= month.firstDay && m_chosenDay < month.firstDay + month.length;
    fillCalendar( m_hijri, Dates::weekday( month.firstDay ), month.length, holdsToday ? m_today - month.firstDay + 1 : 0,
                  holdsChosen ? m_chosenDay - month.firstDay + 1 : 0 );
}

void ClockPages::update( int64_t utcSeconds, int utcOffsetMinutes, const Rtc::DateTime &localTime )
{
    m_offsetMinutes = utcOffsetMinutes;
    m_local = localTime;
    if ( localTime.month == 0 ) {
        return;
    }
    // Keep a millisecond clock between the RTC's seconds; move it only when it is a second off.
    const int64_t predicted = nowMs();
    if ( m_baseUs == 0 || predicted < utcSeconds * 1000 - 1000 || predicted > utcSeconds * 1000 + 2000 ) {
        m_baseMs = utcSeconds * 1000 + 500;
        m_baseUs = esp_timer_get_time();
    }
    m_today = Dates::dayNumber( localTime.year, localTime.month, localTime.day );

    char text[32];
    snprintf( text, sizeof( text ), "%02d:%02d", localTime.hour, localTime.minute );
    setText( m_homeTime, text );
    if ( Language::current() == Language::Turkish ) {
        snprintf( text, sizeof( text ), "%d %s %s", localTime.day, Language::shortMonth( localTime.month ),
                  Language::shortDay( Dates::weekday( m_today ) ) );
    } else {
        snprintf( text, sizeof( text ), "%s %d %s", Language::shortDay( Dates::weekday( m_today ) ), localTime.day,
                  Language::shortMonth( localTime.month ) );
    }
    setText( m_homeDate, text );
    const int offset = utcOffsetMinutes < 0 ? -utcOffsetMinutes : utcOffsetMinutes;
    if ( offset % 60 == 0 ) {
        snprintf( text, sizeof( text ), "UTC%c%d", utcOffsetMinutes < 0 ? '-' : '+', offset / 60 );
    } else {
        snprintf( text, sizeof( text ), "UTC%c%d:%02d", utcOffsetMinutes < 0 ? '-' : '+', offset / 60, offset % 60 );
    }
    setText( m_homeZone, text );
    showCities( utcSeconds );
    showCounters();

    // A calendar left alone returns to the month of the chosen day, else of today.
    if ( m_gregorianStep != 0 && !lv_obj_is_visible( m_gregorian.part ) ) {
        m_gregorianStep = 0;
    }
    showGregorian();
    if ( m_hijriStep != 0 && !lv_obj_is_visible( m_hijri.part ) ) {
        m_hijriStep = 0;
        m_hijriAnchor = -1;
    }
    // The month is computed when the calendar is seen (the screenshots need it at once).
    if ( ( m_scene || lv_obj_is_visible( m_hijri.part ) ) && m_hijriAnchor != anchorDay() && !m_hijriWanted ) {
        m_hijriRequest = { anchorDay(), 0, 0 };
        m_hijriWanted = true;
    }
}

void ClockPages::setCities( const WorldClock &worldClock )
{
    m_cityCount = worldClock.count() < WorldClock::ShownCities ? worldClock.count() : WorldClock::ShownCities;
    for ( int i = 0; i < m_cityCount; ++i ) {
        m_cities[i] = worldClock.city( i );
    }
    lv_obj_set_hidden( m_cityHint, m_cityCount > 0 );
    for ( int i = m_cityCount; i < WorldClock::ShownCities; ++i ) {
        setText( m_cityNames[i], "" );
        setText( m_cityDiffs[i], "" );
        setText( m_cityTimes[i], "" );
    }
}

void ClockPages::setCounters( const DayCounters &counters )
{
    m_counterCount = counters.count();
    for ( int i = 0; i < m_counterCount; ++i ) {
        m_counters[i] = counters.counter( i );
    }
    lv_obj_set_hidden( m_counterHint, m_counterCount > 0 );
    for ( int i = m_counterCount; i < DayCounters::MaxCounters; ++i ) {
        setText( m_counterNames[i], "" );
        setText( m_counterValues[i], "" );
    }
    showCounters();
}

void ClockPages::showCounters()
{
    if ( m_today == 0 ) {
        return;
    }
    for ( int i = 0; i < m_counterCount; ++i ) {
        const int days = m_counters[i].day - m_today;
        char text[24];
        if ( days == 0 ) {
            snprintf( text, sizeof( text ), "%s", Language::text( Language::Today ) );
        } else if ( days > 0 ) {
            snprintf( text, sizeof( text ), Language::text( Language::InDays ), days );
        } else {
            snprintf( text, sizeof( text ), Language::text( Language::DaysAgo ), -days );
        }
        setText( m_counterNames[i], m_counters[i].name );
        setText( m_counterValues[i], text );
    }
}

void ClockPages::showCities( int64_t utcSeconds )
{
    for ( int i = 0; i < m_cityCount; ++i ) {
        const int offset = Zones::offsetAt( m_cities[i].zone, utcSeconds );
        const int64_t local = utcSeconds + offset * 60;
        const int minuteOfDay = static_cast<int>( ( ( local % 86400 ) + 86400 ) % 86400 / 60 );
        char text[32];
        snprintf( text, sizeof( text ), "%02d:%02d", minuteOfDay / 60, minuteOfDay % 60 );
        setText( m_cityTimes[i], text );
        setText( m_cityNames[i], m_cities[i].name );
        // The difference to our own time, and the weekday when the date differs.
        const int diff = offset - m_offsetMinutes;
        const int size = diff < 0 ? -diff : diff;
        int length = 0;
        if ( diff == 0 ) {
            length = snprintf( text, sizeof( text ), "%s", Language::text( Language::SameTime ) );
        } else if ( size % 60 == 0 ) {
            length = snprintf( text, sizeof( text ), Language::text( Language::Hours ), diff < 0 ? '-' : '+', size / 60 );
        } else {
            length = snprintf( text, sizeof( text ), "%c%d:%02d", diff < 0 ? '-' : '+', size / 60, size % 60 );
        }
        const int cityDay = static_cast<int>( ( local >= 0 ? local : local - 86399 ) / 86400 );
        if ( cityDay != m_today ) {
            snprintf( text + length, sizeof( text ) - length, ", %s", Language::shortDay( Dates::weekday( cityDay ) ) );
        }
        setText( m_cityDiffs[i], text );
    }
}

bool ClockPages::takeHijriRequest( HijriRequest &request )
{
    if ( !m_hijriWanted ) {
        return false;
    }
    request = m_hijriRequest;
    return true;
}

void ClockPages::showHijriMonth( const Hijri::Month &month, int today, int method )
{
    m_hijriWanted = false;
    m_hijriMonth = month;
    if ( m_hijriRequest.step == 0 ) {
        m_hijriAnchor = m_hijriRequest.fromDay;
    }
    lv_label_set_text_fmt( m_hijri.title, "%s %d", SkyPages::hijriMonthName( month.month ), month.year );
    m_today = today;
    showHijriMarks();
    lv_label_set_text( m_hijri.footer, Language::text( method == Hijri::Diyanet ? Language::DiyanetMethod : Language::LocalCrescent ) );
}

int64_t ClockPages::nowMs() const
{
    return m_baseMs + ( esp_timer_get_time() - m_baseUs ) / 1000;
}

void ClockPages::formatLocal( char *text, int size, int64_t unixMs ) const
{
    const int64_t local = unixMs / 1000 + m_offsetMinutes * 60;
    const int minuteOfDay = static_cast<int>( ( ( local % 86400 ) + 86400 ) % 86400 / 60 );
    snprintf( text, size, "%02d:%02d", minuteOfDay / 60, minuteOfDay % 60 );
}

void ClockPages::showStopwatch( int64_t now )
{
    const Timers::Stopwatch &watch = m_timers.stopwatch();
    char text[32];
    formatStopwatch( text, sizeof( text ), m_timers.elapsedMs( now ) );
    setText( m_stopwatchTime, text );
    if ( watch.running ) {
        char since[8];
        formatLocal( since, sizeof( since ), watch.startMs );
        snprintf( text, sizeof( text ), Language::text( Language::RunningSince ), since );
    } else {
        snprintf( text, sizeof( text ), "%s", watch.accumulatedMs > 0 ? Language::text( Language::Stopped ) : "" );
    }
    setText( m_stopwatchSince, text );
    for ( int i = 0; i < Timers::LapCount; ++i ) {
        const int number = watch.laps - i;
        if ( number < 1 ) {
            setText( m_lapNames[i], "" );
            setText( m_lapTimes[i], "" );
            continue;
        }
        snprintf( text, sizeof( text ), Language::text( Language::LapNumber ), number );
        setText( m_lapNames[i], text );
        formatStopwatch( text, sizeof( text ), watch.lapMs[i] );
        setText( m_lapTimes[i], text );
    }
    setText( m_stopwatchLeft, Language::text( watch.running ? Language::Lap : Language::Reset ) );
    setText( m_stopwatchRight, Language::text( watch.running ? Language::Stop : Language::Start ) );
    lv_obj_set_style_text_color( m_stopwatchRight, watch.running ? s_amber : lv_color_white(), 0 );
}

void ClockPages::showCountdown( int64_t now )
{
    const Timers::Countdown &timer = m_timers.countdown();
    const int64_t remaining = m_timers.remainingMs( now );
    // Rounded up: the countdown shows 00:00 only once it has run out.
    const int seconds = static_cast<int>( ( remaining + 999 ) / 1000 );
    char text[40];
    snprintf( text, sizeof( text ), "%02d:%02d", seconds / 60, seconds % 60 );
    setText( m_timerTime, text );
    char end[8];
    if ( timer.durationS == 0 ) {
        snprintf( text, sizeof( text ), "%s", Language::text( Language::AddMinutes ) );
    } else if ( timer.running ) {
        formatLocal( end, sizeof( end ), timer.endMs );
        if ( remaining > 0 ) {
            snprintf( text, sizeof( text ), Language::text( Language::TimerEnds ), timer.durationS / 60, timer.durationS % 60, end );
        } else {
            snprintf( text, sizeof( text ), Language::text( Language::TimerDone ), end );
        }
    } else {
        snprintf( text, sizeof( text ), Language::text( Language::TimerPaused ), timer.durationS / 60, timer.durationS % 60 );
    }
    setText( m_timerInfo, text );
    const bool counting = timer.running && remaining > 0;
    setText( m_timerButtons[4], Language::text( counting ? Language::Pause : Language::Start ) );
    lv_obj_set_style_text_color( m_timerButtons[4], counting ? s_amber : lv_color_white(), 0 );
}

void ClockPages::onTick( lv_timer_t *timer )
{
    ClockPages *self = static_cast<ClockPages *>( lv_timer_get_user_data( timer ) );
    if ( self->m_scene || self->m_baseUs == 0 ) {
        return;
    }
    // Also while hidden: a part that slides in must not show an old value first. A hidden label
    // that changes is not drawn.
    const int64_t now = self->nowMs();
    self->showStopwatch( now );
    self->showCountdown( now );
}

void ClockPages::onStopwatchButton( lv_event_t *event )
{
    ClockPages *self = static_cast<ClockPages *>( lv_event_get_user_data( event ) );
    if ( self->m_scene ) {
        return;
    }
    lv_obj_t *box = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    const int64_t now = self->nowMs();
    if ( box == lv_obj_get_parent( self->m_stopwatchRight ) ) {
        self->m_timers.startStopStopwatch( now );
    } else if ( self->m_timers.stopwatch().running ) {
        self->m_timers.lap( now );
    } else {
        self->m_timers.resetStopwatch();
    }
    self->showStopwatch( now );
}

void ClockPages::onTimerButton( lv_event_t *event )
{
    ClockPages *self = static_cast<ClockPages *>( lv_event_get_user_data( event ) );
    if ( self->m_scene ) {
        return;
    }
    lv_obj_t *box = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    const int64_t now = self->nowMs();
    static const int s_minutes[3] = { 1, 5, 10 };
    for ( int i = 0; i < 3; ++i ) {
        if ( box == lv_obj_get_parent( self->m_timerButtons[i] ) ) {
            self->m_timers.addMinutes( s_minutes[i] );
        }
    }
    if ( box == lv_obj_get_parent( self->m_timerButtons[3] ) ) {
        self->m_timers.resetCountdown();
    } else if ( box == lv_obj_get_parent( self->m_timerButtons[4] ) ) {
        self->m_timers.startPauseCountdown( now );
    }
    self->showCountdown( now );
}

bool ClockPages::swipesSideways() const
{
    return m_stack.current() == GregorianPart || m_stack.current() == HijriPart;
}

void ClockPages::stepGregorian( int step )
{
    m_gregorianStep += step;
    showGregorian();
}

void ClockPages::stepHijri( int step )
{
    // One Hijri month at a time: a swipe while the main task computes one is dropped.
    if ( m_hijriWanted || m_hijriAnchor < 0 ) {
        return;
    }
    m_hijriStep += step;
    m_hijriRequest = { m_hijriMonth.firstDay, m_hijriMonth.length, step };
    m_hijriWanted = true;
    lv_label_set_text( m_hijri.title, "..." );
}

void ClockPages::onGesture( lv_event_t *event )
{
    ClockPages *self = static_cast<ClockPages *>( lv_event_get_user_data( event ) );
    const lv_dir_t direction = lv_indev_get_gesture_dir( lv_indev_active() );
    if ( direction != LV_DIR_LEFT && direction != LV_DIR_RIGHT ) {
        return;
    }
    // A swipe to the left brings the next month, as a page would come in from the right.
    const int step = direction == LV_DIR_LEFT ? 1 : -1;
    if ( self->m_stack.current() == GregorianPart ) {
        self->stepGregorian( step );
    } else if ( self->m_stack.current() == HijriPart ) {
        self->stepHijri( step );
    }
}

bool ClockPages::press( const char *name )
{
    // Development: the buttons and tap areas by name, as a finger would press them; "gd<n>" and
    // "hd<n>" choose day n of the Gregorian or Hijri month shown.
    if ( ( name[0] == 'g' || name[0] == 'h' ) && name[1] == 'd' ) {
        const int day = atoi( name + 2 );
        if ( name[0] == 'g' && day >= 1 && day <= m_gregorian.length ) {
            choose( Dates::dayNumber( m_gregorianShown[0], m_gregorianShown[1], day ) );
            return true;
        }
        if ( name[0] == 'h' && !m_hijriWanted && day >= 1 && day <= m_hijriMonth.length ) {
            choose( m_hijriMonth.firstDay + day - 1 );
            return true;
        }
        return false;
    }
    static const char *s_steps[4] = { "g<", "g>", "h<", "h>" };
    for ( int i = 0; i < 4; ++i ) {
        if ( strcmp( name, s_steps[i] ) == 0 ) {
            if ( i < 2 ) {
                stepGregorian( i == 0 ? -1 : 1 );
            } else {
                stepHijri( i == 2 ? -1 : 1 );
            }
            return true;
        }
    }
    lv_obj_t *targets[7] = { lv_obj_get_parent( m_stopwatchLeft ), lv_obj_get_parent( m_stopwatchRight ),
                             lv_obj_get_parent( m_timerButtons[0] ), lv_obj_get_parent( m_timerButtons[1] ),
                             lv_obj_get_parent( m_timerButtons[2] ), lv_obj_get_parent( m_timerButtons[3] ),
                             lv_obj_get_parent( m_timerButtons[4] ) };
    static const char *s_names[7] = { "lap", "start", "+1", "+5", "+10", "reset", "go" };
    for ( int i = 0; i < 7; ++i ) {
        if ( strcmp( name, s_names[i] ) == 0 ) {
            lv_obj_send_event( targets[i], LV_EVENT_CLICKED, nullptr );
            return true;
        }
    }
    return false;
}

const char *const ClockPages::SceneCityNames[WorldClock::ShownCities] = { "London", "New York", "Tokyo" };
const int ClockPages::SceneCityOffsets[WorldClock::ShownCities] = { 60, -240, 540 };
const char *const ClockPages::SceneCounterNames[2][4] = { { "Trip home", "Ramadan", "Birthday", "Wedding" },
                                                          { "Eve dönüş", "Ramazan", "Doğum günü", "Düğün" } };
const int ClockPages::SceneCounterDays[4] = { 12, 232, 143, -4212 };

void ClockPages::setScene( bool scene )
{
    m_scene = scene;
    if ( !scene ) {
        return;
    }
    // Sample states for the screenshots; the saved timers and cities stay as they are. The
    // offsets are those of the scene's date (21 June: summer time in London and New York).
    m_cityCount = WorldClock::ShownCities;
    for ( int i = 0; i < WorldClock::ShownCities; ++i ) {
        m_cities[i] = {};
        strlcpy( m_cities[i].name, SceneCityNames[i], sizeof( m_cities[i].name ) );
        m_cities[i].zone.offset = SceneCityOffsets[i];
    }
    lv_obj_set_hidden( m_cityHint, true );
    // Day counters relative to the scene's 21 June 2026.
    m_counterCount = 4;
    for ( int i = 0; i < 4; ++i ) {
        strlcpy( m_counters[i].name, SceneCounterNames[Language::current()][i], sizeof( m_counters[i].name ) );
        m_counters[i].day = Dates::dayNumber( 2026, 6, 21 ) + SceneCounterDays[i];
    }
    lv_obj_set_hidden( m_counterHint, true );
    setText( m_stopwatchTime, "12:34.56" );
    char text[40];
    snprintf( text, sizeof( text ), Language::text( Language::RunningSince ), "19:06" );
    setText( m_stopwatchSince, text );
    static const char *s_laps[3] = { "04:10.20", "04:02.81", "04:21.55" };
    for ( int i = 0; i < Timers::LapCount; ++i ) {
        lv_label_set_text_fmt( m_lapNames[i], Language::text( Language::LapNumber ), 3 - i );
        setText( m_lapTimes[i], s_laps[i] );
    }
    setText( m_stopwatchLeft, Language::text( Language::Lap ) );
    setText( m_stopwatchRight, Language::text( Language::Stop ) );
    lv_obj_set_style_text_color( m_stopwatchRight, s_amber, 0 );
    setText( m_timerTime, "07:42" );
    snprintf( text, sizeof( text ), Language::text( Language::TimerEnds ), 10, 0, "19:27" );
    setText( m_timerInfo, text );
    setText( m_timerButtons[4], Language::text( Language::Pause ) );
    lv_obj_set_style_text_color( m_timerButtons[4], s_amber, 0 );
}

int64_t ClockPages::countdownEndMs() const
{
    const Timers::Countdown &countdown = m_timers.countdown();
    return countdown.running ? countdown.endMs : 0;
}

bool ClockPages::stopwatchRunning() const
{
    return m_timers.stopwatch().running;
}

void ClockPages::finishCountdown()
{
    m_timers.finishCountdown();
}
