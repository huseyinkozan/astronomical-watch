#include "ToolParts.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Fonts.h"
#include "Language.h"
#include "Ringer.h"
#include "Styles.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "tools";

static const lv_color_t s_grey = LV_COLOR_MAKE( 0xA0, 0xA0, 0xA0 );
static const lv_color_t s_dim = LV_COLOR_MAKE( 0x5A, 0x5A, 0x5A );
static const lv_color_t s_dotOff = LV_COLOR_MAKE( 0x3A, 0x3A, 0x3A );
// Inside the corner gauges, as the other tools.
static const int s_left = 52;
static const int s_right = 358;

static const int s_minimumBpm = 30;
static const int s_maximumBpm = 240;
static const int s_cprBpm = 110;  // the middle of the 100-120 compressions a minute of CPR guidelines
static const int s_fastStep = 5;
// The codec's and the amplifier's start (Ringer: 150 ms of silence first) before the first beat.
static const int64_t s_startDelayUs = 250000;
// Active mode stays on this long after the last tap on the part while the metronome runs.
static const int64_t s_holdUs = 10 * 60 * 1000000LL;
// With no bar, the one dot is lit for this part of each beat.
static const double s_flashPhase = 0.3;
static const int s_dotRadius = 16;
static const int s_dotStep = 56;

// Kept over the always-on wakes.
RTC_DATA_ATTR static int s_bpm = 100;
RTC_DATA_ATTR static int s_beatsInBar = 4;  // 0: none, else 2, 3 or 4

// The emergency card's rows: the red title, five named rows, then the phone and the note as
// lines (the note moves up when there is no phone).
enum CardRow
{
    CardTitleRow,
    CardFirstNamed,
    CardPhoneRow = CardFirstNamed + 5,
    CardNoteRow,
    CardRowCount
};
static const uint32_t s_cardRed = 0xFF5050;

static lv_obj_t *label( lv_obj_t *parent, const lv_font_t *font, lv_color_t colour, int y, const char *text )
{
    lv_obj_t *object = lv_label_create( parent );
    Styles::text( object, font, colour );
    lv_label_set_text( object, text );
    lv_obj_align( object, LV_ALIGN_TOP_MID, 0, y );
    lv_obj_set_clickable( object, false );
    return object;
}

static lv_obj_t *key( lv_obj_t *part, int x1, int y1, int x2, int y2, const char *text, lv_event_cb_t onKey,
                      void *user )
{
    lv_obj_t *box = lv_obj_create( part );
    lv_obj_set_pos( box, x1, y1 );
    lv_obj_set_size( box, x2 - x1, y2 - y1 );
    lv_obj_set_scrollable( box, false );
    lv_obj_set_press_lock( box, false );
    Styles::button( box );
    lv_obj_add_event_cb( box, onKey, LV_EVENT_SHORT_CLICKED, user );
    lv_obj_t *name = lv_label_create( box );
    Styles::text( name, &jetbrains_mono_26, lv_color_white() );
    lv_label_set_text( name, text );
    lv_obj_center( name );
    return box;
}

static void setText( lv_obj_t *object, const char *text )
{
    if ( object != nullptr && strcmp( lv_label_get_text( object ), text ) != 0 ) {
        lv_label_set_text( object, text );
    }
}

static int64_t periodUs()
{
    return 60 * 1000000LL / s_bpm;
}

void ToolParts::build( lv_obj_t *metronome, lv_obj_t *card, const EmergencyCard &emergency )
{
    m_tempo = label( metronome, &jetbrains_mono_light_90, lv_color_white(), 30, "" );
    label( metronome, &jetbrains_mono_26, s_grey, 136, Language::text( Language::BeatsPerMinute ) );
    // The dots, and the tap that steps the beats in a bar.
    m_dots = lv_obj_create( metronome );
    lv_obj_remove_style_all( m_dots );
    lv_obj_set_pos( m_dots, s_left, 176 );
    lv_obj_set_size( m_dots, s_right - s_left, 60 );
    lv_obj_set_scrollable( m_dots, false );
    lv_obj_add_event_cb( m_dots, onDrawDots, LV_EVENT_DRAW_MAIN, this );
    lv_obj_add_event_cb( m_dots, onKeyEvent, LV_EVENT_SHORT_CLICKED, this );
    m_minus = key( metronome, s_left, 248, 146, 312, "-", onKeyEvent, this );
    m_cpr = key( metronome, 158, 248, 252, 312, Language::text( Language::MetronomeCpr ), onKeyEvent, this );
    m_plus = key( metronome, 264, 248, s_right, 312, "+", onKeyEvent, this );
    // A long press steps by 5, repeated while held.
    lv_obj_add_event_cb( m_minus, onKeyEvent, LV_EVENT_LONG_PRESSED_REPEAT, this );
    lv_obj_add_event_cb( m_plus, onKeyEvent, LV_EVENT_LONG_PRESSED_REPEAT, this );
    m_startStop = lv_obj_create( metronome );
    lv_obj_set_pos( m_startStop, s_left, 324 );
    lv_obj_set_size( m_startStop, s_right - s_left, 64 );
    lv_obj_set_scrollable( m_startStop, false );
    lv_obj_set_press_lock( m_startStop, false );
    Styles::accent( m_startStop );
    lv_obj_add_event_cb( m_startStop, onKeyEvent, LV_EVENT_SHORT_CLICKED, this );
    m_startLabel = lv_label_create( m_startStop );
    Styles::text( m_startLabel, &jetbrains_mono_26, lv_color_hex( Styles::Amber ) );
    lv_obj_center( m_startLabel );
    label( metronome, &jetbrains_mono_26, s_dim, 398, Language::text( Language::MetronomeHint ) );
    m_status = label( metronome, &jetbrains_mono_26, s_dim, 428, "" );
    m_litDot = -1;
    m_shownSeconds = -1;
    showMetronome();

    TablePart::Layout layout = {};
    layout.title = "";
    layout.rowCount = CardRowCount;
    for ( int i = 0; i < CardRowCount; ++i ) {
        layout.kinds[i] = i >= CardFirstNamed && i < CardPhoneRow ? TablePart::OneValue : TablePart::Line;
    }
    // A centred column: long values wrap in their cells, the later rows cut first.
    layout.top = -1;
    layout.pitch = 36;
    layout.nameX = 40;
    layout.nameWidth = 115;
    layout.valueX = 160;
    layout.valueWidth = 105;
    layout.maxHeight = 384;
    m_cardTable.build( card, layout );
    m_cardTable.text().setColours( CardTitleRow, s_cardRed, s_cardRed );
    m_cardTable.text().setColours( CardPhoneRow, Styles::Amber, Styles::Amber );
    showCard( emergency );
}

