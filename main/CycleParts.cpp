// The cycle page's parts and overlays, drawn (CyclePages; the layouts of
// docs/superpowers/specs/2026-11-22-cycle-page/pages.png and entry.png).

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Astronomy.h"
#include "CycleDraw.h"
#include "CyclePages.h"
#include "Dates.h"
#include "Fonts.h"
#include "Hayiz.h"
#include "Language.h"
#include "Panel.h"
#include "esp_timer.h"

static const double s_synodic = 29.530589;
static const int s_cx = Panel::WIDTH / 2;
static const int s_cy = 236;

static lv_color_t white()
{
    return lv_color_white();
}

static lv_color_t textGrey()
{
    return lv_color_hex( 0xA0A0A0 );
}

// `text` into `box` of the layer (copied: the band is drawn later).
static void text( lv_layer_t *layer, const lv_font_t *font, lv_color_t colour, lv_text_align_t align, int x1, int y1,
                  int x2, int y2, const char *string )
{
    const lv_area_t box = { x1, y1, x2, y2 };
    if ( !CycleDraw::inClip( layer, box ) ) {
        return;
    }
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init( &dsc );
    dsc.font = font;
    dsc.color = colour;
    dsc.align = align;
    dsc.text = string;
    dsc.text_local = 1;
    lv_draw_label( layer, &dsc, &box );
}

static void centred( lv_layer_t *layer, const lv_font_t *font, lv_color_t colour, int y, const char *string )
{
    text( layer, font, colour, LV_TEXT_ALIGN_CENTER, 20, y, Panel::WIDTH - 21, y + font->line_height, string );
}

static void outline( lv_layer_t *layer, int x1, int y1, int x2, int y2, lv_color_t colour, int width, int radius,
                     lv_color_t fill, bool filled )
{
    const lv_area_t box = { x1, y1, x2, y2 };
    if ( !CycleDraw::inClip( layer, box ) ) {
        return;
    }
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init( &dsc );
    dsc.radius = radius;
    dsc.bg_color = fill;
    dsc.bg_opa = filled ? LV_OPA_COVER : LV_OPA_TRANSP;
    dsc.border_color = colour;
    dsc.border_width = width;
    lv_draw_rect( layer, &dsc, &box );
}

static lv_color_t faded( lv_color_t colour, float share )
{
    return lv_color_mix( colour, lv_color_black(), static_cast<uint8_t>( share * 255 ) );
}

// "1 Ara", "29 Kas".
static void shortDate( int day, char *out, size_t size )
{
    int year = 0;
    int month = 0;
    int date = 0;
    Dates::civil( day, year, month, date );
    snprintf( out, size, "%d %s", date, Language::shortMonth( month ) );
}

// ---- The spiral ----

void CyclePages::spiralPoint( int t, float offset, int &x, int &y, float &angle ) const
{
    // One turn a Moon month: today at its phase's angle (new moon at the top), 52 px out a turn.
    const float start = static_cast<float>( -90 + m_moonAge / s_synodic * 360 );
    angle = start + static_cast<float>( ( t + 0.5 ) * 360 / s_synodic );
    const float r = static_cast<float>( 112 + ( t + 0.5 ) * 52 / s_synodic ) + offset;
    const float a = angle * static_cast<float>( M_PI ) / 180.0f;
    x = s_cx + static_cast<int>( lroundf( r * cosf( a ) ) );
    y = s_cy + static_cast<int>( lroundf( r * sinf( a ) ) );
}

// The current cycle's day of local day `day`, 0 before the current start.
static int cycleDayOf( const Cycle::State &s, int day )
{
    if ( s.count == 0 || day < s.periods[s.count - 1].start ) {
        return 0;
    }
    return day - s.periods[s.count - 1].start + 1;
}

