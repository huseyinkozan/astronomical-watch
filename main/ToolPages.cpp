#include "ToolPages.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Fonts.h"
#include "Language.h"
#include "Morse.h"
#include "Panel.h"
#include "Ringer.h"
#include "Styles.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"

static const char *TAG = "tools";

static const lv_color_t s_grey = LV_COLOR_MAKE( 0xA0, 0xA0, 0xA0 );
static const lv_color_t s_dim = LV_COLOR_MAKE( 0x5A, 0x5A, 0x5A );
// Content stays inside the corner gauges: rows from x 52 to 358.
static const int s_left = 52;
static const int s_right = 358;
static const int s_tickMs = 50;

// The torch's colours, then SOS (white flashes) without and with beeps.
enum TorchColour
{
    TorchRed,
    TorchAmber,
    TorchWhite,
    TorchSos,
    TorchSosBeep,
    TorchColourCount
};
static const uint32_t s_torchColours[TorchColourCount] = { 0xFF0000, 0xFFA000, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF };
static const Language::Text s_torchNames[TorchColourCount] = { Language::TorchRed, Language::TorchAmber,
                                                               Language::TorchWhite, Language::TorchSos,
                                                               Language::TorchSosBeep };
static const int64_t s_torchOnUs = 120 * 1000000LL;
static const int s_minimumLevel = 5;
// After the long press the finger must move this far before the drag's direction counts (as on
// the face); then a colour step every s_colourStepPx sideways, the full brightness range over
// s_levelRangePx up or down.
static const int s_axisPx = 20;
static const int s_colourStepPx = 60;
static const int s_levelRangePx = 222;

// Kept over the always-on wakes (lost at power-off): red at 30 %, dim enough for dark-adapted eyes.
RTC_DATA_ATTR static int s_torchColour = TorchRed;
RTC_DATA_ATTR static int s_torchLevel = 30;

// The random part: dice, a coin or a number from 1 to 100, and the last roll of each.
enum RandomMode
{
    DiceMode,
    CoinMode,
    NumberMode,
    RandomModeCount
};
RTC_DATA_ATTR static int s_randomMode = DiceMode;
RTC_DATA_ATTR static int s_diceCount = 1;
RTC_DATA_ATTR static int s_dice[3] = { 5, 3, 6 };
RTC_DATA_ATTR static int s_coin = 0;  // 0 heads, 1 tails
RTC_DATA_ATTR static int s_number = 42;

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

// A button like the clock page's; returns the button (its label is its first child).
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
    label( box, &jetbrains_mono_26, lv_color_white(), LV_ALIGN_CENTER, 0, 0, text );
    return box;
}

// Sets the text only when it changed, so that LVGL redraws nothing else.
static void setText( lv_obj_t *object, const char *text )
{
    if ( object != nullptr && strcmp( lv_label_get_text( object ), text ) != 0 ) {
        lv_label_set_text( object, text );
    }
}

void ToolPages::build( lv_obj_t *tile, lv_obj_t *pages, Settings &settings, void ( *onLevel )( void *user ), void *user )
{
    m_tile = tile;
    m_pages = pages;
    m_settings = &settings;
    m_onLevel = onLevel;
    m_levelUser = user;
    m_stack.build( tile, PartCount, Language::text( Language::ToolsPageName ) );
    buildTorch( m_stack.part( TorchPart ) );
    buildCounter( m_stack.part( CounterPart ) );
    buildRandom( m_stack.part( RandomPart ) );
    lv_obj_add_event_cb( tile, onPartChanged, LV_EVENT_VALUE_CHANGED, this );
    m_tick = lv_timer_create( onTick, s_tickMs, this );
}

