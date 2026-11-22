#include "CyclePages.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "CycleStore.h"
#include "Dates.h"
#include "Fonts.h"
#include "Astronomy.h"
#include "Hayiz.h"
#include "Language.h"
#include "Panel.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"

static const char *TAG = "cycle";

// A finger that moved this far was a swipe, not a tap.
static const int s_tapSlopPx = 30;
// Three wrong PINs in a row: a wait before the next try.
static const int s_wrongPins = 3;
static const int64_t s_pinWaitUs = 30000000;
static const int64_t s_normalShowUs = 4000000;
static const int64_t s_confirmUs = 3000000;
// The keypad under the PIN's title and dots.
static const int s_keypadY = 150;

void CyclePages::build( lv_obj_t *tile, CycleLog &log, void ( *onChange )( void *user ), void *user )
{
    m_tile = tile;
    m_log = &log;
    m_onChange = onChange;
    m_changeUser = user;
    m_stack.build( tile, PartCount, Language::text( Language::CyclePageName ) );
    for ( int i = 0; i < PartCount; ++i ) {
        lv_obj_t *part = m_stack.part( i );
        lv_obj_set_style_pad_all( part, 0, 0 );
        lv_obj_t *object = lv_obj_create( part );
        lv_obj_remove_style_all( object );
        lv_obj_set_size( object, Panel::WIDTH, Panel::HEIGHT );
        lv_obj_set_pos( object, 0, 0 );
        lv_obj_set_scrollable( object, false );
        lv_obj_set_user_data( object, reinterpret_cast<void *>( static_cast<intptr_t>( i ) ) );
        lv_obj_add_event_cb( object, onDraw, LV_EVENT_DRAW_MAIN, this );
        lv_obj_add_event_cb( object, onTouch, LV_EVENT_PRESSED, this );
        lv_obj_add_event_cb( object, onTouch, LV_EVENT_CLICKED, this );
        m_objects[i] = object;
    }
    lv_obj_add_event_cb( tile, onGesture, LV_EVENT_GESTURE, this );
    recompute();
    // Locked: the PIN first; never asked: offer one.
    if ( m_log->pinHash != 0 && !m_unlocked ) {
        openInput( CheckPin );
    } else if ( m_log->pinHash == 0 && m_log->pinAsked == 0 ) {
        openInput( NewPin );
    }
}

void CyclePages::forget()
{
    if ( m_tile != nullptr ) {
        lv_obj_remove_event_cb_with_user_data( m_tile, onGesture, this );
    }
    m_keypad.forget();
    m_stack.forget();
    m_tile = nullptr;
    m_overlay = nullptr;
    m_pinHeader = nullptr;
    m_open = NoOverlay;
    for ( int i = 0; i < PartCount; ++i ) {
        m_objects[i] = nullptr;
    }
}

StackPage &CyclePages::stack()
{
    return m_stack;
}

bool CyclePages::interactive() const
{
    return m_timeValid && m_log != nullptr && m_today >= m_log->newestDay;
}

void CyclePages::update( int today, bool timeValid, bool night )
{
    const bool changed = today != m_today || timeValid != m_timeValid || night != m_night;
    m_timeValid = timeValid;
    m_night = night;
    if ( today != m_today ) {
        m_today = today;
        if ( interactive() ) {
            m_log->advance( today );
        }
        recompute();
    }
    const int64_t now = esp_timer_get_time();
    if ( m_open == NormalOverlay && now >= m_normalUntilUs ) {
        closeOverlay();
    }
    if ( m_deleteArmedUs != 0 && now - m_deleteArmedUs >= s_confirmUs ) {
        m_deleteArmedUs = 0;
        refresh();
    }
    if ( m_open == PinOverlay && m_waitUntilUs != 0 && m_pinHeader != nullptr ) {
        if ( now >= m_waitUntilUs ) {
            m_waitUntilUs = 0;
            m_wrongPins = 0;
        }
        lv_obj_invalidate( m_pinHeader );
    }
    if ( changed ) {
        refresh();
    }
}

void CyclePages::reload()
{
    recompute();
    refresh();
}

void CyclePages::lock()
{
    m_unlocked = false;
    m_wrongPins = 0;
    m_waitUntilUs = 0;
}

bool CyclePages::unlocked() const
{
    return m_log != nullptr && ( m_log->pinHash == 0 || m_unlocked );
}

void CyclePages::setHayizShown( bool shown )
{
    m_hayizShown = shown;
}

bool CyclePages::swipesSideways() const
{
    return m_tile != nullptr && m_open == NoOverlay && m_stack.current() == CalendarPart;
}

