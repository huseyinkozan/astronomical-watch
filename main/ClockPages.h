#pragma once

#include <atomic>
#include <cstdint>

#include "DayCounters.h"
#include "Hijri.h"
#include "Rtc.h"
#include "StackPage.h"
#include "Timers.h"
#include "AlarmStore.h"
#include "WorldClock.h"
#include "lvgl.h"

// The clock page, right of the watch face: world clock, stopwatch, countdown, and the Gregorian
// and Hijri month calendars, one over the other (StackPage). On a calendar a sideways swipe shows
// the month before or after, so the pages do not move then (swipesSideways()); "<" and ">" at the
// bottom say so. Display builds it and calls it with the display lock
// held; the Hijri months are computed by the main task (up to 0.5 s each with the local crescent
// method), which takes the request and hands the month back.
class ClockPages
{
public:
    enum Part
    {
        WorldPart,
        StopwatchPart,
        TimerPart,
        GregorianPart,
        HijriPart,
        DaysPart,
        AlarmsPart,  // last: the parts are numbered in the saved orders (PartOrder)
        PartCount
    };

    void build( lv_obj_t *tile );

    StackPage &stack();

    // The part shown takes sideways swipes itself (a calendar): Display stops the page swipe.
    bool swipesSideways() const;

    // Every second: the time (Unix seconds), the UTC offset and the local date and time.
    void update( int64_t utcSeconds, int utcOffsetMinutes, const Rtc::DateTime &localTime );

    // A Hijri month the calendar asked for: with `step` 0 the month that holds local day
    // `fromDay`; with 1 the month after the one that starts on `fromDay` and lasts `length` days,
    // with -1 the month before it. Taken once; false when nothing is waiting.
    struct HijriRequest
    {
        int fromDay;
        int length;
        int step;
    };
    bool takeHijriRequest( HijriRequest &request );

    // The month for the Hijri calendar; `today`: the local day number; `method`: Hijri::Method.
    void showHijriMonth( const Hijri::Month &month, int today, int method );

    // The world clock's cities and the day counters (copied).
    void setCities( const WorldClock &worldClock );
    void setCounters( const DayCounters &counters );

    // The fixed scene of the screenshots: sample stopwatch and countdown states.
    void setScene( bool scene );

    // The scene's world clock cities (offsets on its 21 June) and day counters (days from 21 June
    // 2026, names by Language::Id), also for the face slots.
    static const char *const SceneCityNames[WorldClock::ShownCities];
    static const int SceneCityOffsets[WorldClock::ShownCities];
    static const char *const SceneCounterNames[2][4];
    static const int SceneCounterDays[4];

    // Development (host command "B <name>"): presses a button or tap area as a finger would:
    // lap, start (stopwatch), +1, +5, +10, reset, go (countdown); g<, g>, h<, h> step the
    // calendars as a sideways swipe does.
    bool press( const char *name );

    // The alarms part: the definitions and their next due times (Unix s, 0: none), and a row
    // tapped since the last call (its alarm to switch on or off).
    void setAlarms( const AlarmStore::List &alarms, const int64_t *due );
    bool takeAlarmToggle( int &index );

    // The alarms: the countdown's end while it runs (Unix ms; 0: stopped), whether the stopwatch
    // runs, and the countdown stopped at 00:00 after it rang.
    int64_t countdownEndMs() const;
    bool stopwatchRunning() const;
    void finishCountdown();

private:
    static void onTick( lv_timer_t *timer );
    static void onStopwatchButton( lv_event_t *event );
    static void onTimerButton( lv_event_t *event );
    static void onGesture( lv_event_t *event );
    static void onAlarmRow( lv_event_t *event );
    void showAlarms();
    void stepGregorian( int step );
    void stepHijri( int step );