void ToolPages::forget()
{
    setTorchShown( false );
    if ( m_tile != nullptr ) {
        lv_obj_remove_event_cb_with_user_data( m_tile, onPartChanged, this );
    }
    m_stack.forget();
    if ( m_tick != nullptr ) {
        lv_timer_delete( m_tick );
        m_tick = nullptr;
    }
    m_tile = nullptr;
    m_torch = nullptr;
    m_torchHint = nullptr;
    m_torchStatus = nullptr;
    m_count = nullptr;
    m_goal = nullptr;
    m_rate = nullptr;
    m_plusOne = nullptr;
    m_undo = nullptr;
    m_target = nullptr;
    m_targetLabel = nullptr;
    m_bootHint = nullptr;
    m_randomArea = nullptr;
    m_number = nullptr;
    m_coin = nullptr;
    for ( int i = 0; i < RandomModeCount; ++i ) {
        m_modes[i] = nullptr;
    }
    m_diceHint = nullptr;
}

StackPage &ToolPages::stack()
{
    return m_stack;
}

// The torch fills the screen: the page name hides while its part is the current one.
void ToolPages::onPartChanged( lv_event_t *event )
{
    ToolPages *self = static_cast<ToolPages *>( lv_event_get_user_data( event ) );
    self->m_stack.setHeaderHidden( self->m_stack.current() == TorchPart );
}

void ToolPages::onTick( lv_timer_t *timer )
{
    ToolPages *self = static_cast<ToolPages *>( lv_timer_get_user_data( timer ) );
    if ( !self->m_torchShown ) {
        return;
    }
    const int64_t nowUs = esp_timer_get_time();
    bool changed = self->torchSecondsLeft() != self->m_shownSeconds;
    if ( s_torchColour >= TorchSos ) {
        const bool lit = Morse::sosLit( ( nowUs - self->m_sosStartUs ) / 1000 );
        changed = changed || lit != self->m_sosLit;
        self->m_sosLit = lit;
    }
    if ( changed ) {
        self->showTorch();
    }
}

// ---- Torch ----

void ToolPages::buildTorch( lv_obj_t *part )
{
    // The whole screen, over the part's padding for the page name.
    lv_obj_set_style_pad_top( part, 0, 0 );
    m_torch = lv_obj_create( part );
    lv_obj_remove_style_all( m_torch );
    lv_obj_set_size( m_torch, Panel::WIDTH, Panel::HEIGHT );
    lv_obj_set_scrollable( m_torch, false );
    lv_obj_set_style_bg_opa( m_torch, LV_OPA_COVER, 0 );
    const lv_event_code_t codes[6] = { LV_EVENT_PRESSED, LV_EVENT_LONG_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED,
                                       LV_EVENT_PRESS_LOST, LV_EVENT_CLICKED };
    for ( int i = 0; i < 6; ++i ) {
        lv_obj_add_event_cb( m_torch, onTorchEvent, codes[i], this );
    }
    // Black text on the colour (an own style: it changes with the SOS flashes).
    m_torchHint = lv_label_create( m_torch );
    lv_obj_set_style_text_font( m_torchHint, &jetbrains_mono_26, 0 );
    lv_obj_set_style_text_align( m_torchHint, LV_TEXT_ALIGN_CENTER, 0 );
    lv_label_set_text( m_torchHint, Language::text( Language::TorchHint ) );
    lv_obj_align( m_torchHint, LV_ALIGN_TOP_MID, 0, 150 );
    m_torchStatus = lv_label_create( m_torch );
    lv_obj_set_style_text_font( m_torchStatus, &jetbrains_mono_26, 0 );
    lv_label_set_text( m_torchStatus, "" );
    lv_obj_align( m_torchStatus, LV_ALIGN_TOP_MID, 0, 410 );
    showTorch();
}

void ToolPages::showTorch()
{
    if ( m_torch == nullptr ) {
        return;
    }
    const bool sos = s_torchColour >= TorchSos;
    const bool lit = !sos || m_sosLit;
    lv_obj_set_style_bg_color( m_torch, lit ? lv_color_hex( s_torchColours[s_torchColour] ) : lv_color_black(), 0 );
    const lv_color_t text = lit ? lv_color_black() : lv_color_hex( 0x808080 );
    lv_obj_set_style_text_color( m_torchHint, text, 0 );
    lv_obj_set_style_text_color( m_torchStatus, text, 0 );
    char percent[8];
    snprintf( percent, sizeof( percent ), Language::text( Language::PercentValue ), s_torchLevel );
    const int left = m_torchShown ? torchSecondsLeft() : s_torchOnUs / 1000000;
    m_shownSeconds = m_torchShown ? left : -1;
    char status[48];
    snprintf( status, sizeof( status ), "%s  %s  %d:%02d", Language::text( s_torchNames[s_torchColour] ), percent,
              left / 60, left % 60 );
    setText( m_torchStatus, status );
}