void CyclePages::drawSpiral( lv_layer_t *layer, const lv_area_t &area ) const
{
    const Cycle::State &s = m_state;
    // The Moon's phases around it, fixed: one turn is one Moon month.
    for ( int i = 0; i < 30; ++i ) {
        const float a = static_cast<float>( ( -90 + i * 12 ) * M_PI / 180 );
        CycleDraw::moon( layer, s_cx + static_cast<int>( 182 * cosf( a ) ), s_cy + static_cast<int>( 182 * sinf( a ) ), 5,
                         i / 30.0, lv_color_hex( 0xBEBEC8 ), lv_color_hex( 0x232328 ) );
    }
    const int start = s.count > 0 ? s.periods[s.count - 1].start : 0;
    for ( int t = -18; t <= 24; ++t ) {
        // Both ends fade out: a piece of an endless path.
        float share = 1.0f;
        if ( t < -12 ) {
            share = 1 - ( -12 - t ) * 0.12f;
        } else if ( t > 17 ) {
            share = 1 - ( t - 17 ) * 0.13f;
        }
        share = share < 0.2f ? 0.2f : share;
        const int day = m_today + t;
        const float a0 = static_cast<float>( -90 + m_moonAge / s_synodic * 360 + t * 360 / s_synodic ) + 1.6f;
        const float a1 = a0 + static_cast<float>( 360 / s_synodic ) - 3.2f;
        const int r = static_cast<int>( 112 + ( t + 0.5 ) * 52 / s_synodic );
        CycleDraw::dayBox( layer, s_cx, s_cy, r, a0, a1, t <= 0, faded( CycleDraw::grey(), share ) );
        int x = 0;
        int y = 0;
        float angle = 0;
        const int cycleDay = cycleDayOf( s, day );
        if ( t <= 0 ) {
            const int flow = m_log->flow( day );
            if ( flow > 0 ) {
                spiralPoint( t, -18, x, y, angle );
                CycleDraw::drop( layer, x, y, CycleDraw::flowScale( flow ), faded( CycleDraw::red(), share ), false );
            }
        } else if ( s.nextEarly > 0 && start > 0 ) {
            const int n = day - start;
            if ( n >= s.nextEarly && n <= s.nextLate ) {
                const bool middle = abs( n - s.nextMean ) <= 1;
                spiralPoint( t, -18, x, y, angle );
                const lv_color_t colour = middle ? CycleDraw::dimRed() : faded( CycleDraw::dimRed(), 0.7f );
                CycleDraw::drop( layer, x, y, 1.0f, faded( colour, share ), m_night );
            }
        }
        const bool fertile = cycleDay > 0 && s.fertileFirst > 0 && cycleDay >= s.fertileFirst && cycleDay <= s.fertileLast &&
                             ( s.nextMean == 0 || day - start < s.nextMean );
        spiralPoint( t, 18, x, y, angle );
        if ( t <= 0 && ( m_log->get( day ) & CycleLog::Intercourse ) ) {
            const lv_color_t colour = fertile ? CycleDraw::blue() : CycleDraw::grey();
            CycleDraw::spermEgg( layer, x, y, static_cast<int>( angle ), faded( colour, share ) );
        } else if ( fertile ) {
            const lv_color_t colour = t <= 0 ? CycleDraw::blue() : CycleDraw::dimBlue();
            CycleDraw::egg( layer, x, y, cycleDay == s.ovulationDay, faded( colour, share ) );
        }
    }
    // The arrow from the day's number to today's box.
    int x = 0;
    int y = 0;
    float angle = 0;
    spiralPoint( 0, 0, x, y, angle );
    const float a = angle * static_cast<float>( M_PI ) / 180.0f;
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init( &line );
    line.color = white();
    line.width = 2;
    line.p1.x = s_cx + static_cast<int>( 62 * cosf( a ) );
    line.p1.y = s_cy + static_cast<int>( 62 * sinf( a ) );
    line.p2.x = s_cx + static_cast<int>( 88 * cosf( a ) );
    line.p2.y = s_cy + static_cast<int>( 88 * sinf( a ) );
    lv_draw_line( layer, &line );
    lv_draw_triangle_dsc_t head;
    lv_draw_triangle_dsc_init( &head );
    head.color = white();
    const float side = a + static_cast<float>( M_PI ) / 2;
    head.p[0].x = s_cx + static_cast<int>( 96 * cosf( a ) );
    head.p[0].y = s_cy + static_cast<int>( 96 * sinf( a ) );
    head.p[1].x = s_cx + static_cast<int>( 86 * cosf( a ) + 5 * cosf( side ) );
    head.p[1].y = s_cy + static_cast<int>( 86 * sinf( a ) + 5 * sinf( side ) );
    head.p[2].x = s_cx + static_cast<int>( 86 * cosf( a ) - 5 * cosf( side ) );
    head.p[2].y = s_cy + static_cast<int>( 86 * sinf( a ) - 5 * sinf( side ) );
    lv_draw_triangle( layer, &head );

    char line1[24];
    if ( s.cycleDay > 0 ) {
        snprintf( line1, sizeof( line1 ), "%d.", s.cycleDay );
        centred( layer, &jetbrains_mono_48, white(), s_cy - 34, line1 );
        centred( layer, &jetbrains_mono_18, textGrey(), s_cy + 16, Language::text( Language::CycleDay ) );
    } else {
        centred( layer, &jetbrains_mono_16, textGrey(), s_cy - 10, Language::text( Language::CycleNoPeriod ) );
    }
    shortDate( m_today, line1, sizeof( line1 ) );
    centred( layer, &jetbrains_mono_18, white(), s_cy + 40, line1 );

    // Below: the carry-a-pad hint, the estimate's warning, the next period's range with a drop.
    if ( s.carryPad ) {
        centred( layer, &jetbrains_mono_16, CycleDraw::amber(), 408, Language::text( Language::CycleCarryPad ) );
    }
    centred( layer, &jetbrains_mono_14, CycleDraw::amber(), 432, Language::text( Language::CycleEstimate ) );
    char range[32];
    if ( s.nextEarly > 0 && start > 0 ) {
        char from[12];
        char to[12];
        shortDate( start + s.nextEarly, from, sizeof( from ) );
        shortDate( start + s.nextLate, to, sizeof( to ) );
        snprintf( range, sizeof( range ), "%s - %s", from, to );
    } else {
        snprintf( range, sizeof( range ), "%s", Language::text( Language::CycleTapToday ) );
    }
    const lv_font_t *font = s.nextEarly > 0 ? &jetbrains_mono_26 : &jetbrains_mono_16;
    // A monospaced font: the letters (UTF-8 lead bytes) times one advance.
    int letters = 0;
    for ( int i = 0; range[i] != '\0'; ++i ) {
        letters += ( range[i] & 0xC0 ) != 0x80 ? 1 : 0;
    }
    const int width = letters * static_cast<int>( lv_font_get_glyph_width( font, '0', 0 ) );
    const int left = ( Panel::WIDTH - width - 24 ) / 2;
    CycleDraw::drop( layer, left + 8, 470, 1.6f, CycleDraw::red(), false );
    text( layer, font, CycleDraw::red(), LV_TEXT_ALIGN_LEFT, left + 24, 456, Panel::WIDTH - 10, 456 + font->line_height,
          range );
    (void)area;
}

