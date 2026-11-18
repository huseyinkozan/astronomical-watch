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
                lv_obj_t *compassPage, lv_obj_t *timePage );

    // English name of a Hijri month, 1 to 12.
    static const char *hijriMonthName( int month );

    // The name or coordinates of the location, on the pages that depend on it.
    void setPlace( const char *place );

    // `day` is computed once for the local day, `now` every second.
    void show( const Sky::Day &day, const Sky::Now &now, int utcOffsetMinutes );

    // The parts were deleted with their page; show() does nothing until the next build.
    void forget();

private:
    static const int SunRows = 6;
    static const int TwilightRows = 8;
    static const int MoonRows = 7;
    static const int TimeRows = 8;

    TablePart m_sun;
    TablePart m_twilight;
    TablePart m_prayer;
    TablePart m_moon;
    TablePart m_time;
    bool m_built = false;
    char m_place[32] = {};
    lv_obj_t *m_compassArrow = nullptr;
    lv_obj_t *m_compassNorth = nullptr;
    lv_obj_t *m_compassText = nullptr;
    lv_point_precise_t m_arrowPoints[5] = {};
};