void ToolPages::setTorchShown( bool shown )
{
    if ( shown && !m_torchShown ) {
        m_sosStartUs = esp_timer_get_time();
        m_sosLit = true;
        m_torchEndUs = m_sosStartUs + s_torchOnUs;
    }
    if ( !shown && m_adjusting ) {
        endTorchAdjust();
    }
    m_torchShown = shown;
    applySound();
    showTorch();
}

void ToolPages::restartTorch()
{
    m_torchEndUs = esp_timer_get_time() + s_torchOnUs;
    showTorch();
}

int ToolPages::torchLevel() const
{
    return s_torchLevel;
}

int ToolPages::torchSecondsLeft() const
{
    if ( !m_torchShown ) {
        return -1;
    }
    const int64_t leftUs = m_torchEndUs - esp_timer_get_time();
    const int left = leftUs > 0 ? static_cast<int>( ( leftUs + 999999 ) / 1000000 ) : 0;
    // A drag held past the end still sets the torch.
    return m_adjusting && left < 1 ? 1 : left;
}

void ToolPages::applySound()
{
    const bool wanted = m_torchShown && s_torchColour == TorchSosBeep;
    if ( wanted == m_soundOn ) {
        return;
    }
    m_soundOn = wanted;
    Ringer ringer;
    if ( wanted ) {
        ringer.startSos( m_sosStartUs );
    } else {
        ringer.stopSos( false );
    }
    ESP_LOGI( TAG, "SOS beeps %s", wanted ? "on" : "off" );
}

void ToolPages::onTorchEvent( lv_event_t *event )
{
    ToolPages *self = static_cast<ToolPages *>( lv_event_get_user_data( event ) );
    const lv_event_code_t code = lv_event_get_code( event );
    lv_point_t point = {};
    lv_indev_t *indev = lv_indev_active();
    if ( indev != nullptr ) {
        lv_indev_get_point( indev, &point );
    }
    if ( code == LV_EVENT_PRESSED ) {
        self->m_pressX = point.x;
        self->m_pressY = point.y;
        self->m_pressMoved = false;
        return;
    }
    if ( code == LV_EVENT_LONG_PRESSED ) {
        if ( self->m_pressMoved ) {
            return;
        }
        self->m_adjusting = true;
        self->m_axisChosen = false;
        self->m_startColour = s_torchColour;
        self->m_startLevel = s_torchLevel;
        self->m_pressX = point.x;
        self->m_pressY = point.y;
        // The finger may drift: neither the pages nor the parts move while the torch is set.
        lv_obj_set_scrollable( self->m_pages, false );
        self->m_stack.setSwipeLocked( true );
        return;
    }
    if ( code == LV_EVENT_CLICKED ) {
        // Any tap, also the end of a drag: the auto-off starts again.
        self->restartTorch();
        return;
    }
    const int dx = point.x - self->m_pressX;
    const int dy = self->m_pressY - point.y;
    if ( code == LV_EVENT_PRESSING ) {
        if ( !self->m_adjusting ) {
            self->m_pressMoved = self->m_pressMoved || abs( dx ) >= s_axisPx || abs( dy ) >= s_axisPx;
            return;
        }
        if ( !self->m_axisChosen ) {
            if ( abs( dx ) < s_axisPx && abs( dy ) < s_axisPx ) {
                return;
            }
            self->m_axisChosen = true;
            self->m_sideways = abs( dx ) > abs( dy );
        }
        if ( self->m_sideways ) {
            int colour = self->m_startColour + dx / s_colourStepPx;
            colour = colour < 0 ? 0 : ( colour >= TorchColourCount ? TorchColourCount - 1 : colour );
            if ( colour != s_torchColour ) {
                s_torchColour = colour;
                // SOS from its start, so that it reads from the first letter.
                self->m_sosStartUs = esp_timer_get_time();
                self->m_sosLit = true;
                self->applySound();
                self->showTorch();
            }
        } else {
            int level = self->m_startLevel + dy * 100 / s_levelRangePx;
            level = level < s_minimumLevel ? s_minimumLevel : ( level > 100 ? 100 : level );
            if ( level != s_torchLevel ) {
                s_torchLevel = level;
                if ( self->m_onLevel != nullptr ) {
                    self->m_onLevel( self->m_levelUser );
                }
                self->showTorch();
            }
        }
        return;
    }
    // Released, or the press was lost.
    if ( self->m_adjusting ) {
        self->endTorchAdjust();
    }
}