// ---- The calendar ----

static void shownMonth( int today, int step, int &year, int &month )
{
    int date = 0;
    Dates::civil( today, year, month, date );
    month += step;
    while ( month > 12 ) {
        month -= 12;
        ++year;
    }
    while ( month < 1 ) {
        month += 12;
        --year;
    }
}

int CyclePages::calendarDayAt( const lv_point_t &point ) const
{
    int year = 0;
    int month = 0;
    shownMonth( m_today, m_monthStep, year, month );
    const int first = Dates::dayNumber( year, month, 1 );
    const int column = ( point.x - 36 ) / 48;
    const int row = ( point.y - 112 ) / 56;
    if ( point.x < 36 || column > 6 || point.y < 112 || row > 5 ) {
        return 0;
    }
    const int date = row * 7 + column - Dates::weekday( first ) + 1;
    return date >= 1 && date <= Dates::daysInMonth( year, month ) ? first + date - 1 : 0;
}

void CyclePages::drawCalendar( lv_layer_t *layer, const lv_area_t &area ) const
{
    const Cycle::State &s = m_state;
    int year = 0;
    int month = 0;
    shownMonth( m_today, m_monthStep, year, month );
    char title[32];
    snprintf( title, sizeof( title ), "%s %d", Language::monthName( month ), year );
    centred( layer, &jetbrains_mono_26, white(), 48, title );
    for ( int column = 0; column < 7; ++column ) {
        const int x = 60 + column * 48;
        text( layer, &jetbrains_mono_18, textGrey(), LV_TEXT_ALIGN_CENTER, x - 24, 88, x + 24, 110,
              Language::dayInitials( column ) );
    }
    const int first = Dates::dayNumber( year, month, 1 );
    const int offset = Dates::weekday( first );
    const int length = Dates::daysInMonth( year, month );
    const int start = s.count > 0 ? s.periods[s.count - 1].start : 0;
    for ( int date = 1; date <= length; ++date ) {
        const int day = first + date - 1;
        const int cell = offset + date - 1;
        const int x = 60 + cell % 7 * 48;
        const int y = 132 + cell / 7 * 56;
        if ( day == m_today ) {
            outline( layer, x - 22, y - 16, x + 22, y + 34, white(), 2, 0, lv_color_black(), false );
        }
        char number[12];
        snprintf( number, sizeof( number ), "%d", date );
        text( layer, &jetbrains_mono_18, white(), LV_TEXT_ALIGN_CENTER, x - 22, y - 12, x + 22, y + 10, number );
        // The marks under the number, side by side.
        int kinds[3] = {};
        int count = 0;
        const uint8_t value = m_log->get( day );
        const int cycleDay = cycleDayOf( s, day );
        const bool fertile = cycleDay > 0 && s.fertileFirst > 0 && cycleDay >= s.fertileFirst &&
                             cycleDay <= s.fertileLast && ( s.nextMean == 0 || day - start < s.nextMean );
        if ( ( value & CycleLog::FlowMask ) != 0 ) {
            kinds[count++] = 1;  // a logged drop
        } else if ( day > m_today && s.nextEarly > 0 && start > 0 && day - start >= s.nextEarly && day - start <= s.nextLate ) {
            kinds[count++] = 2;  // an estimated drop
        }
        if ( value & CycleLog::Intercourse ) {
            kinds[count++] = 3;
        } else if ( fertile ) {
            kinds[count++] = 4;
        }
        if ( ( value & CycleLog::MadeUpFast ) && count < 3 ) {
            kinds[count++] = 5;
        }
        int mx = x - 7 * ( count - 1 );
        for ( int k = 0; k < count; ++k ) {
            switch ( kinds[k] ) {
            case 1:
                CycleDraw::drop( layer, mx, y + 22, CycleDraw::flowScale( value & CycleLog::FlowMask ) * 0.85f,
                                 CycleDraw::red(), false );
                break;
            case 2:
                CycleDraw::drop( layer, mx, y + 22, 0.85f, CycleDraw::dimRed(), m_night );
                break;
            case 3:
                CycleDraw::spermEgg( layer, mx, y + 22, 0, fertile ? CycleDraw::blue() : CycleDraw::grey() );
                break;
            case 4:
                CycleDraw::egg( layer, mx, y + 22, cycleDay == s.ovulationDay,
                                day <= m_today ? CycleDraw::blue() : CycleDraw::dimBlue() );
                break;
            default:
                // A make-up fast: a small crescent.
                CycleDraw::moon( layer, mx, y + 22, 5, 0.12, CycleDraw::amber(), lv_color_black() );
                break;
            }
            mx += 14;
        }
    }
    text( layer, &jetbrains_mono_26, textGrey(), LV_TEXT_ALIGN_CENTER, s_cx - 90, 458, s_cx - 50, 490, "<" );
    text( layer, &jetbrains_mono_26, textGrey(), LV_TEXT_ALIGN_CENTER, s_cx + 50, 458, s_cx + 90, 490, ">" );
    (void)area;
}