    // A calendar: title, footer, and one object that draws the weekday row, up to six weeks of
    // days, a ring around today and one around the chosen day (labels for each day took 20 KB of
    // the heap, which the portal's Wi-Fi needs). A tap on a day chooses it on both calendars.
    struct Calendar
    {
        lv_obj_t *part;
        lv_obj_t *title;
        lv_obj_t *grid;
        lv_obj_t *footer;
        int firstWeekday;  // 0 Monday
        int length;        // 0: nothing to draw yet
        int today;         // the day of the month to ring, 0 for none
        int chosen;        // the chosen day of the month, 0 for none
    };
    static void onDrawCalendar( lv_event_t *event );
    static void onCalendarTouch( lv_event_t *event );
    void buildCalendar( Calendar &calendar, lv_obj_t *part );
    // `firstWeekday`: 0 Monday; `today` and `chosen`: days of the month, 0 for none.
    void fillCalendar( Calendar &calendar, int firstWeekday, int length, int today, int chosen );
    // The day of the month at a point on the screen, 0 for none.
    static int dayAt( const Calendar &calendar, const lv_point_t &point );
    // Chooses `day` (days since 1970-01-01, local) on both calendars.
    void choose( int day );
    // The day whose month the calendars show when not stepped: the chosen one, else today.
    int anchorDay() const;
    void showGregorian();
    void showHijriMarks();
    void showStopwatch( int64_t nowMs );
    void showCountdown( int64_t nowMs );
    int64_t nowMs() const;
    // "hh:mm" of a Unix time in milliseconds, in local time.
    void formatLocal( char *text, int size, int64_t unixMs ) const;

    StackPage m_stack;
    Timers m_timers;
    bool m_scene = false;
    int m_offsetMinutes = 0;
    Rtc::DateTime m_local = {};
    // Unix milliseconds at esp_timer time m_baseUs (the RTC has whole seconds only).
    int64_t m_baseMs = 0;
    int64_t m_baseUs = 0;

    // World clock: our own time first, then the cities from the setup portal.
    void showCities( int64_t utcSeconds );
    lv_obj_t *m_homeTime = nullptr;
    lv_obj_t *m_homeDate = nullptr;
    lv_obj_t *m_homeZone = nullptr;
    lv_obj_t *m_cityHint = nullptr;
    lv_obj_t *m_cityNames[WorldClock::ShownCities] = {};
    lv_obj_t *m_cityDiffs[WorldClock::ShownCities] = {};
    lv_obj_t *m_cityTimes[WorldClock::ShownCities] = {};
    WorldClock::City m_cities[WorldClock::MaxCities] = {};
    int m_cityCount = 0;

    lv_obj_t *m_stopwatchTime = nullptr;
    lv_obj_t *m_stopwatchSince = nullptr;
    lv_obj_t *m_lapNames[Timers::LapCount] = {};
    lv_obj_t *m_lapTimes[Timers::LapCount] = {};
    lv_obj_t *m_stopwatchLeft = nullptr;   // Lap / Reset
    lv_obj_t *m_stopwatchRight = nullptr;  // Start / Stop

    lv_obj_t *m_timerTime = nullptr;
    lv_obj_t *m_timerInfo = nullptr;
    lv_obj_t *m_timerButtons[5] = {};  // +1, +5, +10, Reset, Start / Pause

    // Day counters.
    void showCounters();
    lv_obj_t *m_counterHint = nullptr;
    lv_obj_t *m_counterNames[DayCounters::MaxCounters] = {};
    lv_obj_t *m_counterValues[DayCounters::MaxCounters] = {};
    DayCounters::Counter m_counters[DayCounters::MaxCounters] = {};
    int m_counterCount = 0;
    lv_obj_t *m_alarmNames[Alarms::MaxAlarms] = {};
    lv_obj_t *m_alarmNext[Alarms::MaxAlarms] = {};
    lv_obj_t *m_alarmStates[Alarms::MaxAlarms] = {};
    lv_obj_t *m_alarmRows[Alarms::MaxAlarms] = {};
    lv_obj_t *m_alarmHint = nullptr;
    AlarmStore::List m_alarms = {};
    int64_t m_alarmDue[Alarms::MaxAlarms] = {};
    volatile int m_alarmToggle = -1;

    Calendar m_gregorian = {};
    Calendar m_hijri = {};
    lv_point_t m_pressPoint = {};
    int m_gregorianStep = 0;  // months from the anchor day's
    int m_gregorianShown[4] = {};  // year, month, today, chosen day: redraw when one changes
    int m_hijriStep = 0;
    int m_today = 0;         // today's local day number (days since 1970-01-01)
    int m_chosenDay = -1;    // the day chosen on a calendar, -1: none (each wake starts without)
    int m_hijriAnchor = -1;  // the anchor day whose Hijri month was asked for, -1: none yet
    Hijri::Month m_hijriMonth = {};
    std::atomic<bool> m_hijriWanted { false };
    HijriRequest m_hijriRequest = {};
};
