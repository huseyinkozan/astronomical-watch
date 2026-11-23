#include "ToolPages.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Fonts.h"
#include "Language.h"
#include "Morse.h"
#include "Panel.h"
#include "Ringer.h"
#include "Styles.h"
#include "Units.h"
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
// 25 a second: the torch's SOS and the spirit level's readings.
static const int s_tickMs = 40;
static const int64_t s_rollBlankUs = 200000;

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

// The calculator's text, result and modes; Calculator has constant initial values only, so the
// wakes do not reset it.
RTC_DATA_ATTR static Calculator s_calculator;

// The display over the keys (parts' coordinates: 12 px above the screen's, StackPage::HeaderPad):
// the value in 48 px with its right edge at x 360, under it the angle unit and the text typed,
// whose descenders ("(") end above the keys (screen y 140).
static const int s_displayRight = 360 - Panel::WIDTH;
static const int s_valueY = 36;
static const int s_textY = 88;
static const int s_keypadX = 40;
static const int s_keypadY = 128;
// Characters that fit: 48 px between the upper corner gauges, 26 px right of "DEG".
static const int s_valueChars = 11;
static const int s_textChars = 15;

// The unit converter: the kind shown, and per kind the unit last entered and its value.
static const Language::Text s_kindNames[Units::CategoryCount] = { Language::ConvAngleTime, Language::ConvDistance,
                                                                  Language::ConvTemperature };
static const Language::Text s_unitNames[Units::CategoryCount][Units::MaxUnits] = {
    { Language::UnitHourAngle, Language::UnitDegrees, Language::UnitRadians, Language::UnitRadians },
    { Language::UnitKilometres, Language::UnitAstronomical, Language::UnitLightYears, Language::UnitParsecs },
    { Language::UnitCelsius, Language::UnitFahrenheit, Language::UnitKelvin, Language::UnitKelvin } };
RTC_DATA_ATTR static int s_kind = Units::AngleTime;
RTC_DATA_ATTR static int s_kindUnit[Units::CategoryCount] = { Units::Degrees, Units::AstronomicalUnits, Units::Celsius };
RTC_DATA_ATTR static double s_kindValue[Units::CategoryCount] = { 187.5625, 1, 20 };
// The rows: up to four values of 17 characters with their unit under them.
static const int s_rowTop = 92;
static const int s_rowStep = 74;
static const int s_rowHeight = 70;
static const int s_rowChars = 17;

// The spirit level's modes and the sighting's inputs (metres above the eye, metres away), kept
// over the always-on wakes.
enum LevelMode
{
    LevelBubble,
    LevelHeight,    // the height from the distance
    LevelDistance,  // the distance from the height
    LevelModeCount
};
static const Language::Text s_levelModeNames[LevelModeCount] = { Language::LevelModeLevel, Language::LevelModeHeight,
                                                                 Language::ConvDistance };
RTC_DATA_ATTR static int s_levelMode = LevelBubble;
RTC_DATA_ATTR static double s_sightDistance = 20;
RTC_DATA_ATTR static double s_sightHeight = 10;
// The level's rings (part coordinates): centred at x 205, y 222, the outer one (45 degrees)
// 110 px; the bubble 15 px.
static const int s_dialX = 205;
static const int s_dialY = 222;
static const int s_dialRadius = 110;
static const int s_bubbleRadius = 15;
static const lv_color_t s_bubbleColour = LV_COLOR_MAKE( 0x40, 0xE0, 0x70 );
static const lv_color_t s_bandColour = LV_COLOR_MAKE( 0x60, 0x40, 0x10 );
static const double s_bandLow = 30;
static const double s_bandHigh = 45;
// The rings drawn: 1, 5, 15, 30 and 45 degrees.
static const int s_ringCount = 5;
static const double s_ringDegrees[s_ringCount] = { 1, 5, 15, 30, 45 };

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

void ToolPages::build( lv_obj_t *tile, lv_obj_t *pages, Settings &settings, Motion *motion, const EmergencyCard &card,
                       void ( *onLevel )( void *user ), void *user )
{
    m_tile = tile;
    m_motion = motion;
    m_pages = pages;
    m_settings = &settings;
    m_onLevel = onLevel;
    m_levelUser = user;
    m_stack.build( tile, PartCount, Language::text( Language::ToolsPageName ) );
    buildTorch( m_stack.part( TorchPart ) );
    buildCounter( m_stack.part( CounterPart ) );
    buildRandom( m_stack.part( RandomPart ) );
    buildCalculator( m_stack.part( CalculatorPart ) );
    buildConverter( m_stack.part( ConverterPart ) );
    buildLevel( m_stack.part( LevelPart ) );
    m_parts.build( m_stack.part( MetronomePart ), m_stack.part( CardPart ), card );
    lv_obj_add_event_cb( tile, onPartChanged, LV_EVENT_VALUE_CHANGED, this );
    m_tick = lv_timer_create( onTick, s_tickMs, this );
}