void CyclePages::recompute()
{
    if ( m_log == nullptr ) {
        return;
    }
    m_state = Cycle::analyse( *m_log, m_today );
    // The spiral's angle of today (its Moon phase): computed once a day, not at every band.
    const int64_t noon = static_cast<int64_t>( m_today ) * 86400 + 43200;
    m_moonAge = Astronomy::moonPhase( noon ).ageDays;
}

void CyclePages::refresh()
{
    for ( int i = 0; i < PartCount; ++i ) {
        if ( m_objects[i] != nullptr ) {
            lv_obj_invalidate( m_objects[i] );
        }
    }
    if ( m_overlay != nullptr ) {
        lv_obj_invalidate( m_overlay );
    }
    if ( m_pinHeader != nullptr ) {
        lv_obj_invalidate( m_pinHeader );
    }
}

void CyclePages::save()
{
    CycleStore::save( *m_log );
    if ( m_onChange != nullptr ) {
        m_onChange( m_changeUser );
    }
    recompute();
    refresh();
}

void CyclePages::addRamadan( int year, int firstDay, int length )
{
    if ( m_log == nullptr || year <= m_log->lastRamadanYear ) {
        return;
    }
    const int days = Hayiz::hayzDays( *m_log, firstDay, length );
    m_log->ramadanAdded += days;
    m_log->lastRamadanYear = year;
    ESP_LOGI( TAG, "Ramadan %d ended: %d hayız days added", year, days );
    save();
}

uint32_t CyclePages::hash( const char *digits ) const
{
    // FNV-1a over the salt and the digits: against someone holding the watch, not a lab.
    uint32_t h = 2166136261u;
    for ( int i = 0; i < 4; ++i ) {
        h = ( h ^ ( ( m_log->pinSalt >> ( 8 * i ) ) & 0xFF ) ) * 16777619u;
    }
    for ( int i = 0; digits[i] != '\0'; ++i ) {
        h = ( h ^ static_cast<uint8_t>( digits[i] ) ) * 16777619u;
    }
    return h == 0 ? 1 : h;
}

// ---- Events ----

void CyclePages::onDraw( lv_event_t *event )
{
    CyclePages *self = static_cast<CyclePages *>( lv_event_get_user_data( event ) );
    lv_obj_t *object = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    const int part = static_cast<int>( reinterpret_cast<intptr_t>( lv_obj_get_user_data( object ) ) );
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t area = {};
    lv_obj_get_coords( object, &area );
    if ( !self->m_timeValid ) {
        lv_draw_label_dsc_t text;
        lv_draw_label_dsc_init( &text );
        text.font = &jetbrains_mono_26;
        text.color = lv_color_hex( 0xA0A0A0 );
        text.align = LV_TEXT_ALIGN_CENTER;
        text.text = Language::text( Language::CycleNoTime );
        const lv_area_t box = { area.x1 + 30, area.y1 + 220, area.x2 - 30, area.y1 + 300 };
        lv_draw_label( layer, &text, &box );
        return;
    }
    switch ( part ) {
    case SpiralPart:
        self->drawSpiral( layer, area );
        break;
    case CalendarPart:
        self->drawCalendar( layer, area );
        break;
    case HistoryPart:
        self->drawHistory( layer, area );
        break;
    case MoonPart:
        self->drawMoon( layer, area );
        break;
    case PregnancyPart:
        self->drawPregnancy( layer, area );
        break;
    case HayizPart:
        self->drawHayiz( layer, area );
        break;
    default:
        self->drawSettings( layer, area );
        break;
    }
}

// A tap: lifted near where it touched, and no swipe.
static bool tapped( lv_event_t *event, lv_point_t &press, lv_point_t &point )
{
    lv_indev_t *indev = lv_indev_active();
    if ( indev == nullptr ) {
        return false;
    }
    lv_indev_get_point( indev, &point );
    if ( lv_event_get_code( event ) == LV_EVENT_PRESSED ) {
        press = point;
        return false;
    }
    if ( lv_indev_get_gesture_dir( indev ) != LV_DIR_NONE ) {
        return false;
    }
    return abs( point.x - press.x ) + abs( point.y - press.y ) <= s_tapSlopPx;
}

void CyclePages::onTouch( lv_event_t *event )
{
    CyclePages *self = static_cast<CyclePages *>( lv_event_get_user_data( event ) );
    lv_point_t point = {};
    if ( !tapped( event, self->m_pressPoint, point ) || self->m_open != NoOverlay ) {
        return;
    }
    lv_obj_t *object = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    self->tapPart( static_cast<int>( reinterpret_cast<intptr_t>( lv_obj_get_user_data( object ) ) ), self->m_pressPoint );
}

