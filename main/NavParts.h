#pragma once

#include <cstdint>

#include "Astronomy.h"
#include "Settings.h"
#include "TablePart.h"
#include "lvgl.h"

// Two parts of the observing page (ObservingPages): the nautical almanac and aiming, hidden by
// default (the portal shows them). Called with the display lock held.
//
// - Nautical almanac: one body at a time (a tap steps the Sun, the Moon, Venus, Mars, Jupiter,
//   Saturn): its GHA and declination at the UTC second (Almanac), the Sun's semi-diameter, local
//   apparent noon and noon altitude, the Moon's semi-diameter, horizontal parallax and meridian
//   passage, a planet's SHA and meridian passage. Updated each second while shown.
// - Aiming, in two modes stepped by a tap on the title: a satellite dish (a tap on the satellite
//   steps the presets, the portal's own longitude first when it is not one of them): elevation,
//   true azimuth, the magnetic variation (Magnetic, WMM2025), the compass bearing and the time
//   the Sun stands in that direction today; a solar panel facing the equator: its tilt for the
//   year and for today, its azimuth and compass bearing. Computed once a minute while shown.
// The choices are kept in RTC memory.
class NavParts
{
public:
    void build( lv_obj_t *almanac, lv_obj_t *aiming, Settings &settings );
    void forget();

    // Which part shows: 0 the almanac, 1 aiming, -1 neither.
    void setShown( int part );

    // Every update of the sky.
    void show( int64_t now, const Astronomy::Observer &observer, int utcOffsetMinutes, bool located );

    // Development (host command "B <name>"): body (the next one), aim (dish or panel), satellite.
    bool press( const char *name );

private:
    static void onTapEvent( lv_event_t *event );
    void showAlmanac();
    void showAiming();
    // The satellites to choose from: the presets, and the portal's longitude first when it is
    // not one of them.
    int satelliteCount() const;
    // The satellite of the choice (east positive), its name ("Türksat 42.0° E") into `name`.
    double satellite( char *name, size_t size ) const;
    // Local "12:59:41" or "12:59" of a Unix time.
    void localTime( int64_t time, bool seconds, char *text, size_t size ) const;
    int64_t localMidnight() const;

    Settings *m_settings = nullptr;
    TablePart m_almanacTable;
    TablePart m_aimingTable;
    lv_obj_t *m_bodyTap = nullptr;
    lv_obj_t *m_modeTap = nullptr;
    lv_obj_t *m_satelliteTap = nullptr;
    int m_shown = -1;
    int64_t m_now = 0;
    Astronomy::Observer m_observer = { 0, 0 };
    int m_utcOffsetMinutes = 0;
    bool m_located = false;
    int64_t m_aimingMinute = -1;
    // The variation and the Sun's time in the dish's direction for one place, day and satellite.
    double m_variation = 0;
    int64_t m_sunTime = 0;
    bool m_sunInLine = false;
    int64_t m_aimingDay = 0;
    double m_aimingSatellite = -999;
    Astronomy::Observer m_aimingObserver = { -999, -999 };
};