void ToolPages::endTorchAdjust()
{
    m_adjusting = false;
    if ( m_pages != nullptr ) {
        lv_obj_set_scrollable( m_pages, true );
    }
    m_stack.setSwipeLocked( false );
    m_torchEndUs = esp_timer_get_time() + s_torchOnUs;
    ESP_LOGI( TAG, "Torch colour %d, level %d %%", s_torchColour, s_torchLevel );
}

// ---- Counter ----

void ToolPages::buildCounter( lv_obj_t *part )
{
    m_tally.count = m_settings->value( Settings::CounterValue );
    m_tally.target = m_settings->value( Settings::CounterTarget );
    m_tally.startMinute = m_settings->value( Settings::CounterStart );
    // A long press on the count (its area, larger than the digits) resets it.
    lv_obj_t *area = lv_obj_create( part );
    lv_obj_remove_style_all( area );
    lv_obj_set_pos( area, s_left, 30 );
    lv_obj_set_size( area, s_right - s_left, 170 );
    lv_obj_set_scrollable( area, false );
    lv_obj_add_event_cb( area, onCountPressed, LV_EVENT_LONG_PRESSED, this );
    m_count = lv_label_create( part );
    lv_obj_set_style_text_font( m_count, &jetbrains_mono_light_90, 0 );
    lv_obj_align( m_count, LV_ALIGN_TOP_MID, 0, 30 );
    lv_obj_set_clickable( m_count, false );
    m_goal = label( part, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_MID, 0, 138, "" );
    m_rate = label( part, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_MID, 0, 170, "" );

    m_plusOne = lv_obj_create( part );
    lv_obj_set_pos( m_plusOne, s_left, 210 );
    lv_obj_set_size( m_plusOne, s_right - s_left, 104 );
    lv_obj_set_scrollable( m_plusOne, false );
    lv_obj_set_press_lock( m_plusOne, false );
    Styles::accent( m_plusOne );
    lv_obj_add_event_cb( m_plusOne, onCounterButton, LV_EVENT_CLICKED, this );
    label( m_plusOne, &jetbrains_mono_48, lv_color_hex( Styles::Amber ), LV_ALIGN_CENTER, 0, 0, "+1" );

    m_undo = button( part, s_left, 324, 150, 388, "", onCounterButton, this );
    lv_obj_add_event_cb( m_undo, onDrawUndo, LV_EVENT_DRAW_POST, nullptr );
    m_target = button( part, 160, 324, s_right, 388, "", onCounterButton, this );
    m_targetLabel = lv_obj_get_child( m_target, 0 );
    label( part, &jetbrains_mono_26, s_dim, LV_ALIGN_TOP_MID, 0, 398, Language::text( Language::CounterHint ) );
    m_bootHint = label( part, &jetbrains_mono_26, s_dim, LV_ALIGN_TOP_MID, 0, 428, "BOOT: +1" );
    showCounter();
}