void ToolPages::forget()
{
    setTorchShown( false );
    setLevelShown( false );
    m_parts.forget();
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
    m_torchTitle = nullptr;
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
    m_rollShowUs = 0;
    m_calculatorPad.forget();
    m_calculatorValue = nullptr;
    m_calculatorText = nullptr;
    m_calculatorAngle = nullptr;
    m_kind = nullptr;
    m_kindLabel = nullptr;
    for ( int i = 0; i < 4; ++i ) {
        m_rows[i] = nullptr;
        m_rowValues[i] = nullptr;
        m_rowUnits[i] = nullptr;
    }
    m_entry = nullptr;
    m_entryValue = nullptr;
    m_entryUnit = nullptr;
    m_entryAngle = nullptr;
    m_entryPad.forget();
    m_entryRow = -1;
    m_levelMode = nullptr;
    m_levelModeLabel = nullptr;
    m_levelDial = nullptr;
    m_bubble = nullptr;
    m_levelAngles = nullptr;
    m_levelSlope = nullptr;
    m_levelHint = nullptr;
    m_sightArea = nullptr;
    m_sightAngle = nullptr;
    m_sightAngleLabel = nullptr;
    m_sightInput = nullptr;
    m_sightInputValue = nullptr;
    m_sightInputLabel = nullptr;
    m_sightResult = nullptr;
    m_sightResultLabel = nullptr;
    m_sightHint = nullptr;
    m_levelEntry = nullptr;
    m_levelEntryValue = nullptr;
    m_levelEntryUnit = nullptr;
    m_levelEntryAngle = nullptr;
    m_levelPad.forget();
    m_levelEntryOpen = false;
    m_held = false;
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
    // Swiping away from an entry leaves it, and from the level its held angle.
    self->closeEntry();
    self->closeLevelEntry();
    self->m_held = false;
}

void ToolPages::onTick( lv_timer_t *timer )
{
    ToolPages *self = static_cast<ToolPages *>( lv_timer_get_user_data( timer ) );
    if ( self->m_levelShown ) {
        self->readLevel();
    }
    self->m_parts.tick();
    if ( self->m_rollShowUs != 0 && esp_timer_get_time() >= self->m_rollShowUs ) {
        self->m_rollShowUs = 0;
        self->showRandom();
    }
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
    // The colour as the title, stepped by a tap like the other parts' titles: a held finger
    // hid the text under it. Where the stacked pages show their first row (screen y, the part has
    // no header padding here).
    m_torchTitle = lv_obj_get_child( button( m_torch, s_left, StackPage::HeaderPad + 36, s_right, StackPage::HeaderPad + 84,
                                             "", onTorchTitle, this ), 0 );
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
    setText( m_torchTitle, Language::text( s_torchNames[s_torchColour] ) );
    char status[24];
    snprintf( status, sizeof( status ), "%s  %d:%02d", percent, left / 60, left % 60 );
    setText( m_torchStatus, status );
}

void ToolPages::stepTorch()
{
    s_torchColour = ( s_torchColour + 1 ) % TorchColourCount;
    // SOS from its start, so that it reads from the first letter.
    m_sosStartUs = esp_timer_get_time();
    m_sosLit = true;
    applySound();
    showTorch();
}