void ToolParts::forget()
{
    setMetronomeShown( false );
    m_tempo = nullptr;
    m_dots = nullptr;
    m_minus = nullptr;
    m_cpr = nullptr;
    m_plus = nullptr;
    m_startStop = nullptr;
    m_startLabel = nullptr;
    m_status = nullptr;
    m_cardTable.forget();
}

void ToolParts::showCard( const EmergencyCard &emergency )
{
    TableText &text = m_cardTable.text();
    text.setLine( CardTitleRow, Language::text( Language::CardTitle ) );
    const Language::Text names[5] = { Language::CardName, Language::CardBlood, Language::CardAllergies,
                                      Language::CardMedication, Language::CardContact };
    const EmergencyCard::Field fields[5] = { EmergencyCard::Name, EmergencyCard::BloodType, EmergencyCard::Allergies,
                                             EmergencyCard::Medication, EmergencyCard::ContactName };
    for ( int i = 0; i < 5; ++i ) {
        const char *value = emergency.fields[fields[i]];
        text.set( CardFirstNamed + i, 0, Language::text( names[i] ) );
        text.set( CardFirstNamed + i, 1, value[0] != '\0' ? value : "-" );
    }
    const char *phone = emergency.fields[EmergencyCard::ContactPhone];
    const char *note = emergency.fields[EmergencyCard::Note];
    const bool hasPhone = phone[0] != '\0';
    text.setLine( CardPhoneRow, hasPhone ? phone : note );
    text.setColours( CardPhoneRow, hasPhone ? Styles::Amber : TableText::White,
                     hasPhone ? Styles::Amber : TableText::White );
    text.setLine( CardNoteRow, hasPhone ? note : "" );
    m_cardTable.refresh();
}

// ---- Metronome ----

void ToolParts::setMetronomeShown( bool shown )
{
    m_shown = shown;
    if ( !shown && m_running ) {
        startStop( false );
    }
}

int ToolParts::metronomeSecondsLeft() const
{
    if ( !m_running ) {
        return -1;
    }
    const int64_t leftUs = m_untilUs - esp_timer_get_time();
    return leftUs > 0 ? static_cast<int>( ( leftUs + 999999 ) / 1000000 ) : 0;
}

void ToolParts::startStop( bool run )
{
    Ringer ringer;
    m_running = run;
    if ( run ) {
        const int64_t nowUs = esp_timer_get_time();
        m_startUs = nowUs + s_startDelayUs;
        m_untilUs = nowUs + s_holdUs;
        ringer.startBeats( m_startUs, periodUs(), s_beatsInBar );
    } else {
        ringer.stopTool( false );
    }
    ESP_LOGI( TAG, "Metronome %s, %d bpm, bar %d", run ? "on" : "off", s_bpm, s_beatsInBar );
    showMetronome();
}

void ToolParts::setTempo( int bpm )
{
    bpm = bpm < s_minimumBpm ? s_minimumBpm : ( bpm > s_maximumBpm ? s_maximumBpm : bpm );
    if ( bpm == s_bpm ) {
        return;
    }
    if ( m_running ) {
        // The new beat goes on from the last one heard.
        double phase = 0;
        const int beat = beatNow( &phase );
        if ( beat >= 0 ) {
            m_startUs += static_cast<int64_t>( beat ) * periodUs();
        }
        s_bpm = bpm;
        Ringer ringer;
        ringer.startBeats( m_startUs, periodUs(), s_beatsInBar );
    } else {
        s_bpm = bpm;
    }
    showMetronome();
}