// The undo key's sign: an arc over the top with an arrowhead at its left end.
void ToolPages::onDrawUndo( lv_event_t *event )
{
    lv_obj_t *object = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    lv_area_t area = {};
    lv_obj_get_coords( object, &area );
    const int cx = ( area.x1 + area.x2 + 1 ) / 2;
    const int cy = ( area.y1 + area.y2 + 1 ) / 2 + 6;
    const int radius = 16;
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init( &arc );
    arc.color = lv_color_white();
    arc.width = 4;
    arc.radius = radius;
    arc.center.x = cx;
    arc.center.y = cy;
    arc.start_angle = 180;
    arc.end_angle = 360;
    arc.rounded = 1;
    lv_draw_arc( layer, &arc );
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init( &line );
    line.color = lv_color_white();
    line.width = 4;
    line.round_start = 1;
    line.round_end = 1;
    line.p1.x = cx - radius;
    line.p1.y = cy + 6;
    line.p2.x = cx - radius - 9;
    line.p2.y = cy - 4;
    lv_draw_line( layer, &line );
    line.p2.x = cx - radius + 9;
    lv_draw_line( layer, &line );
}

void ToolPages::showCounter()
{
    if ( m_count == nullptr ) {
        return;
    }
    char text[48];
    snprintf( text, sizeof( text ), "%d", m_tally.inStep() );
    setText( m_count, text );
    lv_obj_set_style_text_color( m_count, m_tally.reached() ? lv_color_hex( Styles::Amber ) : lv_color_white(), 0 );
    if ( m_tally.goal() == 0 ) {
        text[0] = '\0';
    } else if ( m_tally.stepCount() == 1 ) {
        snprintf( text, sizeof( text ), "/ %d", m_tally.goal() );
    } else {
        char step[24];
        snprintf( step, sizeof( step ), Language::text( Language::CounterStep ), m_tally.step() + 1, m_tally.stepCount() );
        snprintf( text, sizeof( text ), "/ %d   %s", m_tally.goal(), step );
    }
    setText( m_goal, text );
    const int perHour = m_tally.perHour( m_utcSeconds );
    if ( perHour < 0 ) {
        text[0] = '\0';
    } else {
        char rate[16];
        char elapsed[24];
        snprintf( rate, sizeof( rate ), Language::text( Language::CounterRate ), perHour );
        const int minutes = m_tally.elapsedMinutes( m_utcSeconds );
        if ( minutes < 60 ) {
            snprintf( elapsed, sizeof( elapsed ), Language::text( Language::Minutes ), minutes );
        } else {
            snprintf( elapsed, sizeof( elapsed ), Language::text( Language::DurationHourMins ), minutes / 60, minutes % 60 );
        }
        snprintf( text, sizeof( text ), "%s  %s", rate, elapsed );
    }
    setText( m_rate, text );
    if ( m_tally.target == Tally::NoTarget ) {
        setText( m_targetLabel, Language::text( Language::TargetNone ) );
    } else if ( m_tally.target == Tally::Steps33 ) {
        setText( m_targetLabel, Language::text( Language::TargetSteps ) );
    } else {
        snprintf( text, sizeof( text ), Language::text( Language::TargetValue ), m_tally.goal() );
        setText( m_targetLabel, text );
    }
    lv_obj_set_hidden( m_bootHint, m_settings->value( Settings::BootCounter ) == 0 );
}

// In memory only: the settings are written when active mode ends, not at each count.
void ToolPages::saveCounter()
{
    m_settings->setValue( Settings::CounterValue, m_tally.count );
    m_settings->setValue( Settings::CounterTarget, m_tally.target );
    m_settings->setValue( Settings::CounterStart, static_cast<int>( m_tally.startMinute ) );
    showCounter();
}

void ToolPages::onCounterButton( lv_event_t *event )
{
    ToolPages *self = static_cast<ToolPages *>( lv_event_get_user_data( event ) );
    lv_obj_t *target = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    if ( target == self->m_plusOne ) {
        self->m_tally.add( self->m_utcSeconds );
    } else if ( target == self->m_undo ) {
        self->m_tally.undo();
    } else if ( target == self->m_target ) {
        self->m_tally.nextTarget();
    }
    self->saveCounter();
}

void ToolPages::onCountPressed( lv_event_t *event )
{
    ToolPages *self = static_cast<ToolPages *>( lv_event_get_user_data( event ) );
    self->m_tally.reset();
    self->saveCounter();
    ESP_LOGI( TAG, "Counter reset" );
}

void ToolPages::countUp()
{
    m_tally.add( m_utcSeconds );
    saveCounter();
}

