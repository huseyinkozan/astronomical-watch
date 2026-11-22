#pragma once

#include <cstdint>

#include "Calculator.h"
#include "Cycle.h"
#include "CycleLog.h"
#include "Keypad.h"
#include "StackPage.h"
#include "lvgl.h"

// The cycle page (docs/superpowers/specs/2026-11-22-cycle-page-design.md): menstrual cycle
// tracking entered and read on the watch, locked by an optional PIN. Its parts, one over the
// other: the spiral, the calendar, the history rows, the Moon rows, the pregnancy estimate, the
// hayız rulings and the page's settings. Each part is one drawn object (the internal heap stays
// free for the portal's Wi-Fi); taps are told apart by where they land. The rules live in
// Cycle and Hayiz (host-tested); the log is the main task's, used here with the display lock
// held. CycleParts.cpp draws the parts.
class CyclePages
{
public:
    enum Part
    {
        SpiralPart,
        CalendarPart,
        HistoryPart,
        MoonPart,
        PregnancyPart,
        HayizPart,
        SettingsPart,
        PartCount  // numbered in the saved orders (PartOrder): new ones at the end
    };

    // `onChange` with `user` after each saved change (the face's gauge and icon follow).
    void build( lv_obj_t *tile, CycleLog &log, void ( *onChange )( void *user ), void *user );
    void forget();
    StackPage &stack();

    // Each second: today's local day and whether the time is valid; recomputes at a new day.
    // `night`: the red night mode (estimated drops are drawn half filled then).
    void update( int today, bool timeValid, bool night );

    // The log changed from outside (the development scene): derive and draw again.
    void reload();

    // A Ramadan that ended (found by the main task, Hijri's months cost seconds): its hayız days
    // are added to the make-up balance once a Hijri year.
    void addRamadan( int year, int firstDay, int length );

    // Active mode ended: the PIN is asked again next time.
    void lock();
    // No PIN, or the right one given since active mode started.
    bool unlocked() const;

    // Whether the Hayız part is shown (the entry's make-up fast key goes with it).
    void setHayizShown( bool shown );

    // The calendar steps its month with a sideways swipe: the tiles stay then.
    bool swipesSideways() const;

    // Development: "entry <flow> <intercourse> [<madeup>]" saves today's entry; "pin <digits>"
    // types and confirms a PIN; "teen" steps the adolescent mode; "legend"; "month <step>";
    // "open-entry", "open-pin" and "close" for the overlays' pictures.
    bool press( const char *name );

private:
    enum Overlay
    {
        NoOverlay,
        PinOverlay,     // the keypad for the PIN, or a number on the settings part
        EntryOverlay,   // a day's bleeding, intercourse and make-up fast
        LegendOverlay,  // the symbols' meanings
        NormalOverlay   // what is normal, for a few seconds
    };
    enum Input
    {
        CheckPin,     // unlock
        NewPin,       // the first of two
        RepeatPin,    // the second
        OldPinOff,    // the old PIN, then the PIN is removed
        OldPinChange, // the old PIN, then a new one
        FirstPeriod,  // year.month of the first period
        EarlierFasts  // the make-up fasts owed from before
    };

    static void onDraw( lv_event_t *event );
    static void onTouch( lv_event_t *event );
    static void onGesture( lv_event_t *event );
    static void onOverlayDraw( lv_event_t *event );
    static void onOverlayTouch( lv_event_t *event );
    static void onKey( void *user, Calculator::Key key );

    // CycleParts.cpp: one per part, into `layer` of the part's object at `area`.
    void drawSpiral( lv_layer_t *layer, const lv_area_t &area ) const;
    void drawCalendar( lv_layer_t *layer, const lv_area_t &area ) const;
    void drawHistory( lv_layer_t *layer, const lv_area_t &area ) const;
    void drawMoon( lv_layer_t *layer, const lv_area_t &area ) const;
    void drawPregnancy( lv_layer_t *layer, const lv_area_t &area ) const;
    void drawHayiz( lv_layer_t *layer, const lv_area_t &area ) const;
    void drawSettings( lv_layer_t *layer, const lv_area_t &area ) const;
    void drawEntry( lv_layer_t *layer, const lv_area_t &area ) const;
    void drawLegend( lv_layer_t *layer, const lv_area_t &area ) const;
    void drawNormal( lv_layer_t *layer, const lv_area_t &area ) const;
    void drawPinHeader( lv_layer_t *layer, const lv_area_t &area ) const;
    // The spiral's day `t` (days from today): its box's middle on the arm and its angle.
    void spiralPoint( int t, float offset, int &x, int &y, float &angle ) const;
    // The calendar's day under a screen point, 0: none.
    int calendarDayAt( const lv_point_t &point ) const;

    void tapPart( int part, const lv_point_t &point );
    void tapOverlay( const lv_point_t &point );
    void openOverlay( Overlay overlay );
    void closeOverlay();
    void openInput( Input input );
    void openEntry( int day );
    void submitInput();
    void save();
    void refresh();
    void recompute();

    uint32_t hash( const char *digits ) const;
    bool interactive() const;  // the time valid and not before the log's newest day

    StackPage m_stack;
    lv_obj_t *m_tile = nullptr;
    lv_obj_t *m_objects[PartCount] = {};
    lv_obj_t *m_overlay = nullptr;
    lv_obj_t *m_pinHeader = nullptr;
    Keypad m_keypad;
    Calculator m_calculator;
    CycleLog *m_log = nullptr;
    void ( *m_onChange )( void *user ) = nullptr;
    void *m_changeUser = nullptr;
    Cycle::State m_state = {};
    int m_today = 0;
    bool m_timeValid = false;
    bool m_night = false;
    bool m_unlocked = false;
    bool m_hayizShown = false;
    Overlay m_open = NoOverlay;
    Input m_input = CheckPin;
    char m_typed[12] = {};
    uint32_t m_firstPin = 0;     // the new PIN's hash while it is repeated
    int m_wrongPins = 0;
    int64_t m_waitUntilUs = 0;   // after three wrong PINs
    int64_t m_normalUntilUs = 0;
    int64_t m_deleteArmedUs = 0;
    int m_entryDay = 0;
    uint8_t m_entryValue = 0;
    int m_monthStep = 0;         // the calendar's month, from today's
    double m_moonAge = 0;        // today's Moon age in days (the spiral's turn)
    lv_point_t m_pressPoint = {};
};