void CyclePages::onGesture( lv_event_t *event )
{
    CyclePages *self = static_cast<CyclePages *>( lv_event_get_user_data( event ) );
    if ( !self->swipesSideways() ) {
        return;
    }
    const lv_dir_t direction = lv_indev_get_gesture_dir( lv_indev_active() );
    if ( direction == LV_DIR_LEFT || direction == LV_DIR_RIGHT ) {
        // A swipe to the left brings the next month, as on the clock page.
        self->m_monthStep += direction == LV_DIR_LEFT ? 1 : -1;
        self->refresh();
    }
}

void CyclePages::onOverlayDraw( lv_event_t *event )
{
    CyclePages *self = static_cast<CyclePages *>( lv_event_get_user_data( event ) );
    lv_obj_t *object = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    lv_area_t area = {};
    lv_obj_get_coords( object, &area );
    lv_layer_t *layer = lv_event_get_layer( event );
    if ( object == self->m_pinHeader ) {
        self->drawPinHeader( layer, area );
    } else if ( self->m_open == EntryOverlay ) {
        self->drawEntry( layer, area );
    } else if ( self->m_open == LegendOverlay ) {
        self->drawLegend( layer, area );
    } else if ( self->m_open == NormalOverlay ) {
        self->drawNormal( layer, area );
    }
}

void CyclePages::onOverlayTouch( lv_event_t *event )
{
    CyclePages *self = static_cast<CyclePages *>( lv_event_get_user_data( event ) );
    lv_point_t point = {};
    if ( tapped( event, self->m_pressPoint, point ) ) {
        self->tapOverlay( self->m_pressPoint );
    }
}

// ---- Taps on the parts ----

void CyclePages::tapPart( int part, const lv_point_t &point )
{
    if ( !interactive() || !unlocked() ) {
        return;
    }
    if ( part == SpiralPart ) {
        int x = 0;
        int y = 0;
        float angle = 0;
        spiralPoint( 0, 0, x, y, angle );
        if ( abs( point.x - x ) <= 22 && abs( point.y - y ) <= 22 ) {
            openEntry( m_today );
        } else if ( abs( point.x - Panel::WIDTH / 2 ) <= 50 && abs( point.y - 236 ) <= 50 ) {
            openOverlay( LegendOverlay );
        } else if ( point.y >= 440 ) {
            m_normalUntilUs = esp_timer_get_time() + s_normalShowUs;
            openOverlay( NormalOverlay );
        }
    } else if ( part == CalendarPart ) {
        const int day = calendarDayAt( point );
        if ( day != 0 && day <= m_today && day > m_today - CycleLog::Days ) {
            openEntry( day );
        }
    } else if ( part == SettingsPart ) {
        // The rows from y 80, 64 px apart: PIN, change PIN, adolescent, first period, earlier
        // fasts, delete.
        const int row = ( point.y - 80 ) / 64;
        if ( point.y < 80 || row > 5 ) {
            return;
        }
        if ( row == 0 ) {
            openInput( m_log->pinHash == 0 ? NewPin : OldPinOff );
        } else if ( row == 1 && m_log->pinHash != 0 ) {
            openInput( OldPinChange );
        } else if ( row == 2 ) {
            m_log->teenMode = static_cast<uint8_t>( ( m_log->teenMode + 1 ) % 3 );
            save();
        } else if ( row == 3 && m_log->teenMode == CycleLog::TeenAuto ) {
            openInput( FirstPeriod );
        } else if ( row == 4 && m_hayizShown ) {
            openInput( EarlierFasts );
        } else if ( row == 5 ) {
            const int64_t now = esp_timer_get_time();
            if ( m_deleteArmedUs != 0 && now - m_deleteArmedUs < s_confirmUs ) {
                m_deleteArmedUs = 0;
                CycleStore::erase();
                m_log->clear();
                m_log->advance( m_today );
                m_unlocked = false;
                ESP_LOGW( TAG, "Cycle data deleted on the watch" );
                save();
            } else {
                m_deleteArmedUs = now;
                refresh();
            }
        }
    }
}

// ---- Overlays ----