void ToolPages::update( int64_t utcSeconds )
{
    m_utcSeconds = utcSeconds;
    if ( m_count != nullptr && m_stack.current() == CounterPart ) {
        showCounter();
    }
}

// ---- Random ----

void ToolPages::buildRandom( lv_obj_t *part )
{
    // The drawing (dice, the coin's ring) and the tap area that rolls.
    m_randomArea = lv_obj_create( part );
    lv_obj_remove_style_all( m_randomArea );
    lv_obj_set_pos( m_randomArea, 0, 40 );
    lv_obj_set_size( m_randomArea, Panel::WIDTH, 260 );
    lv_obj_set_scrollable( m_randomArea, false );
    lv_obj_add_event_cb( m_randomArea, onDrawRandom, LV_EVENT_DRAW_MAIN, this );
    lv_obj_add_event_cb( m_randomArea, onRandomEvent, LV_EVENT_CLICKED, this );
    m_number = label( part, &jetbrains_mono_light_90, lv_color_white(), LV_ALIGN_TOP_MID, 0, 116, "" );
    m_coin = label( part, &jetbrains_mono_48, lv_color_white(), LV_ALIGN_TOP_MID, 0, 140, "" );
    const char *names[RandomModeCount] = { Language::text( Language::RandomDice ),
                                           Language::text( Language::RandomCoin ), "1-100" };
    for ( int i = 0; i < RandomModeCount; ++i ) {
        const int x = 30 + i * 123;
        m_modes[i] = button( part, x, 316, x + 104, 380, names[i], onRandomEvent, this );
    }
    label( part, &jetbrains_mono_26, s_dim, LV_ALIGN_TOP_MID, 0, 396, Language::text( Language::RandomHint ) );
    m_diceHint = label( part, &jetbrains_mono_26, s_dim, LV_ALIGN_TOP_MID, 0, 426, Language::text( Language::DiceHint ) );
    showRandom();
}

void ToolPages::roll()
{
    if ( s_randomMode == DiceMode ) {
        for ( int i = 0; i < 3; ++i ) {
            s_dice[i] = static_cast<int>( esp_random() % 6 ) + 1;
        }
    } else if ( s_randomMode == CoinMode ) {
        s_coin = static_cast<int>( esp_random() % 2 );
    } else {
        s_number = static_cast<int>( esp_random() % 100 ) + 1;
    }
    showRandom();
}

void ToolPages::showRandom()
{
    if ( m_randomArea == nullptr ) {
        return;
    }
    for ( int i = 0; i < RandomModeCount; ++i ) {
        if ( i == s_randomMode ) {
            lv_obj_add_state( m_modes[i], LV_STATE_CHECKED );
        } else {
            lv_obj_remove_state( m_modes[i], LV_STATE_CHECKED );
        }
    }
    char text[8];
    snprintf( text, sizeof( text ), "%d", s_number );
    setText( m_number, text );
    setText( m_coin, Language::text( s_coin == 0 ? Language::CoinHeads : Language::CoinTails ) );
    lv_obj_set_hidden( m_number, s_randomMode != NumberMode );
    lv_obj_set_hidden( m_coin, s_randomMode != CoinMode );
    lv_obj_set_hidden( m_diceHint, s_randomMode != DiceMode );
    lv_obj_invalidate( m_randomArea );
}

void ToolPages::onRandomEvent( lv_event_t *event )
{
    ToolPages *self = static_cast<ToolPages *>( lv_event_get_user_data( event ) );
    lv_obj_t *target = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    for ( int i = 0; i < RandomModeCount; ++i ) {
        if ( target != self->m_modes[i] ) {
            continue;
        }
        // The dice key again: one more die, after three one again.
        if ( i == DiceMode && s_randomMode == DiceMode ) {
            s_diceCount = s_diceCount % 3 + 1;
        }
        s_randomMode = i;
    }
    self->roll();
}