// ---- History ----

void CyclePages::drawHistory( lv_layer_t *layer, const lv_area_t &area ) const
{
    const Cycle::State &s = m_state;
    const int x0 = 64;
    const float px = 7.2f;
    static const int s_marks[6] = { 1, 8, 15, 22, 29, 36 };
    for ( int i = 0; i < 6; ++i ) {
        char number[12];
        snprintf( number, sizeof( number ), "%d", s_marks[i] );
        const int x = x0 + static_cast<int>( ( s_marks[i] - 1 ) * px );
        text( layer, &jetbrains_mono_14, textGrey(), LV_TEXT_ALIGN_LEFT, x - 2, 52, x + 30, 70, number );
    }
    const int rows = s.count < 8 ? s.count : 8;
    uint16_t warnings = 0;
    for ( int r = 0; r < rows; ++r ) {
        const Cycle::Period &p = s.periods[s.count - rows + r];
        const bool current = r == rows - 1;
        const int y = 84 + r * 34;
        int year = 0;
        int month = 0;
        int date = 0;
        Dates::civil( p.start, year, month, date );
        text( layer, &jetbrains_mono_16, textGrey(), LV_TEXT_ALIGN_LEFT, 14, y - 1, 62, y + 19, Language::shortMonth( month ) );
        const int length = current ? s.cycleDay : p.cycleLength;
        const int drawn = length < 40 ? length : 40;
        for ( int d = 1; d <= drawn; ++d ) {
            const int x = x0 + static_cast<int>( ( d - 1 ) * px );
            const lv_color_t colour = d <= p.length && m_log->flow( p.start + d - 1 ) > 0
                                          ? CycleDraw::red()
                                          : ( current ? CycleDraw::grey() : CycleDraw::dimGrey() );
            outline( layer, x, y, x + static_cast<int>( px ) - 2, y + 18, colour, 0, 0, colour, true );
        }
        if ( current ) {
            for ( int d = drawn + 1; d <= s.nextLate + 1 && d <= 40; ++d ) {
                const int x = x0 + static_cast<int>( ( d - 1 ) * px );
                const bool likely = s.nextEarly > 0 && d - 1 >= s.nextEarly && d - 1 <= s.nextLate;
                outline( layer, x, y, x + static_cast<int>( px ) - 2, y + 18,
                         likely ? CycleDraw::dimRed() : CycleDraw::dimGrey(), 1, 0, lv_color_black(), false );
            }
            const int tx = x0 + static_cast<int>( ( drawn - 0.5f ) * px );
            lv_draw_triangle_dsc_t arrow;
            lv_draw_triangle_dsc_init( &arrow );
            arrow.color = white();
            arrow.p[0] = { tx, y + 22 };
            arrow.p[1] = { tx - 6, y + 31 };
            arrow.p[2] = { tx + 6, y + 31 };
            lv_draw_triangle( layer, &arrow );
        } else {
            char number[16];
            snprintf( number, sizeof( number ), p.warnings ? "%d!" : "%d", p.cycleLength );
            const int x = x0 + static_cast<int>( drawn * px ) + 6;
            text( layer, &jetbrains_mono_16, p.warnings ? CycleDraw::amber() : white(), LV_TEXT_ALIGN_LEFT, x, y - 1,
                  x + 60, y + 19, number );
        }
        warnings |= p.warnings;
    }
    if ( s.complete > 0 ) {
        char value[32];
        snprintf( value, sizeof( value ), Language::text( Language::HistoryDays ), s.meanLength );
        text( layer, &jetbrains_mono_18, textGrey(), LV_TEXT_ALIGN_LEFT, 40, 360, 200, 384, Language::text( Language::HistoryMean ) );
        text( layer, &jetbrains_mono_18, white(), LV_TEXT_ALIGN_RIGHT, 200, 360, Panel::WIDTH - 40, 384, value );
        snprintf( value, sizeof( value ), "%d-%d (±%.1f)", s.shortest, s.longest, s.spread );
        text( layer, &jetbrains_mono_18, textGrey(), LV_TEXT_ALIGN_LEFT, 40, 386, 200, 410, Language::text( Language::HistorySpread ) );
        text( layer, &jetbrains_mono_18, white(), LV_TEXT_ALIGN_RIGHT, 160, 386, Panel::WIDTH - 40, 410, value );
        centred( layer, &jetbrains_mono_18, white(), 412,
                 Language::text( s.irregular ? Language::HistoryIrregular : Language::HistoryRegular ) );
    }
    // The warnings found, the first two, and whom to see.
    warnings |= s.warnings;
    if ( warnings != 0 ) {
        static const Language::Text s_names[6] = { Language::WarnShort, Language::WarnLong, Language::WarnLongBleeding,
                                                   Language::WarnNoPeriod, Language::WarnHeavy, Language::WarnBetween };
        char line[96] = "";
        int shown = 0;
        for ( int i = 0; i < 6 && shown < 2; ++i ) {
            if ( warnings & ( 1 << i ) ) {
                const size_t used = strlen( line );
                snprintf( line + used, sizeof( line ) - used, "%s%s", shown ? ", " : "! ", Language::text( s_names[i] ) );
                ++shown;
            }
        }
        centred( layer, &jetbrains_mono_14, CycleDraw::amber(), 440, line );
        centred( layer, &jetbrains_mono_14, CycleDraw::amber(), 458,
                 Language::text( s.teen ? Language::WarnDoctorTeen : Language::WarnDoctor ) );
    }
    (void)area;
}