void CyclePages::openOverlay( Overlay overlay )
{
    closeOverlay();
    m_open = overlay;
    m_overlay = lv_obj_create( m_tile );
    lv_obj_remove_style_all( m_overlay );
    lv_obj_set_size( m_overlay, Panel::WIDTH, Panel::HEIGHT );
    lv_obj_set_pos( m_overlay, 0, 0 );
    lv_obj_set_scrollable( m_overlay, false );
    lv_obj_set_style_bg_color( m_overlay, lv_color_black(), 0 );
    lv_obj_set_style_bg_opa( m_overlay, LV_OPA_COVER, 0 );
    lv_obj_add_event_cb( m_overlay, onOverlayDraw, LV_EVENT_DRAW_MAIN, this );
    lv_obj_add_event_cb( m_overlay, onOverlayTouch, LV_EVENT_PRESSED, this );
    lv_obj_add_event_cb( m_overlay, onOverlayTouch, LV_EVENT_CLICKED, this );
    // The parts stay put under an overlay.
    m_stack.setSwipeLocked( true );
}

void CyclePages::closeOverlay()
{
    if ( m_overlay != nullptr ) {
        m_keypad.forget();
        lv_obj_delete( m_overlay );
    }
    m_overlay = nullptr;
    m_pinHeader = nullptr;
    m_open = NoOverlay;
    m_stack.setSwipeLocked( false );
    refresh();
}

void CyclePages::tapOverlay( const lv_point_t &point )
{
    if ( m_open == LegendOverlay || m_open == NormalOverlay ) {
        closeOverlay();
        return;
    }
    if ( m_open == PinOverlay ) {
        // Only the "No PIN" key of the offer, under the title; the keys are the keypad's.
        if ( m_input == NewPin && m_log->pinHash == 0 && m_log->pinAsked == 0 && point.y >= 100 && point.y < 145 ) {
            m_log->pinAsked = 1;
            m_unlocked = true;
            closeOverlay();
            save();
        }
        return;
    }
    if ( m_open != EntryOverlay ) {
        return;
    }
    // The four flow keys, the intercourse and make-up fast keys, OK (CycleParts.cpp's layout).
    if ( point.y >= 134 && point.y <= 222 ) {
        const int key = ( point.x - 44 ) / 82;
        if ( key >= 0 && key < 4 && point.x >= 44 ) {
            m_entryValue = static_cast<uint8_t>( ( m_entryValue & ~CycleLog::FlowMask ) | key );
        }
    } else if ( point.y >= 276 && point.y <= 350 ) {
        const bool teen = m_state.teen;
        if ( point.x < Panel::WIDTH / 2 && !teen ) {
            m_entryValue ^= CycleLog::Intercourse;
        } else if ( point.x >= Panel::WIDTH / 2 && m_hayizShown ) {
            m_entryValue ^= CycleLog::MadeUpFast;
        }
    } else if ( point.y >= 390 && point.y <= 450 ) {
        m_log->set( m_entryDay, m_entryValue );
        ESP_LOGI( TAG, "Entry for day %d: 0x%02x", m_entryDay, m_entryValue );
        closeOverlay();
        save();
        return;
    } else if ( point.y < 46 ) {
        closeOverlay();
        return;
    }
    lv_obj_invalidate( m_overlay );
}

void CyclePages::openEntry( int day )
{
    m_entryDay = day;
    m_entryValue = m_log->get( day );
    openOverlay( EntryOverlay );
}

void CyclePages::openInput( Input input )
{
    openOverlay( PinOverlay );
    m_input = input;
    m_typed[0] = '\0';
    m_pinHeader = lv_obj_create( m_overlay );
    lv_obj_remove_style_all( m_pinHeader );
    lv_obj_set_size( m_pinHeader, Panel::WIDTH, s_keypadY - 4 );
    lv_obj_set_pos( m_pinHeader, 0, 0 );
    lv_obj_set_clickable( m_pinHeader, false );
    lv_obj_add_event_cb( m_pinHeader, onOverlayDraw, LV_EVENT_DRAW_MAIN, this );
    m_calculator = Calculator();
    m_keypad.build( m_overlay, ( Panel::WIDTH - Keypad::Width ) / 2, s_keypadY, m_calculator, onKey, this );
}

void CyclePages::onKey( void *user, Calculator::Key key )
{
    CyclePages *self = static_cast<CyclePages *>( user );
    if ( self->m_waitUntilUs != 0 ) {
        return;
    }
    const size_t length = strlen( self->m_typed );
    const bool pin = self->m_input <= OldPinChange;
    const size_t limit = pin ? 4 : 7;
    if ( key >= Calculator::Key0 && key <= Calculator::Key9 && length < limit ) {
        self->m_typed[length] = static_cast<char>( '0' + ( key - Calculator::Key0 ) );
        self->m_typed[length + 1] = '\0';
    } else if ( key == Calculator::Point && !pin && length < limit ) {
        self->m_typed[length] = '.';
        self->m_typed[length + 1] = '\0';
    } else if ( key == Calculator::Back && length > 0 ) {
        self->m_typed[length - 1] = '\0';
    } else if ( key == Calculator::Clear ) {
        if ( length == 0 && self->m_input != CheckPin && self->m_input != NewPin ) {
            self->closeOverlay();
            return;
        }
        self->m_typed[0] = '\0';
    } else if ( key == Calculator::Equals ) {
        self->submitInput();
        return;
    }
    if ( self->m_pinHeader != nullptr ) {
        lv_obj_invalidate( self->m_pinHeader );
    }
}