void ToolPages::onTorchTitle( lv_event_t *event )
{
    ToolPages *self = static_cast<ToolPages *>( lv_event_get_user_data( event ) );
    self->stepTorch();
    // A tap, so the auto-off starts again.
    self->restartTorch();
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
        ringer.stopTool( false );
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
            // Round the list, so that SOS is a step or two away in either direction from any
            // place the finger started.
            int colour = ( self->m_startColour + dx / s_colourStepPx ) % TorchColourCount;
            colour = colour < 0 ? colour + TorchColourCount : colour;
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
    m_rollShowUs = esp_timer_get_time() + s_rollBlankUs;
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
    const bool blank = m_rollShowUs != 0;
    lv_obj_set_hidden( m_number, blank || s_randomMode != NumberMode );
    lv_obj_set_hidden( m_coin, blank || s_randomMode != CoinMode );
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
    const ToolPages *self = static_cast<ToolPages *>( lv_event_get_user_data( event ) );
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
        if ( self->m_rollShowUs != 0 ) {
            continue;  // the dice blank for a moment before the new roll
        }
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

// ---- Calculator ----

// The display's three labels on `parent`: the value, the text (or the unit entered) and the
// angle unit.
static void buildDisplay( lv_obj_t *parent, lv_obj_t **value, lv_obj_t **text, lv_obj_t **angle )
{
    *value = label( parent, &jetbrains_mono_48, lv_color_white(), LV_ALIGN_TOP_RIGHT, s_displayRight, s_valueY, "" );
    *text = label( parent, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_RIGHT, s_displayRight, s_textY, "" );
    *angle = label( parent, &jetbrains_mono_26, s_dim, LV_ALIGN_TOP_LEFT, s_keypadX, s_textY, "" );
}

// The end of `text` in `maxChars` characters, "…" first when it was cut.
static void showTail( lv_obj_t *object, const char *text, int maxChars )
{
    const char *tail = Calculator::tail( text, maxChars );
    char shown[Calculator::MaxText + 4];
    if ( tail == text ) {
        snprintf( shown, sizeof( shown ), "%s", text );
    } else {
        snprintf( shown, sizeof( shown ), "…%s", Calculator::tail( text, maxChars - 1 ) );
    }
    setText( object, shown );
}

void ToolPages::buildCalculator( lv_obj_t *part )
{
    buildDisplay( part, &m_calculatorValue, &m_calculatorText, &m_calculatorAngle );
    m_calculatorPad.build( part, s_keypadX, s_keypadY, s_calculator, onCalculatorKey, this );
    showCalculator();
}

void ToolPages::showCalculator()
{
    if ( m_calculatorValue == nullptr ) {
        return;
    }
    char text[24] = "";
    lv_color_t colour = lv_color_white();
    double value = 0;
    if ( s_calculator.state() == Calculator::Failed ) {
        snprintf( text, sizeof( text ), "%s", Language::text( Language::CalcError ) );
        colour = lv_color_hex( Styles::Amber );
    } else if ( s_calculator.state() == Calculator::Result ) {
        Calculator::format( s_calculator.value(), s_valueChars, text, sizeof( text ) );
    } else if ( s_calculator.preview( value ) ) {
        // The live result in grey, until "=".
        Calculator::format( value, s_valueChars, text, sizeof( text ) );
        colour = s_grey;
    } else if ( s_calculator.text()[0] == '\0' ) {
        snprintf( text, sizeof( text ), "0" );
        colour = s_dim;
    }
    setText( m_calculatorValue, text );
    lv_obj_set_style_text_color( m_calculatorValue, colour, 0 );
    showTail( m_calculatorText, s_calculator.text(), s_textChars );
    setText( m_calculatorAngle, s_calculator.label( Calculator::Angle ) );
    m_calculatorPad.refresh();
}

void ToolPages::onCalculatorKey( void *user, Calculator::Key key )
{
    ToolPages *self = static_cast<ToolPages *>( user );
    s_calculator.press( key );
    self->showCalculator();
}

// ---- Unit converter ----

void ToolPages::buildConverter( lv_obj_t *part )
{
    m_kind = button( part, s_left, 36, s_right, 84, "", onConverterEvent, this );
    m_kindLabel = lv_obj_get_child( m_kind, 0 );
    for ( int i = 0; i < 4; ++i ) {
        const int y = s_rowTop + i * s_rowStep;
        m_rows[i] = button( part, s_left, y, s_right, y + s_rowHeight, "", onConverterEvent, this );
        m_rowValues[i] = lv_obj_get_child( m_rows[i], 0 );
        lv_obj_align( m_rowValues[i], LV_ALIGN_TOP_LEFT, 16, 4 );
        m_rowUnits[i] = label( m_rows[i], &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_LEFT, 16, 34, "" );
    }
    lv_obj_t *hint = label( part, &jetbrains_mono_26, s_dim, LV_ALIGN_TOP_MID, 0, 390, Language::text( Language::ConvHint ) );
    Styles::centred( hint );

    // The entry: the calculator's display and keys over the whole part, hidden until a row is tapped.
    m_entry = lv_obj_create( part );
    lv_obj_remove_style_all( m_entry );
    lv_obj_set_size( m_entry, Panel::WIDTH, Panel::HEIGHT - StackPage::HeaderPad );
    lv_obj_set_scrollable( m_entry, false );
    lv_obj_set_style_bg_color( m_entry, lv_color_black(), 0 );
    lv_obj_set_style_bg_opa( m_entry, LV_OPA_COVER, 0 );
    buildDisplay( m_entry, &m_entryValue, &m_entryUnit, &m_entryAngle );
    m_entryPad.build( m_entry, s_keypadX, s_keypadY, m_entryCalculator, onEntryKey, this );
    lv_obj_set_hidden( m_entry, m_entryRow < 0 );
    showConverter();
    showEntry();
}

// A value in its row: h:m:s for the hour angle, else up to 10 significant digits.
static void formatRow( int kind, int unit, double value, char *text, int size )
{
    bool negative = false;
    int h = 0;
    int m = 0;
    int s = 0;
    if ( kind == Units::AngleTime && unit == Units::Hours && Units::splitHms( value, negative, h, m, s ) ) {
        snprintf( text, size, Language::text( Language::HmsValue ), negative ? "-" : "", h, m, s );
        return;
    }
    Calculator::format( value, s_rowChars, text, size );
}

void ToolPages::showConverter()
{
    if ( m_kind == nullptr ) {
        return;
    }
    setText( m_kindLabel, Language::text( s_kindNames[s_kind] ) );
    const int count = Units::unitCount( s_kind );
    for ( int i = 0; i < 4; ++i ) {
        lv_obj_set_hidden( m_rows[i], i >= count );
        if ( i >= count ) {
            continue;
        }
        char text[40];
        formatRow( s_kind, i, Units::convert( s_kind, s_kindUnit[s_kind], i, s_kindValue[s_kind] ), text, sizeof( text ) );
        setText( m_rowValues[i], text );
        setText( m_rowUnits[i], Language::text( s_unitNames[s_kind][i] ) );
        // The unit entered last has the amber outline.
        if ( i == s_kindUnit[s_kind] ) {
            lv_obj_add_state( m_rows[i], LV_STATE_CHECKED );
        } else {
            lv_obj_remove_state( m_rows[i], LV_STATE_CHECKED );
        }
    }
}

void ToolPages::onConverterEvent( lv_event_t *event )
{
    ToolPages *self = static_cast<ToolPages *>( lv_event_get_user_data( event ) );
    lv_obj_t *target = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    if ( target == self->m_kind ) {
        s_kind = ( s_kind + 1 ) % Units::CategoryCount;
        self->showConverter();
        return;
    }
    for ( int i = 0; i < 4; ++i ) {
        if ( target == self->m_rows[i] ) {
            self->openEntry( i );
        }
    }
}

void ToolPages::openEntry( int row )
{
    if ( m_entry == nullptr || row < 0 || row >= Units::unitCount( s_kind ) ) {
        return;
    }
    m_entryRow = row;
    m_entryFailed = false;
    m_entryCalculator.clear();
    // h:m:s: the point key writes ":".
    m_entryCalculator.setColon( s_kind == Units::AngleTime && row == Units::Hours );
    lv_obj_set_hidden( m_entry, false );
    showEntry();
}

void ToolPages::closeEntry()
{
    m_entryRow = -1;
    if ( m_entry != nullptr ) {
        lv_obj_set_hidden( m_entry, true );
    }
}

void ToolPages::showEntry()
{
    if ( m_entry == nullptr || m_entryRow < 0 ) {
        return;
    }
    const char *text = m_entryCalculator.text();
    if ( m_entryFailed ) {
        setText( m_entryValue, Language::text( Language::CalcError ) );
        lv_obj_set_style_text_color( m_entryValue, lv_color_hex( Styles::Amber ), 0 );
    } else if ( text[0] == '\0' ) {
        setText( m_entryValue, "0" );
        lv_obj_set_style_text_color( m_entryValue, s_dim, 0 );
    } else {
        showTail( m_entryValue, text, s_valueChars );
        lv_obj_set_style_text_color( m_entryValue, lv_color_white(), 0 );
    }
    setText( m_entryUnit, Language::text( s_unitNames[s_kind][m_entryRow] ) );
    setText( m_entryAngle, m_entryCalculator.label( Calculator::Angle ) );
    m_entryPad.refresh();
}

void ToolPages::onEntryKey( void *user, Calculator::Key key )
{
    ToolPages *self = static_cast<ToolPages *>( user );
    const int row = self->m_entryRow;
    if ( row < 0 ) {
        return;
    }
    const char *text = self->m_entryCalculator.text();
    const bool empty = text[0] == '\0';
    if ( empty && ( key == Calculator::Clear || key == Calculator::Equals ) ) {
        self->closeEntry();
        return;
    }
    if ( key != Calculator::Equals ) {
        self->m_entryFailed = false;
        self->m_entryCalculator.press( key );
        self->showEntry();
        return;
    }
    // h:m:s with colons, anything else as an expression in the row's unit.
    double value = 0;
    bool valid = false;
    if ( s_kind == Units::AngleTime && row == Units::Hours && strchr( text, ':' ) != nullptr ) {
        valid = Units::parseHms( text, value );
    } else {
        valid = Calculator::evaluate( text, self->m_entryCalculator.radians(), value );
    }
    if ( valid && Units::valid( s_kind, row, value ) ) {
        s_kindUnit[s_kind] = row;
        s_kindValue[s_kind] = value;
        self->closeEntry();
        self->showConverter();
        ESP_LOGI( TAG, "Converter: kind %d, unit %d", s_kind, row );
    } else {
        self->m_entryFailed = true;
        self->showEntry();
    }
}

// Sets the text colour only when it changed: a style set redraws the object even with the same
// value, and the level and the alidade update 25 times a second.
static void setColour( lv_obj_t *object, lv_color_t colour )
{
    if ( !lv_color_eq( lv_obj_get_style_text_color( object, LV_PART_MAIN ), colour ) ) {
        lv_obj_set_style_text_color( object, colour, 0 );
    }
}

// ---- Spirit level ----

// `value` with one decimal, without a "-0.0".
static double oneDecimal( double value )
{
    const double rounded = std::round( value * 10.0 ) / 10.0;
    return rounded == 0.0 ? 0.0 : rounded;
}

// An angle of the level: one decimal below 10 degrees, whole degrees above (the row's width).
static void formatTilt( double degrees, char *text, int size )
{
    if ( std::fabs( degrees ) < 9.95 ) {
        snprintf( text, size, "%.1f°", oneDecimal( degrees ) );
    } else {
        snprintf( text, size, "%.0f°", degrees );
    }
}

void ToolPages::buildLevel( lv_obj_t *part )
{
    m_levelMode = button( part, s_left, 36, s_right, 84, "", onLevelEvent, this );
    m_levelModeLabel = lv_obj_get_child( m_levelMode, 0 );

    // The level: the rings drawn by one object, the bubble moved over them (LVGL redraws only
    // where it was and where it goes).
    m_levelDial = lv_obj_create( part );
    lv_obj_remove_style_all( m_levelDial );
    lv_obj_set_pos( m_levelDial, s_dialX - s_dialRadius - 2, s_dialY - s_dialRadius - 2 );
    lv_obj_set_size( m_levelDial, 2 * s_dialRadius + 4, 2 * s_dialRadius + 4 );
    lv_obj_set_clickable( m_levelDial, false );
    lv_obj_add_event_cb( m_levelDial, onDrawLevel, LV_EVENT_DRAW_POST, nullptr );
    m_bubble = lv_obj_create( part );
    lv_obj_remove_style_all( m_bubble );
    lv_obj_set_size( m_bubble, 2 * s_bubbleRadius, 2 * s_bubbleRadius );
    lv_obj_set_style_radius( m_bubble, LV_RADIUS_CIRCLE, 0 );
    lv_obj_set_style_border_width( m_bubble, 3, 0 );
    lv_obj_set_style_border_color( m_bubble, s_bubbleColour, 0 );
    lv_obj_set_clickable( m_bubble, false );
    m_levelAngles = label( part, &jetbrains_mono_26, lv_color_white(), LV_ALIGN_TOP_MID, 0, 340, "" );
    m_levelSlope = label( part, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_MID, 0, 374, "" );
    m_levelHint = label( part, &jetbrains_mono_26, s_dim, LV_ALIGN_TOP_MID, 0, 408, Language::text( Language::LevelHint ) );

    // Sighting: the edge's altitude (a tap holds it), the value entered and the answer.
    m_sightArea = lv_obj_create( part );
    lv_obj_remove_style_all( m_sightArea );
    lv_obj_set_pos( m_sightArea, s_left, 92 );
    lv_obj_set_size( m_sightArea, s_right - s_left, 108 );
    lv_obj_set_scrollable( m_sightArea, false );
    lv_obj_add_event_cb( m_sightArea, onLevelEvent, LV_EVENT_PRESSED, this );
    lv_obj_add_event_cb( m_sightArea, onLevelEvent, LV_EVENT_CLICKED, this );
    m_sightAngle = label( part, &jetbrains_mono_48, lv_color_white(), LV_ALIGN_TOP_MID, 0, 104, "" );
    m_sightAngleLabel = label( part, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_MID, 0, 160, "" );
    m_sightInput = button( part, s_left, 206, s_right, 276, "", onLevelEvent, this );
    m_sightInputValue = lv_obj_get_child( m_sightInput, 0 );
    lv_obj_align( m_sightInputValue, LV_ALIGN_TOP_LEFT, 16, 4 );
    m_sightInputLabel = label( m_sightInput, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_LEFT, 16, 34, "" );
    m_sightResult = label( part, &jetbrains_mono_48, lv_color_hex( Styles::Amber ), LV_ALIGN_TOP_MID, 0, 290, "" );
    m_sightResultLabel = label( part, &jetbrains_mono_26, s_grey, LV_ALIGN_TOP_MID, 0, 346, "" );
    m_sightHint = label( part, &jetbrains_mono_26, s_dim, LV_ALIGN_TOP_MID, 0, 386, Language::text( Language::SightHint ) );
    Styles::centred( m_sightHint );

    // The entry of the distance or the height, over the whole part as the converter's.
    m_levelEntry = lv_obj_create( part );
    lv_obj_remove_style_all( m_levelEntry );
    lv_obj_set_size( m_levelEntry, Panel::WIDTH, Panel::HEIGHT - StackPage::HeaderPad );
    lv_obj_set_scrollable( m_levelEntry, false );
    lv_obj_set_style_bg_color( m_levelEntry, lv_color_black(), 0 );
    lv_obj_set_style_bg_opa( m_levelEntry, LV_OPA_COVER, 0 );
    buildDisplay( m_levelEntry, &m_levelEntryValue, &m_levelEntryUnit, &m_levelEntryAngle );
    m_levelPad.build( m_levelEntry, s_keypadX, s_keypadY, m_entryCalculator, onLevelKey, this );
    lv_obj_set_hidden( m_levelEntry, true );
    m_levelEntryOpen = false;
    showLevel();
}

void ToolPages::onDrawLevel( lv_event_t *event )
{
    lv_obj_t *object = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    lv_area_t area = {};
    lv_obj_get_coords( object, &area );
    const int cx = ( area.x1 + area.x2 + 1 ) / 2;
    const int cy = ( area.y1 + area.y2 + 1 ) / 2;
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init( &arc );
    arc.center.x = cx;
    arc.center.y = cy;
    arc.start_angle = 0;
    arc.end_angle = 360;
    // The slope band, 30 to 45 degrees, as a wide ring under the lines.
    const int bandInner = static_cast<int>( std::lround( s_dialRadius * Tilt::ringFraction( s_bandLow ) ) );
    arc.color = s_bandColour;
    arc.radius = s_dialRadius;
    arc.width = s_dialRadius - bandInner;
    lv_draw_arc( layer, &arc );
    arc.color = s_dim;
    arc.width = 2;
    for ( int i = 0; i < s_ringCount; ++i ) {
        arc.radius = static_cast<int>( std::lround( s_dialRadius * Tilt::ringFraction( s_ringDegrees[i] ) ) ) + 1;
        lv_draw_arc( layer, &arc );
    }
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init( &line );
    line.color = s_dim;
    line.width = 2;
    line.p1.x = cx - s_dialRadius;
    line.p1.y = cy;
    line.p2.x = cx + s_dialRadius;
    line.p2.y = cy;
    lv_draw_line( layer, &line );
    line.p1.x = cx;
    line.p1.y = cy - s_dialRadius;
    line.p2.x = cx;
    line.p2.y = cy + s_dialRadius;
    lv_draw_line( layer, &line );
}

void ToolPages::setLevelShown( bool shown )
{
    // The accelerometer's own state counts too, as the observing page's alidade switches the
    // same stream: a flag of our own left on by a quick page change would not start it again.
    if ( shown == m_levelShown && ( m_motion == nullptr || m_motion->streaming() == shown ) ) {
        return;
    }
    m_levelShown = shown;
    m_tilt.reset();
    if ( m_motion != nullptr ) {
        m_motion->setStreaming( shown );
    }
    showLevel();
}

void ToolPages::readLevel()
{
    int16_t mg[3] = {};
    if ( m_motion == nullptr || m_motion->readLatest( mg ) != ESP_OK ) {
        return;
    }
    m_tilt.add( Tilt::fromSensor( mg[0], mg[1], mg[2], m_settings->value( Settings::Mirror ) ) );
    showLevel();
}

void ToolPages::showLevel()
{
    if ( m_levelMode == nullptr ) {
        return;
    }
    setText( m_levelModeLabel, Language::text( s_levelModeNames[s_levelMode] ) );
    const bool bubble = s_levelMode == LevelBubble;
    lv_obj_set_hidden( m_levelDial, !bubble );
    lv_obj_set_hidden( m_bubble, !bubble );
    lv_obj_set_hidden( m_levelAngles, !bubble );
    lv_obj_set_hidden( m_levelSlope, !bubble );
    lv_obj_set_hidden( m_levelHint, !bubble );
    lv_obj_t *sight[7] = { m_sightArea, m_sightAngle, m_sightAngleLabel, m_sightInput, m_sightResult,
                           m_sightResultLabel, m_sightHint };
    for ( int i = 0; i < 7; ++i ) {
        lv_obj_set_hidden( sight[i], bubble );
    }
    const bool valid = m_tilt.valid();
    const Tilt::Vector v = m_tilt.value();
    char text[48];
    if ( bubble ) {
        double right = 0;
        double up = 0;
        if ( valid ) {
            Tilt::bubble( v, right, up );
        }
        const int x = s_dialX + static_cast<int>( std::lround( right * s_dialRadius ) ) - s_bubbleRadius;
        const int y = s_dialY - static_cast<int>( std::lround( up * s_dialRadius ) ) - s_bubbleRadius;
        // Only when it moves: a style set redraws even at the same place.
        if ( lv_obj_get_style_x( m_bubble, LV_PART_MAIN ) != x || lv_obj_get_style_y( m_bubble, LV_PART_MAIN ) != y ) {
            lv_obj_set_pos( m_bubble, x, y );
        }
        if ( valid ) {
            char pitch[12];
            char roll[12];
            formatTilt( Tilt::pitch( v ), pitch, sizeof( pitch ) );
            formatTilt( Tilt::roll( v ), roll, sizeof( roll ) );
            snprintf( text, sizeof( text ), Language::text( Language::PitchRoll ), pitch, roll );
        } else {
            snprintf( text, sizeof( text ), "%s", "-" );
        }
        setText( m_levelAngles, text );
        const double slope = Tilt::slope( v );
        if ( valid ) {
            snprintf( text, sizeof( text ), Language::text( Language::SlopeValue ), oneDecimal( slope ) );
        } else {
            text[0] = '\0';
        }
        setText( m_levelSlope, text );
        // Avalanche terrain: the band's slopes in amber.
        const bool inBand = valid && slope >= s_bandLow && slope <= s_bandHigh;
        setColour( m_levelSlope, inBand ? lv_color_hex( Styles::Amber ) : s_grey );
        return;
    }
    const double altitude = m_held ? m_heldAltitude : Tilt::edgeAltitude( v );
    if ( valid || m_held ) {
        snprintf( text, sizeof( text ), "%.1f°", oneDecimal( altitude ) );
    } else {
        snprintf( text, sizeof( text ), "%s", "-" );
    }
    setText( m_sightAngle, text );
    setColour( m_sightAngle, m_held ? lv_color_hex( Styles::Amber ) : lv_color_white() );
    setText( m_sightAngleLabel, Language::text( m_held ? Language::SightHeld : Language::EdgeAltitude ) );
    const bool height = s_levelMode == LevelHeight;
    char number[24];
    Calculator::format( height ? s_sightDistance : s_sightHeight, 12, number, sizeof( number ) );
    snprintf( text, sizeof( text ), "%s m", number );
    setText( m_sightInputValue, text );
    setText( m_sightInputLabel, Language::text( height ? Language::SightDistance : Language::SightHeight ) );
    double answer = 0;
    const bool solved = ( valid || m_held ) && ( height ? Tilt::heightFromDistance( s_sightDistance, altitude, answer )
                                                        : Tilt::distanceFromHeight( s_sightHeight, altitude, answer ) );
    if ( solved && answer < 1e6 ) {
        snprintf( text, sizeof( text ), "%.1f m", oneDecimal( answer ) );
    } else {
        snprintf( text, sizeof( text ), "%s", "-" );
    }
    setText( m_sightResult, text );
    setText( m_sightResultLabel, Language::text( height ? Language::SightHeight : Language::SightDistance ) );
}

void ToolPages::onLevelEvent( lv_event_t *event )
{
    ToolPages *self = static_cast<ToolPages *>( lv_event_get_user_data( event ) );
    lv_obj_t *target = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    if ( target == self->m_levelMode ) {
        s_levelMode = ( s_levelMode + 1 ) % LevelModeCount;
        self->m_held = false;
        self->showLevel();
    } else if ( target == self->m_sightInput ) {
        self->openLevelEntry();
    } else if ( target == self->m_sightArea ) {
        if ( lv_event_get_code( event ) == LV_EVENT_PRESSED ) {
            self->m_pressAltitude = Tilt::edgeAltitude( self->m_tilt.value() );
            return;
        }
        self->m_held = !self->m_held && self->m_tilt.valid();
        self->m_heldAltitude = self->m_pressAltitude;
        self->showLevel();
        ESP_LOGI( TAG, "Sighting angle %s", self->m_held ? "held" : "released" );
    }
}

void ToolPages::openLevelEntry()
{
    if ( m_levelEntry == nullptr ) {
        return;
    }
    m_levelEntryOpen = true;
    m_entryFailed = false;
    m_entryCalculator.clear();
    m_entryCalculator.setColon( false );
    lv_obj_set_hidden( m_levelEntry, false );
    showLevelEntry();
}

void ToolPages::closeLevelEntry()
{
    m_levelEntryOpen = false;
    if ( m_levelEntry != nullptr ) {
        lv_obj_set_hidden( m_levelEntry, true );
    }
}

void ToolPages::showLevelEntry()
{
    if ( m_levelEntry == nullptr || !m_levelEntryOpen ) {
        return;
    }
    const char *text = m_entryCalculator.text();
    if ( m_entryFailed ) {
        setText( m_levelEntryValue, Language::text( Language::CalcError ) );
        lv_obj_set_style_text_color( m_levelEntryValue, lv_color_hex( Styles::Amber ), 0 );
    } else if ( text[0] == '\0' ) {
        setText( m_levelEntryValue, "0" );
        lv_obj_set_style_text_color( m_levelEntryValue, s_dim, 0 );
    } else {
        showTail( m_levelEntryValue, text, s_valueChars );
        lv_obj_set_style_text_color( m_levelEntryValue, lv_color_white(), 0 );
    }
    char unit[40];
    snprintf( unit, sizeof( unit ), "%s, m",
              Language::text( s_levelMode == LevelHeight ? Language::SightDistance : Language::SightHeight ) );
    setText( m_levelEntryUnit, unit );
    setText( m_levelEntryAngle, "" );
    m_levelPad.refresh();
}

void ToolPages::onLevelKey( void *user, Calculator::Key key )
{
    ToolPages *self = static_cast<ToolPages *>( user );
    if ( !self->m_levelEntryOpen ) {
        return;
    }
    const char *text = self->m_entryCalculator.text();
    if ( text[0] == '\0' && ( key == Calculator::Clear || key == Calculator::Equals ) ) {
        self->closeLevelEntry();
        return;
    }
    if ( key != Calculator::Equals ) {
        self->m_entryFailed = false;
        self->m_entryCalculator.press( key );
        self->showLevelEntry();
        return;
    }
    double value = 0;
    if ( Calculator::evaluate( text, self->m_entryCalculator.radians(), value ) && value > 0 && value < 1e6 ) {
        if ( s_levelMode == LevelHeight ) {
            s_sightDistance = value;
        } else {
            s_sightHeight = value;
        }
        self->closeLevelEntry();
        self->showLevel();
    } else {
        self->m_entryFailed = true;
        self->showLevelEntry();
    }
}

// ---- Development ----

void ToolPages::setMetronomeShown( bool shown )
{
    m_parts.setMetronomeShown( shown );
}

int ToolPages::metronomeSecondsLeft() const
{
    return m_parts.metronomeSecondsLeft();
}

void ToolPages::setCard( const EmergencyCard &card )
{
    if ( m_tile != nullptr ) {
        m_parts.showCard( card );
    }
}

bool ToolPages::press( const char *name )
{
    if ( m_count == nullptr ) {
        return false;
    }
    if ( m_parts.press( name ) ) {
        return true;
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
    } else if ( strncmp( name, "keys ", 5 ) == 0 && m_levelEntryOpen ) {
        for ( const char *k = name + 5; *k != '\0' && m_levelEntryOpen; ++k ) {
            onLevelKey( this, Calculator::keyForChar( *k ) );
        }
    } else if ( strncmp( name, "keys ", 5 ) == 0 ) {
        const bool entry = m_entryRow >= 0;
        for ( const char *k = name + 5; *k != '\0'; ++k ) {
            // Keys after the entry closed are dropped.
            if ( entry && m_entryRow >= 0 ) {
                onEntryKey( this, Calculator::keyForChar( *k ) );
            } else if ( !entry ) {
                onCalculatorKey( this, Calculator::keyForChar( *k ) );
            }
        }
    } else if ( strcmp( name, "kind" ) == 0 ) {
        s_kind = ( s_kind + 1 ) % Units::CategoryCount;
        showConverter();
    } else if ( strncmp( name, "unit ", 5 ) == 0 ) {
        openEntry( atoi( name + 5 ) );
    } else if ( strcmp( name, "level" ) == 0 ) {
        s_levelMode = ( s_levelMode + 1 ) % LevelModeCount;
        m_held = false;
        showLevel();
    } else if ( strcmp( name, "hold" ) == 0 ) {
        m_held = !m_held;
        m_heldAltitude = Tilt::edgeAltitude( m_tilt.value() );
        showLevel();
    } else if ( strcmp( name, "input" ) == 0 ) {
        openLevelEntry();
    } else if ( strcmp( name, "torch" ) == 0 ) {
        stepTorch();
    } else {
        return false;
    }
    return true;
}
