#pragma once

#include <cstdint>

#include "Astronomy.h"
#include "Messier.h"
#include "Observing.h"
#include "StackPage.h"
#include "lvgl.h"

// Three parts of the observing page (ObservingPages): polar alignment, the Milky Way window and
// the deep-sky finder. Called with the display lock held.
//
// - Polar alignment: the pole star on a polar scope's reticle drawn as a 12-hour clock (Polaris,
//   sigma Octantis south of the equator; Observing), its clock reading and hour angle. Inverted by
//   default as most polar scopes show it; a tap flips it upright and back (kept in RTC memory).
// - Milky Way window: tonight's dark, the galactic centre 10 degrees up and the Moon out of the
//   way on a 16:00 to 08:00 bar, the window's times, the centre's highest, the Moon, and the next
//   night with a window (up to 120 nights ahead). Asked for when the part is shown, a moment after
//   it slid in, once a night and place, and computed in the main task.
// - Deep-sky finder: the Messier objects 10 degrees up now, highest first, each with its altitude
//   and best time in tonight's dark, again each minute while shown. The list scrolls (one of the
//   two vertical scrolls in the pages, with the launcher's): a swipe past either end steps the
//   part, and a list that fits leaves the swipes to the page.
class SkyParts
{
public:
    void build( lv_obj_t *polar, lv_obj_t *milkyWay, lv_obj_t *deepSky, StackPage &stack );
    void forget();

    // Which of the three parts shows: 0 polar, 1 Milky Way, 2 deep sky, -1 none of them.
    void setShown( int part );

    // Every update of the sky; the parts change once a minute.
    void show( int64_t now, const Astronomy::Observer &observer, int utcOffsetMinutes, bool located );

    // The Milky Way window's night is computed in the main task (0.13 s measured, a second or more
    // for the longest searches, which would freeze the LVGL task): the part asks a moment after it slid in, the answer for a
    // night or place no longer shown is dropped.
    static const int SearchNights = 120;
    bool takeMilkyWayRequest( int64_t &noon, Astronomy::Observer &observer );
    void showMilkyWayNight( const Observing::MilkyWayNight &night, bool hasNext, int64_t nextNoon, int64_t noon,
                            const Astronomy::Observer &observer );

    // Development (host command "B <name>"): flip (the reticle), scroll <px> (the list).
    bool press( const char *name );

    enum Shown
    {
        PolarShown,
        MilkyWayShown,
        DeepSkyShown
    };

private:
    static void onPolarTap( lv_event_t *event );
    static void onDrawReticle( lv_event_t *event );
    static void onDrawNightBar( lv_event_t *event );
    static void onDrawList( lv_event_t *event );
    static void onListEvent( lv_event_t *event );
    static void onMilkyWayTimer( lv_timer_t *timer );
    void showPolar();
    void showMilkyWay();
    void showDeepSky();
    // The local noon before tonight: today's, or yesterday's while last night lasts.
    int64_t tonightNoon() const;
    // Minutes since local midnight of a Unix time.
    int localMinute( int64_t time ) const;

    StackPage *m_stack = nullptr;
    int m_shown = -1;
    int64_t m_now = 0;
    Astronomy::Observer m_observer = { 0, 0 };
    int m_utcOffsetMinutes = 0;
    bool m_located = false;
    int64_t m_minute = -1;

    // Polar alignment.
    lv_obj_t *m_reticle = nullptr;
    lv_obj_t *m_poleRows[3] = {};
    double m_poleHourAngle = 0;
    int m_poleMinutes = 0;
    bool m_south = false;

    // Milky Way window: computed for one night and place (m_milkyNoon, m_milkyObserver).
    lv_obj_t *m_milkyTitle = nullptr;
    lv_obj_t *m_nightBar = nullptr;
    lv_obj_t *m_milkyRows[4] = {};
    lv_timer_t *m_milkyTimer = nullptr;
    Observing::MilkyWayNight m_milky = {};
    int64_t m_milkyNoon = 0;
    Astronomy::Observer m_milkyObserver = { 0, 0 };
    bool m_milkyKnown = false;
    bool m_milkyWanted = false;
    bool m_hasNext = false;
    int64_t m_nextNoon = 0;

    // Deep-sky finder: the entries while the page is built.
    lv_obj_t *m_deepTitle = nullptr;
    lv_obj_t *m_deepNone = nullptr;
    lv_obj_t *m_list = nullptr;
    lv_obj_t *m_listContent = nullptr;
    Messier::Entry *m_entries = nullptr;
    int m_entryCount = 0;
    int64_t m_listMinute = -1;
    int m_pressX = 0;
    int m_pressY = 0;
    bool m_pressAtTop = false;
    bool m_pressAtBottom = false;
};
