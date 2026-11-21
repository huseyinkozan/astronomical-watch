#pragma once

#include <cstdint>

#include "Calculator.h"
#include "Keypad.h"
#include "Settings.h"
#include "StackPage.h"
#include "Tally.h"
#include "lvgl.h"

// The tools page (docs/superpowers/plans/2026-11-21-tools.md): the torch, the counter, a random
// roll, the calculator and the unit converter, one over the other (StackPage). Display builds it and calls it with the display
// lock held; tools run only in active mode.
//
// Torch: the whole screen in one colour (red, amber, white, then SOS flashes, with beeps or
// without). A long press, then a drag: sideways the colour, up or down the panel's brightness
// while the torch shows (Display sets the panel). A tap restarts its 2 min auto-off; meanwhile the
// display timeout does not end active mode, and at 0:00 active mode ends as at a timeout (main).
// Counter: a large count with a target (Tally), +1 and undo keys; a long press on the count
// resets it; the BOOT key counts too (Settings::BootCounter, Display::back). Random: dice, a coin
// or a number from 1 to 100, a tap rolls.
// Calculator: an expression on a keypad (Calculator, Keypad), its live result above it, the
// result after "="; fn shows the functions, then their inverses. Unit converter: one kind of
// units at a time (Units), the kind chosen by a tap on its title (the vertical swipe stays the
// parts' step); a tap on a value opens the calculator's keypad over the part to enter it in that
// unit ("=" takes it, C or "=" on nothing goes back).
class ToolPages
{
public:
    enum Part
    {
        TorchPart,
        CounterPart,
        RandomPart,
        CalculatorPart,
        ConverterPart,
        PartCount  // the parts are numbered in the saved orders (PartOrder): new ones at the end
    };

    // `pages`: the tileview, held still while the torch is set by a drag. `onLevel` is called
    // with `user` when the torch's brightness changes (Display sets the panel).
    void build( lv_obj_t *tile, lv_obj_t *pages, Settings &settings, void ( *onLevel )( void *user ), void *user );

    // The objects were deleted with the tile's contents; the values stay for the next build.
    void forget();

    StackPage &stack();

    // Display: whether the torch is on screen (its tile shown, its part current). Showing it
    // starts the auto-off and the SOS clock; hiding it stops the beeps.
    void setTorchShown( bool shown );
    // The panel's brightness while the torch shows.
    int torchLevel() const;
    // Seconds until the torch's auto-off while it shows, -1 otherwise.
    int torchSecondsLeft() const;

    // The BOOT key on the counter.
    void countUp();

    // Every second: Unix time (0: not known), for the counter's rate.
    void update( int64_t utcSeconds );

    // Development (host command "B <name>"): count, undo, target, reset, roll, dice, coin,
    // number, torch (the next colour), "keys <keys>" (Calculator::keyForChar; the converter's
    // entry while it is open, else the calculator), kind (the converter's next kind), "unit <n>"
    // (opens the entry of the converter's row n).
    bool press( const char *name );

private:
    static void onTick( lv_timer_t *timer );
    static void onTorchEvent( lv_event_t *event );
    static void onCounterButton( lv_event_t *event );
    static void onCountPressed( lv_event_t *event );
    static void onRandomEvent( lv_event_t *event );
    static void onDrawRandom( lv_event_t *event );
    static void onDrawUndo( lv_event_t *event );
    static void onPartChanged( lv_event_t *event );
    void buildTorch( lv_obj_t *part );
    void buildCounter( lv_obj_t *part );
    void buildRandom( lv_obj_t *part );
    void showTorch();
    void restartTorch();
    void endTorchAdjust();
    // Starts or stops the SOS beeps as the torch and its colour want them.
    void applySound();
    void showCounter();
    void saveCounter();
    void roll();
    void showRandom();
    static void onCalculatorKey( void *user, Calculator::Key key );
    static void onEntryKey( void *user, Calculator::Key key );
    static void onConverterEvent( lv_event_t *event );
    void buildCalculator( lv_obj_t *part );
    void buildConverter( lv_obj_t *part );
    void showCalculator();
    void showConverter();
    void openEntry( int row );
    void closeEntry();
    void showEntry();

    StackPage m_stack;
    lv_obj_t *m_tile = nullptr;
    lv_obj_t *m_pages = nullptr;
    lv_timer_t *m_tick = nullptr;
    Settings *m_settings = nullptr;
    void ( *m_onLevel )( void *user ) = nullptr;
    void *m_levelUser = nullptr;

    // Torch.
    lv_obj_t *m_torch = nullptr;
    lv_obj_t *m_torchHint = nullptr;
    lv_obj_t *m_torchStatus = nullptr;
    bool m_torchShown = false;
    int64_t m_torchEndUs = 0;
    int64_t m_sosStartUs = 0;
    bool m_sosLit = false;
    bool m_soundOn = false;
    int m_shownSeconds = -1;  // the countdown shown, so that the text changes once a second
    // A long press, then the drag's direction decides colour or brightness.
    bool m_adjusting = false;
    bool m_axisChosen = false;
    bool m_sideways = false;
    bool m_pressMoved = false;
    int m_pressX = 0;
    int m_pressY = 0;
    int m_startColour = 0;
    int m_startLevel = 0;

    // Counter.
    Tally m_tally;
    int64_t m_utcSeconds = 0;
    lv_obj_t *m_count = nullptr;
    lv_obj_t *m_goal = nullptr;
    lv_obj_t *m_rate = nullptr;
    lv_obj_t *m_plusOne = nullptr;
    lv_obj_t *m_undo = nullptr;
    lv_obj_t *m_target = nullptr;
    lv_obj_t *m_targetLabel = nullptr;
    lv_obj_t *m_bootHint = nullptr;

    // Random.
    lv_obj_t *m_randomArea = nullptr;
    lv_obj_t *m_number = nullptr;
    lv_obj_t *m_coin = nullptr;
    lv_obj_t *m_modes[3] = {};
    lv_obj_t *m_diceHint = nullptr;

    // Calculator (its state in RTC memory, ToolPages.cpp).
    Keypad m_calculatorPad;
    lv_obj_t *m_calculatorValue = nullptr;
    lv_obj_t *m_calculatorText = nullptr;
    lv_obj_t *m_calculatorAngle = nullptr;

    // Unit converter, and the entry of a value over it.
    lv_obj_t *m_kind = nullptr;
    lv_obj_t *m_kindLabel = nullptr;
    lv_obj_t *m_rows[4] = {};
    lv_obj_t *m_rowValues[4] = {};
    lv_obj_t *m_rowUnits[4] = {};
    lv_obj_t *m_entry = nullptr;
    lv_obj_t *m_entryValue = nullptr;
    lv_obj_t *m_entryUnit = nullptr;
    lv_obj_t *m_entryAngle = nullptr;
    Keypad m_entryPad;
    Calculator m_entryCalculator;
    int m_entryRow = -1;  // the row entered, -1: the entry is closed
    bool m_entryFailed = false;
};