// ---- With the Moon ----

void CyclePages::drawMoon( lv_layer_t *layer, const lv_area_t &area ) const
{
    const Cycle::State &s = m_state;
    const int x0 = 50;
    const float px = 10.3f;
    // The new moon of the current Moon month, then seven before it.
    const double lastNew = m_today + 0.5 - m_moonAge;
    static const int s_phaseDays[5] = { 0, 7, 15, 22, 29 };
    for ( int i = 0; i < 5; ++i ) {
        CycleDraw::moon( layer, x0 + static_cast<int>( s_phaseDays[i] * px ), 58, 7, s_phaseDays[i] / s_synodic,
                         lv_color_hex( 0xBEBEC8 ), lv_color_hex( 0x232328 ) );
    }
    for ( int row = 0; row < 8; ++row ) {
        const double rowStart = lastNew - ( 7 - row ) * s_synodic;
        const int y = 92 + row * 34;
        outline( layer, x0, y + 9, x0 + static_cast<int>( s_synodic * px ), y + 9, CycleDraw::dimGrey(), 0, 0,
                 CycleDraw::dimGrey(), true );
        int year = 0;
        int month = 0;
        int date = 0;
        Dates::civil( static_cast<int>( rowStart ), year, month, date );
        text( layer, &jetbrains_mono_16, textGrey(), LV_TEXT_ALIGN_LEFT, 6, y - 1, 48, y + 19, Language::shortMonth( month ) );
        for ( int i = 0; i < s.count; ++i ) {
            const double offset = s.periods[i].start + 0.5 - rowStart;
            if ( offset >= 0 && offset < s_synodic ) {
                const int x = x0 + static_cast<int>( offset * px );
                outline( layer, x - 6, y + 3, x + 6, y + 15, CycleDraw::red(), 0, LV_RADIUS_CIRCLE, CycleDraw::red(), true );
            }
        }
    }
    centred( layer, &jetbrains_mono_16, textGrey(), 374, Language::text( Language::MoonRowText ) );
    centred( layer, &jetbrains_mono_16, textGrey(), 396, Language::text( Language::MoonDotText ) );
    text( layer, &jetbrains_mono_16, CycleDraw::amber(), LV_TEXT_ALIGN_CENTER, 40, 428, Panel::WIDTH - 40, 480,
          Language::text( Language::MoonNote ) );
    (void)area;
}

// ---- Pregnancy ----

void CyclePages::drawPregnancy( lv_layer_t *layer, const lv_area_t &area ) const
{
    const Cycle::State &s = m_state;
    const int start = s.count > 0 ? s.periods[s.count - 1].start : 0;
    const int x0 = 30;
    const float cw = ( Panel::WIDTH - 60 ) / 32.0f;
    if ( start > 0 ) {
        for ( int day = 1; day <= 32; ++day ) {
            const int x = x0 + static_cast<int>( ( day - 1 ) * cw );
            const bool passed = day <= s.cycleDay;
            outline( layer, x + 1, 110, x + static_cast<int>( cw ) - 1, 130, CycleDraw::grey(), passed ? 0 : 1, 0,
                     CycleDraw::grey(), passed );
            const int cx = x + static_cast<int>( cw / 2 );
            const int flow = m_log->flow( start + day - 1 );
            if ( passed && flow > 0 ) {
                CycleDraw::drop( layer, cx, 148, 0.7f, CycleDraw::red(), false );
            } else if ( !passed && s.nextEarly > 0 && day - 1 >= s.nextEarly && day - 1 <= s.nextLate ) {
                CycleDraw::drop( layer, cx, 148, 0.7f, CycleDraw::dimRed(), m_night );
            }
            if ( s.fertileFirst > 0 && day >= s.fertileFirst && day <= s.fertileLast ) {
                CycleDraw::egg( layer, cx, 94, day == s.ovulationDay, passed ? CycleDraw::blue() : CycleDraw::dimBlue() );
            }
        }
        const int tx = x0 + static_cast<int>( ( s.cycleDay - 0.5f ) * cw );
        if ( s.cycleDay <= 32 ) {
            lv_draw_triangle_dsc_t arrow;
            lv_draw_triangle_dsc_init( &arrow );
            arrow.color = white();
            arrow.p[0] = { tx, 162 };
            arrow.p[1] = { tx - 6, 174 };
            arrow.p[2] = { tx + 6, 174 };
            lv_draw_triangle( layer, &arrow );
        }
    }
    int y = 196;
    char from[12];
    char to[12];
    char value[32];
    if ( s.teen ) {
        text( layer, &jetbrains_mono_18, CycleDraw::amber(), LV_TEXT_ALIGN_CENTER, 36, y, Panel::WIDTH - 36, y + 80,
              Language::text( Language::PregTeen ) );
        y += 92;
    } else if ( start > 0 && s.fertileFirst > 0 ) {
        shortDate( start + s.fertileFirst - 1, from, sizeof( from ) );
        shortDate( start + s.fertileLast - 1, to, sizeof( to ) );
        snprintf( value, sizeof( value ), "%s-%s", from, to );
        CycleDraw::egg( layer, 48, y + 12, false, CycleDraw::blue() );
        text( layer, &jetbrains_mono_18, textGrey(), LV_TEXT_ALIGN_LEFT, 66, y, 220, y + 24, Language::text( Language::PregFertile ) );
        text( layer, &jetbrains_mono_18, white(), LV_TEXT_ALIGN_RIGHT, 150, y, Panel::WIDTH - 36, y + 24, value );
        y += 40;
        if ( s.ovulationDay > 0 ) {
            shortDate( start + s.ovulationDay - 1, value, sizeof( value ) );
            CycleDraw::egg( layer, 48, y + 12, true, CycleDraw::blue() );
            text( layer, &jetbrains_mono_18, textGrey(), LV_TEXT_ALIGN_LEFT, 66, y, 220, y + 24, Language::text( Language::PregOvulation ) );
            text( layer, &jetbrains_mono_18, white(), LV_TEXT_ALIGN_RIGHT, 150, y, Panel::WIDTH - 36, y + 24, value );
            y += 40;
        }
    }
    if ( start > 0 && s.nextEarly > 0 ) {
        shortDate( start + s.nextEarly, from, sizeof( from ) );
        shortDate( start + s.nextLate, to, sizeof( to ) );
        snprintf( value, sizeof( value ), "%s-%s", from, to );
        CycleDraw::drop( layer, 48, y + 12, 1.0f, CycleDraw::red(), false );
        text( layer, &jetbrains_mono_18, textGrey(), LV_TEXT_ALIGN_LEFT, 66, y, 220, y + 24, Language::text( Language::PregNext ) );
        text( layer, &jetbrains_mono_18, white(), LV_TEXT_ALIGN_RIGHT, 150, y, Panel::WIDTH - 36, y + 24, value );
    }
    text( layer, &jetbrains_mono_16, CycleDraw::amber(), LV_TEXT_ALIGN_CENTER, 34, 340, Panel::WIDTH - 34, 470,
          Language::text( Language::PregWarning ) );
    (void)area;
}