void ToolPages::onDrawRandom( lv_event_t *event )
{
    lv_obj_t *object = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t area = {};
    lv_obj_get_coords( object, &area );
    const int cx = ( area.x1 + area.x2 + 1 ) / 2;
    const int cy = ( area.y1 + area.y2 + 1 ) / 2;
    lv_draw_rect_dsc_t box;
    lv_draw_rect_dsc_init( &box );
    box.bg_color = lv_color_black();
    box.border_color = lv_color_white();
    box.border_width = 4;
    if ( s_randomMode == CoinMode ) {
        box.radius = LV_RADIUS_CIRCLE;
        box.border_color = lv_color_hex( Styles::Amber );
        const lv_area_t ring = { cx - 95, cy - 95, cx + 94, cy + 94 };
        lv_draw_rect( layer, &box, &ring );
        return;
    }
    if ( s_randomMode != DiceMode ) {
        return;
    }
    // The pips of each value on a 3 x 3 grid (bit = row * 3 + column).
    static const uint16_t s_pips[6] = { 1 << 4, 1 << 0 | 1 << 8, 1 << 0 | 1 << 4 | 1 << 8, 1 << 0 | 1 << 2 | 1 << 6 | 1 << 8,
                                        1 << 0 | 1 << 2 | 1 << 4 | 1 << 6 | 1 << 8,
                                        1 << 0 | 1 << 2 | 1 << 3 | 1 << 5 | 1 << 6 | 1 << 8 };
    const int count = s_diceCount < 1 ? 1 : ( s_diceCount > 3 ? 3 : s_diceCount );
    const int size = count == 1 ? 150 : ( count == 2 ? 120 : 96 );
    const int gap = 16;
    const int left = cx - ( count * size + ( count - 1 ) * gap ) / 2;
    lv_draw_rect_dsc_t pip;
    lv_draw_rect_dsc_init( &pip );
    pip.bg_color = lv_color_white();
    pip.radius = LV_RADIUS_CIRCLE;
    const int pipRadius = size / 11;
    for ( int d = 0; d < count; ++d ) {
        const int x = left + d * ( size + gap );
        const int y = cy - size / 2;
        box.radius = size / 7;
        const lv_area_t die = { x, y, x + size - 1, y + size - 1 };
        lv_draw_rect( layer, &box, &die );
        const int value = s_dice[d] < 1 || s_dice[d] > 6 ? 1 : s_dice[d];
        for ( int cell = 0; cell < 9; ++cell ) {
            if ( ( s_pips[value - 1] & ( 1 << cell ) ) == 0 ) {
                continue;
            }
            const int px = x + size * ( 1 + cell % 3 ) / 4;
            const int py = y + size * ( 1 + cell / 3 ) / 4;
            const lv_area_t dot = { px - pipRadius, py - pipRadius, px + pipRadius, py + pipRadius };
            lv_draw_rect( layer, &pip, &dot );
        }
    }
}

// ---- Development ----

bool ToolPages::press( const char *name )
{
    if ( m_count == nullptr ) {
        return false;
    }
    if ( strcmp( name, "count" ) == 0 ) {
        countUp();
    } else if ( strcmp( name, "undo" ) == 0 ) {
        m_tally.undo();
        saveCounter();
    } else if ( strcmp( name, "target" ) == 0 ) {
        m_tally.nextTarget();
        saveCounter();
    } else if ( strcmp( name, "reset" ) == 0 ) {
        m_tally.reset();
        saveCounter();
    } else if ( strcmp( name, "roll" ) == 0 ) {
        roll();
    } else if ( strcmp( name, "dice" ) == 0 || strcmp( name, "coin" ) == 0 || strcmp( name, "number" ) == 0 ) {
        const int mode = name[0] == 'd' ? DiceMode : ( name[0] == 'c' ? CoinMode : NumberMode );
        if ( mode == DiceMode && s_randomMode == DiceMode ) {
            s_diceCount = s_diceCount % 3 + 1;
        }
        s_randomMode = mode;
        roll();
    } else if ( strcmp( name, "torch" ) == 0 ) {
        s_torchColour = ( s_torchColour + 1 ) % TorchColourCount;
        m_sosStartUs = esp_timer_get_time();
        m_sosLit = true;
        applySound();
        showTorch();
    } else {
        return false;
    }
    return true;
}