void CyclePages::submitInput()
{
    const bool pin = m_input <= OldPinChange;
    if ( pin && strlen( m_typed ) != 4 ) {
        return;
    }
    switch ( m_input ) {
    case CheckPin:
    case OldPinOff:
    case OldPinChange:
        if ( hash( m_typed ) != m_log->pinHash ) {
            ESP_LOGW( TAG, "Wrong PIN" );
            if ( ++m_wrongPins >= s_wrongPins ) {
                m_waitUntilUs = esp_timer_get_time() + s_pinWaitUs;
            }
            m_typed[0] = '\0';
            lv_obj_invalidate( m_pinHeader );
            return;
        }
        m_wrongPins = 0;
        m_unlocked = true;
        if ( m_input == OldPinOff ) {
            m_log->pinHash = 0;
            closeOverlay();
            save();
        } else if ( m_input == OldPinChange ) {
            openInput( NewPin );
        } else {
            closeOverlay();
        }
        return;
    case NewPin:
        if ( m_log->pinSalt == 0 ) {
            m_log->pinSalt = esp_random() | 1;
        }
        m_firstPin = hash( m_typed );
        m_input = RepeatPin;
        m_typed[0] = '\0';
        lv_obj_invalidate( m_pinHeader );
        return;
    case RepeatPin:
        if ( hash( m_typed ) != m_firstPin ) {
            m_input = NewPin;
            m_typed[0] = '\0';
            lv_obj_invalidate( m_pinHeader );
            return;
        }
        m_log->pinHash = m_firstPin;
        m_log->pinAsked = 1;
        m_unlocked = true;
        ESP_LOGI( TAG, "PIN set" );
        closeOverlay();
        save();
        return;
    case FirstPeriod: {
        // year.month, e.g. 2025.9; the first of that month.
        int year = 0;
        int month = 0;
        if ( sscanf( m_typed, "%d.%d", &year, &month ) == 2 && year >= 2000 && year <= 2100 && month >= 1 &&
             month <= 12 ) {
            const int day = Dates::dayNumber( year, month, 1 );
            if ( day <= m_today ) {
                m_log->firstPeriodDay = day;
                closeOverlay();
                save();
            }
        }
        return;
    }
    case EarlierFasts:
        m_log->earlierBalance = atoi( m_typed );
        closeOverlay();
        save();
        return;
    }
}

// ---- Development ----

bool CyclePages::press( const char *name )
{
    if ( m_tile == nullptr ) {
        return false;
    }
    int flow = 0;
    int together = 0;
    int madeUp = 0;
    if ( sscanf( name, "entry %d %d %d", &flow, &together, &madeUp ) >= 2 && interactive() ) {
        m_log->set( m_today, static_cast<uint8_t>( ( flow & CycleLog::FlowMask ) | ( together ? CycleLog::Intercourse : 0 ) |
                                                   ( madeUp ? CycleLog::MadeUpFast : 0 ) ) );
        save();
        return true;
    }
    if ( strncmp( name, "pin ", 4 ) == 0 && m_open == PinOverlay ) {
        snprintf( m_typed, sizeof( m_typed ), "%s", name + 4 );
        submitInput();
        return true;
    }
    if ( strcmp( name, "teen" ) == 0 ) {
        m_log->teenMode = static_cast<uint8_t>( ( m_log->teenMode + 1 ) % 3 );
        save();
        return true;
    }
    if ( strcmp( name, "legend" ) == 0 ) {
        openOverlay( m_open == LegendOverlay ? NoOverlay : LegendOverlay );
        if ( m_open == NoOverlay ) {
            closeOverlay();
        }
        return true;
    }
    int step = 0;
    if ( sscanf( name, "month %d", &step ) == 1 ) {
        m_monthStep = step;
        refresh();
        return true;
    }
    if ( strcmp( name, "open-entry" ) == 0 && interactive() ) {
        openEntry( m_today );
        return true;
    }
    if ( strcmp( name, "open-pin" ) == 0 ) {
        openInput( m_log->pinHash != 0 ? CheckPin : NewPin );
        return true;
    }
    if ( strcmp( name, "close" ) == 0 ) {
        closeOverlay();
        return true;
    }
    return false;
}
