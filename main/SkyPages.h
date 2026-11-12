#pragma once

#include "Sky.h"
#include "lvgl.h"

// The astronomy pages of active mode (M10a, plain text; the designs follow in M10b): Sun,
// twilight, Moon and the sun compass. Display builds them in its tiles and calls show() with
// the display lock held.
class SkyPages
{
public:
    void build( lv_obj_t *sunPage, lv_obj_t *twilightPage, lv_obj_t *moonPage, lv_obj_t *compassPage );

    // `day` is computed once for the local day, `now` every second.
    void show( const Sky::Day &day, const Sky::Now &now, int utcOffsetMinutes );

private:
    // A table row: the name, then two values in columns (the second is null when the first
    // spans both).
    struct Row
    {
        lv_obj_t *name;
        lv_obj_t *values[2];
    };

    static const int SunRows = 6;
    static const int TwilightRows = 8;
    static const int MoonRows = 7;

    static Row createRow( lv_obj_t *page, const char *name, bool twoValues );

    Row m_sun[SunRows] = {};
    Row m_twilight[TwilightRows] = {};
    lv_obj_t *m_moonPhase = nullptr;
    Row m_moon[MoonRows] = {};
    lv_obj_t *m_hijri = nullptr;
    lv_obj_t *m_compassArrow = nullptr;
    lv_obj_t *m_compassNorth = nullptr;
    lv_obj_t *m_compassText = nullptr;
    lv_point_precise_t m_arrowPoints[5] = {};
};
