#pragma once

#include "Sky.h"
#include "TablePart.h"
#include "lvgl.h"

// The astronomy pages of active mode (M10a, plain text; the designs follow in M10b): Sun,
// twilight, Moon and the sun compass. Display builds them in its tiles and calls show() with
// the display lock held.
class SkyPages
{
public:
    void build( lv_obj_t *sunPage, lv_obj_t *twilightPage, lv_obj_t *prayerPage, lv_obj_t *moonPage,
                lv_obj_t *compassPage, lv_obj_t *timePage, lv_obj_t *tidePage );

    // English name of a Hijri month, 1 to 12.
    static const char *hijriMonthName( int month );

    // The name or coordinates of the location, on the pages that depend on it.
    void setPlace( const char *place );

    // `day` is computed once for the local day, `now` every second.
    void show( const Sky::Day &day, const Sky::Now &now, int utcOffsetMinutes );

    // The tide part (Tide, docs/superpowers/specs/2026-11-23-tide-design.md, D): the local day's
    // curve, its high and low waters, the state now, the Moon's strength and the interval; a
    // message alone without the interval (Tide::Off).
    void showTide( const Sky::Day &day, int intervalMinutes, int64_t now, int utcOffsetMinutes );

    // The parts were deleted with their page; show() does nothing until the next build.
    void forget();

private:
    static const int SunRows = 7;
    static const int TwilightRows = 8;
    static const int MoonRows = 7;
    static const int TimeRows = 8;

    TablePart m_sun;
    TablePart m_twilight;
    TablePart m_prayer;
    TablePart m_moon;
    TablePart m_time;
    TablePart m_tide;
    // The tide's curve over the local day, a sample every 15 min (Tide::at's level and direction),
    // computed again for a new day or interval; the moment's place along it.
    static const int TideSamples = 97;
    static void onDrawTideCurve( lv_event_t *event );
    lv_obj_t *m_tideCurve = nullptr;
    lv_obj_t *m_tideMessage = nullptr;
    int16_t m_tideLevels[TideSamples] = {};
    bool m_tideRising[TideSamples] = {};
    int m_tideKey = 0;
    int m_tideNow = -1;  // thousandths of the day, -1: not today
    bool m_built = false;
    char m_place[32] = {};
    lv_obj_t *m_compassArrow = nullptr;
    lv_obj_t *m_compassNorth = nullptr;
    lv_obj_t *m_compassText = nullptr;
    lv_point_precise_t m_arrowPoints[5] = {};
};