// ---- Hayız ----

static void row( lv_layer_t *layer, int y, const char *name, const char *value, lv_color_t colour )
{
    text( layer, &jetbrains_mono_26, textGrey(), LV_TEXT_ALIGN_LEFT, 40, y, Panel::WIDTH - 40, y + 32, name );
    text( layer, &jetbrains_mono_26, colour, LV_TEXT_ALIGN_RIGHT, 140, y, Panel::WIDTH - 40, y + 32, value );
}

void CyclePages::drawHayiz( lv_layer_t *layer, const lv_area_t &area ) const
{
    const Hayiz::Today t = Hayiz::today( *m_log, m_today );
    char value[24];
    const Language::Text names[3] = { Language::HayizPurity, Language::HayizNow, Language::HayizIstihaze };
    if ( t.day > 0 ) {
        snprintf( value, sizeof( value ), Language::text( Language::HayizDayFmt ), t.day );
    } else {
        value[0] = '\0';
    }
    const bool hayz = t.kind == Hayiz::Hayz;
    row( layer, 64, Language::text( names[t.kind] ), value, hayz ? CycleDraw::red() : white() );
    row( layer, 112, Language::text( Language::HayizPrayer ),
         Language::text( hayz ? Language::HayizPrayerNot : Language::HayizPrayerYes ), white() );
    row( layer, 160, Language::text( Language::HayizFasting ),
         Language::text( hayz ? Language::HayizFastNot : Language::HayizFastYes ), white() );
    snprintf( value, sizeof( value ), Language::text( Language::HistoryDays ), 10 );
    row( layer, 208, Language::text( Language::HayizMost ), value, textGrey() );
    snprintf( value, sizeof( value ), Language::text( Language::HistoryDays ), 15 );
    row( layer, 256, Language::text( Language::HayizLeast ), value, textGrey() );
    snprintf( value, sizeof( value ), Language::text( Language::HistoryDays ), Hayiz::balance( *m_log ) );
    row( layer, 304, Language::text( Language::HayizMakeUp ), value, CycleDraw::amber() );
    text( layer, &jetbrains_mono_18, CycleDraw::amber(), LV_TEXT_ALIGN_CENTER, 40, 380, Panel::WIDTH - 40, 450,
          Language::text( Language::HayizAsk ) );
    (void)area;
}

// ---- Settings ----