int ToolParts::beatNow( double *phase ) const
{
    if ( !m_running ) {
        return -1;
    }
    const int64_t elapsedUs = esp_timer_get_time() - m_startUs;
    if ( elapsedUs < 0 ) {
        return -1;
    }
    const int64_t period = periodUs();
    *phase = static_cast<double>( elapsedUs % period ) / static_cast<double>( period );
    return static_cast<int>( elapsedUs / period );
}

void ToolParts::showMetronome()
{
    if ( m_tempo == nullptr ) {
        return;
    }
    char text[32];
    snprintf( text, sizeof( text ), "%d", s_bpm );
    setText( m_tempo, text );
    setText( m_startLabel, Language::text( m_running ? Language::Stop : Language::Start ) );
    const int left = metronomeSecondsLeft();
    m_shownSeconds = left;
    if ( left < 0 ) {
        setText( m_status, Language::text( Language::MetronomeSound ) );
    } else {
        snprintf( text, sizeof( text ), Language::text( Language::MetronomeStops ), left / 60, left % 60 );
        setText( m_status, text );
    }
    lv_obj_invalidate( m_dots );
}

void ToolParts::tick()
{
    if ( m_tempo == nullptr || !m_running ) {
        return;
    }
    double phase = 0;
    const int beat = beatNow( &phase );
    int lit = -1;
    if ( beat >= 0 ) {
        lit = s_beatsInBar >= 2 ? beat % s_beatsInBar : ( phase < s_flashPhase ? 0 : -1 );
    }
    if ( lit != m_litDot ) {
        m_litDot = lit;
        lv_obj_invalidate( m_dots );
    }
    if ( metronomeSecondsLeft() != m_shownSeconds ) {
        showMetronome();
    }
}

void ToolParts::onDrawDots( lv_event_t *event )
{
    ToolParts *self = static_cast<ToolParts *>( lv_event_get_user_data( event ) );
    lv_obj_t *object = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    lv_area_t area = {};
    lv_obj_get_coords( object, &area );
    const int count = s_beatsInBar >= 2 ? s_beatsInBar : 1;
    const int cx = ( area.x1 + area.x2 + 1 ) / 2;
    const int cy = ( area.y1 + area.y2 + 1 ) / 2;
    const int lit = self->m_running ? self->m_litDot : -1;
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init( &dot );
    dot.radius = LV_RADIUS_CIRCLE;
    for ( int i = 0; i < count; ++i ) {
        const int x = cx + ( 2 * i - ( count - 1 ) ) * s_dotStep / 2;
        dot.bg_color = i == lit ? lv_color_hex( Styles::Amber ) : s_dotOff;
        const lv_area_t box = { x - s_dotRadius, cy - s_dotRadius, x + s_dotRadius - 1, cy + s_dotRadius - 1 };
        lv_draw_rect( layer, &dot, &box );
    }
}

void ToolParts::onKeyEvent( lv_event_t *event )
{
    ToolParts *self = static_cast<ToolParts *>( lv_event_get_user_data( event ) );
    lv_obj_t *target = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    const bool repeat = lv_event_get_code( event ) == LV_EVENT_LONG_PRESSED_REPEAT;
    // Any tap on the part holds active mode for another 10 min.
    self->m_untilUs = esp_timer_get_time() + s_holdUs;
    if ( target == self->m_minus ) {
        self->setTempo( repeat ? ( s_bpm - 1 ) / s_fastStep * s_fastStep : s_bpm - 1 );
    } else if ( target == self->m_plus ) {
        self->setTempo( repeat ? s_bpm / s_fastStep * s_fastStep + s_fastStep : s_bpm + 1 );
    } else if ( target == self->m_cpr ) {
        self->setTempo( s_cprBpm );
    } else if ( target == self->m_dots ) {
        s_beatsInBar = s_beatsInBar == 0 ? 2 : ( s_beatsInBar >= 4 ? 0 : s_beatsInBar + 1 );
        if ( self->m_running ) {
            Ringer ringer;
            ringer.startBeats( self->m_startUs, periodUs(), s_beatsInBar );
        }
        self->m_litDot = -1;
        self->showMetronome();
    } else if ( target == self->m_startStop ) {
        self->startStop( !self->m_running );
    }
}

bool ToolParts::press( const char *name )
{
    if ( m_tempo == nullptr ) {
        return false;
    }
    lv_obj_t *target = nullptr;
    if ( strcmp( name, "beat" ) == 0 ) {
        target = m_startStop;
    } else if ( strcmp( name, "bar" ) == 0 ) {
        target = m_dots;
    } else if ( strcmp( name, "cpr" ) == 0 ) {
        target = m_cpr;
    } else if ( strncmp( name, "tempo ", 6 ) == 0 ) {
        m_untilUs = esp_timer_get_time() + s_holdUs;
        setTempo( atoi( name + 6 ) );
        return true;
    } else {
        return false;
    }
    lv_obj_send_event( target, LV_EVENT_SHORT_CLICKED, nullptr );
    return true;
}
