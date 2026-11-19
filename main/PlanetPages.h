#pragma once

#include <cstdint>

#include "Planets.h"
#include "StackPage.h"
#include "TablePart.h"
#include "lvgl.h"

// The planets page (docs/superpowers/specs/2026-11-19-planets-design.md): four parts one over
// the other, the solar system, the sky, the day's rise and set, the distances. The positions
// follow the minute; the day's rise and set come from the main task on request (a computation
// of a few hundred milliseconds). Display calls it with the display lock held.
class PlanetPages
{
public:
    enum Part
    {
        OrbitsPart,
        SkyPart,
        RiseSetPart,
        DistancePart,
        PartCount
    };

    void build( lv_obj_t *tile );

    // The page was freed: its objects are gone, its values stay for the next build.
    void forget();

    StackPage &stack();

    // The moment and the place, every second; the drawings follow each new minute.
    void show( int64_t now, const Astronomy::Observer &observer, int utcOffsetMinutes, bool located );

    // A local day whose rise and set times the page wants (built, located, not yet computed for
    // this day and place). The day's local midnight in Unix seconds.
    bool takeDayRequest( int64_t &localMidnight, Astronomy::Observer &observer );

    void showDay( const Planets::Day &day, int64_t localMidnight, const Astronomy::Observer &observer );

private:
    struct Dot
    {
        int16_t x;
        int16_t y;
        uint8_t radius;
        uint32_t colour;
        bool ring;  // Saturn
    };

    static void onDrawOrbits( lv_event_t *event );
    static void onDrawSky( lv_event_t *event );
    void place();
    void fillRiseSet();

    StackPage m_stack;
    lv_obj_t *m_orbits = nullptr;
    lv_obj_t *m_sky = nullptr;
    TablePart m_riseSet;
    TablePart m_distance;
    bool m_built = false;

    int64_t m_now = 0;
    int64_t m_placedMinute = -1;
    Astronomy::Observer m_observer = {};
    int m_offsetMinutes = 0;
    bool m_located = false;

    // The orbit radii and the bodies' places on the two drawings.
    Dot m_orbitDots[Planets::BodyCount] = {};
    static const int SkyBodies = Planets::BodyCount + 1;  // the seven planets, the Sun, the Moon
    Dot m_skyDots[SkyBodies] = {};

    Planets::Day m_day = {};
    int64_t m_dayMidnight = 0;
    Astronomy::Observer m_dayObserver = {};
    bool m_dayValid = false;
    bool m_dayAsked = false;
};