void CyclePages::drawSettings( lv_layer_t *layer, const lv_area_t &area ) const
{
    char value[24];
    for ( int r = 0; r < 6; ++r ) {
        const int y = 80 + r * 64;
        outline( layer, 46, y, Panel::WIDTH - 47, y + 56, lv_color_hex( 0x2C2C2C ), 1, 12, lv_color_black(), false );
        const char *name = "";
        const char *shown = "";
        lv_color_t colour = white();
        switch ( r ) {
        case 0:
            name = Language::text( Language::CyclePinRow );
            shown = Language::text( m_log->pinHash != 0 ? Language::On : Language::Off );
            break;
        case 1:
            name = Language::text( Language::CyclePinRow );
            shown = Language::text( Language::CycleChange );
            colour = m_log->pinHash != 0 ? white() : CycleDraw::dimGrey();
            break;
        case 2:
            name = Language::text( Language::CycleTeenRow );
            shown = m_log->teenMode == CycleLog::TeenAuto ? Language::text( Language::CycleTeenAuto )
                                                          : Language::text( m_log->teenMode ? Language::On : Language::Off );
            break;
        case 3:
            name = Language::text( Language::CycleFirstRow );
            if ( m_log->firstPeriodDay != 0 ) {
                int year = 0;
                int month = 0;
                int date = 0;
                Dates::civil( m_log->firstPeriodDay, year, month, date );
                snprintf( value, sizeof( value ), "%s %d", Language::shortMonth( month ), year );
                shown = value;
            } else {
                shown = Language::text( Language::CycleNotSet );
            }
            colour = m_log->teenMode == CycleLog::TeenAuto ? white() : CycleDraw::dimGrey();
            break;
        case 4:
            name = Language::text( Language::CycleEarlierRow );
            snprintf( value, sizeof( value ), "%d", static_cast<int>( m_log->earlierBalance ) );
            shown = value;
            colour = m_hayizShown ? white() : CycleDraw::dimGrey();
            break;
        default:
            name = Language::text( Language::CycleDeleteRow );
            shown = m_deleteArmedUs != 0 ? Language::text( Language::ResetAgain ) : "";
            colour = CycleDraw::red();
            break;
        }
        text( layer, &jetbrains_mono_26, white(), LV_TEXT_ALIGN_LEFT, 60, y + 12, 260, y + 44, name );
        text( layer, &jetbrains_mono_26, colour, LV_TEXT_ALIGN_RIGHT, 160, y + 12, Panel::WIDTH - 60, y + 44, shown );
    }
    (void)area;
}

// ---- Overlays ----

static void key( lv_layer_t *layer, int x1, int y1, int x2, int y2, bool checked )
{
    outline( layer, x1, y1, x2, y2, checked ? lv_color_hex( 0xE6E6E6 ) : lv_color_hex( 0x5A5A5A ), checked ? 3 : 2, 12,
             lv_color_hex( 0x1E1E1E ), checked );
}

void CyclePages::drawEntry( lv_layer_t *layer, const lv_area_t &area ) const
{
    outline( layer, 20, 46, Panel::WIDTH - 21, 494, lv_color_hex( 0x505050 ), 2, 18, lv_color_hex( 0x0A0A0A ), true );
    char title[32];
    char date[16];
    shortDate( m_entryDay, date, sizeof( date ) );
    if ( m_entryDay == m_today ) {
        snprintf( title, sizeof( title ), Language::text( Language::EntryToday ), date );
    } else {
        int year = 0;
        int month = 0;
        int dayOfMonth = 0;
        Dates::civil( m_entryDay, year, month, dayOfMonth );
        snprintf( title, sizeof( title ), "%s %s", date, Language::dayName( Dates::weekday( m_entryDay ) ) );
    }
    centred( layer, &jetbrains_mono_26, white(), 58, title );
    centred( layer, &jetbrains_mono_18, textGrey(), 104, Language::text( Language::EntryBleeding ) );
    static const Language::Text s_flows[4] = { Language::FlowNone, Language::FlowLight, Language::FlowMedium, Language::FlowHeavy };
    const int flow = m_entryValue & CycleLog::FlowMask;
    for ( int i = 0; i < 4; ++i ) {
        const int x1 = 44 + i * 82;
        key( layer, x1, 134, x1 + 72, 222, i == flow );
        const int cx = x1 + 36;
        if ( i == 0 ) {
            outline( layer, cx - 10, 166, cx + 10, 168, textGrey(), 0, 0, textGrey(), true );
        } else {
            CycleDraw::drop( layer, cx, 168, CycleDraw::flowScale( i ), CycleDraw::red(), false );
        }
        text( layer, &jetbrains_mono_18, white(), LV_TEXT_ALIGN_CENTER, x1, 190, x1 + 72, 214, Language::text( s_flows[i] ) );
    }
    // Intercourse (not in the adolescent mode) and the make-up fast (with the Hayız part).
    const bool together = ( m_entryValue & CycleLog::Intercourse ) != 0;
    const bool madeUp = ( m_entryValue & CycleLog::MadeUpFast ) != 0;
    if ( !m_state.teen ) {
        text( layer, &jetbrains_mono_16, textGrey(), LV_TEXT_ALIGN_CENTER, 40, 250, 200, 272, Language::text( Language::EntryIntercourse ) );
        key( layer, 44, 276, 196, 350, together );
        if ( together ) {
            CycleDraw::spermEgg( layer, 80, 313, 0, CycleDraw::blue() );
        } else {
            CycleDraw::egg( layer, 80, 313, false, CycleDraw::dimGrey() );
        }
        text( layer, &jetbrains_mono_26, white(), LV_TEXT_ALIGN_LEFT, 100, 298, 196, 330,
              Language::text( together ? Language::EntryYes : Language::EntryNo ) );
    }
    if ( m_hayizShown ) {
        text( layer, &jetbrains_mono_16, textGrey(), LV_TEXT_ALIGN_CENTER, 210, 250, 370, 272, Language::text( Language::EntryMadeUp ) );
        key( layer, 214, 276, 366, 350, madeUp );
        CycleDraw::moon( layer, 250, 313, 7, 0.12, madeUp ? CycleDraw::amber() : CycleDraw::dimGrey(), lv_color_hex( 0x0A0A0A ) );
        text( layer, &jetbrains_mono_26, white(), LV_TEXT_ALIGN_LEFT, 270, 298, 366, 330,
              Language::text( madeUp ? Language::EntryYes : Language::EntryNo ) );
    }
    outline( layer, 110, 390, 300, 450, lv_color_hex( 0xC8C8C8 ), 2, 14, lv_color_black(), false );
    centred( layer, &jetbrains_mono_26, white(), 404, Language::text( Language::EntryOk ) );
    (void)area;
}

void CyclePages::drawLegend( lv_layer_t *layer, const lv_area_t &area ) const
{
    int y = 64;
    const Language::Text names[8] = { Language::LegendPast,       Language::LegendFuture,     Language::LegendBleeding,
                                      Language::LegendBleedingEst, Language::LegendFertile,   Language::LegendFertileEst,
                                      Language::LegendOvulation,  Language::LegendIntercourse };
    for ( int i = 0; i < 8; ++i ) {
        const int cy = y + 14;
        switch ( i ) {
        case 0:
            outline( layer, 50, cy - 8, 90, cy + 8, CycleDraw::grey(), 0, 0, CycleDraw::grey(), true );
            break;
        case 1:
            outline( layer, 50, cy - 8, 90, cy + 8, CycleDraw::grey(), 2, 0, lv_color_black(), false );
            break;
        case 2:
            CycleDraw::drop( layer, 70, cy, 1.0f, CycleDraw::red(), false );
            break;
        case 3:
            CycleDraw::drop( layer, 70, cy, 1.0f, CycleDraw::dimRed(), m_night );
            break;
        case 4:
            CycleDraw::egg( layer, 70, cy, false, CycleDraw::blue() );
            break;
        case 5:
            CycleDraw::egg( layer, 70, cy, false, CycleDraw::dimBlue() );
            break;
        case 6:
            CycleDraw::egg( layer, 70, cy, true, CycleDraw::blue() );
            break;
        default:
            CycleDraw::spermEgg( layer, 66, cy, 0, CycleDraw::blue() );
            break;
        }
        text( layer, &jetbrains_mono_18, white(), LV_TEXT_ALIGN_LEFT, 110, y + 2, Panel::WIDTH - 30, y + 26, Language::text( names[i] ) );
        y += 44;
    }
    (void)area;
}

void CyclePages::drawNormal( lv_layer_t *layer, const lv_area_t &area ) const
{
    const bool teen = m_state.teen;
    centred( layer, &jetbrains_mono_26, white(), 180, Language::text( teen ? Language::CycleNormalTeen : Language::CycleNormalAdult ) );
    centred( layer, &jetbrains_mono_26, white(), 226, Language::text( teen ? Language::CycleBleedTeen : Language::CycleBleedAdult ) );
    if ( teen ) {
        centred( layer, &jetbrains_mono_26, white(), 272, Language::text( Language::CycleIrregularYears ) );
    }
    (void)area;
}

void CyclePages::drawPinHeader( lv_layer_t *layer, const lv_area_t &area ) const
{
    Language::Text title = Language::PinTitle;
    switch ( m_input ) {
    case NewPin:
        title = Language::PinSet;
        break;
    case RepeatPin:
        title = Language::PinAgain;
        break;
    case OldPinOff:
    case OldPinChange:
        title = Language::PinOld;
        break;
    case FirstPeriod:
        title = Language::CycleFirstRow;
        break;
    case EarlierFasts:
        title = Language::CycleEarlierRow;
        break;
    default:
        break;
    }
    centred( layer, &jetbrains_mono_26, white(), 44, Language::text( title ) );
    if ( waitLeftSeconds() > 0 ) {
        char wait[24];
        const int seconds = static_cast<int>( waitLeftSeconds() );
        snprintf( wait, sizeof( wait ), Language::text( Language::PinWait ), seconds > 0 ? seconds : 0 );
        centred( layer, &jetbrains_mono_18, CycleDraw::amber(), 82, wait );
    } else if ( m_input <= OldPinChange ) {
        const int typed = static_cast<int>( strlen( m_typed ) );
        for ( int i = 0; i < 4; ++i ) {
            const int x = s_cx - 45 + i * 30;
            outline( layer, x - 7, 82, x + 7, 96, white(), 2, LV_RADIUS_CIRCLE, white(), i < typed );
        }
    } else {
        centred( layer, &jetbrains_mono_26, white(), 76, m_typed[0] != '\0' ? m_typed : ( m_input == FirstPeriod ? "2025.9" : "0" ) );
    }
    // The first offer of a PIN can be declined.
    if ( m_input == NewPin && m_log->pinHash == 0 && m_log->pinAsked == 0 ) {
        outline( layer, s_cx - 70, 108, s_cx + 70, 142, lv_color_hex( 0x808080 ), 2, 10, lv_color_black(), false );
        centred( layer, &jetbrains_mono_18, white(), 113, Language::text( Language::PinSkip ) );
    }
    (void)area;
}
