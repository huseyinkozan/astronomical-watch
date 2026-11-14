#pragma once

#include "Rtc.h"
#include "Sky.h"
#include "lvgl.h"

// The watch face of active mode in one of four layouts, chosen by a vertical swipe (Display) or
// in the setup portal: 0 classic, 1 day arc, 2 sky arcs (the Sun's and the Moon's paths over the
// horizon), 3 dial (an analog clock between the Sun's and the Moon's orbits). Each layout shows
// the items
// that Settings::FaceItems selects. Called with the display lock held.
class WatchFace
{
public:
    // Bits of Settings::FaceItems.
    enum Item
    {
        DateItem = 1,     // day name, date and Hijri date
        SunItem = 2,      // the next sunrise or sunset
        MoonItem = 4      // the Moon's phase
    };

    static const int StyleCount = 4;

    void build( lv_obj_t *page );

    // `direction`: 1 after a swipe up (the new layout comes in from below), -1 after a swipe down,
    // 0 at once.
    void setStyle( int style, int items, int direction = 0 );

    void showTime( const Rtc::DateTime &localTime );

    void showSky( const Sky::Day &day, const Sky::Now &now, int utcOffsetMinutes );

private:
    // The objects of one layout; null where the layout has none.
    struct Layout
    {
        lv_obj_t *root;
        lv_obj_t *time;     // HH:MM
        lv_obj_t *seconds;
        lv_obj_t *date;
        lv_obj_t *hijri;
        lv_obj_t *sunName;
        lv_obj_t *sunTime;
        lv_obj_t *sunLeft;
        lv_obj_t *arc;
        lv_obj_t *horizon;
        lv_obj_t *sunDot;
        lv_obj_t *riseText;
        lv_obj_t *setText;
        lv_obj_t *dayName;
        lv_obj_t *moonArc;
        lv_obj_t *skyMoon;  // the Moon on its path or orbit
        lv_obj_t *dial;     // the dial's orbits, ticks and hands (onDrawDial)
    };

    // What the dial draws: the orbits' rise and set angles (degrees, clockwise from 3 o'clock),
    // the bodies' angles, and the time.
    struct Dial
    {
        bool sunKnown;
        bool sunUp;
        float sunRise;
        float sunSet;
        float sunAngle;
        bool moonKnown;
        bool moonUp;
        float moonRise;
        float moonSet;
        float moonAngle;
        int hour;
        int minute;
        int second;
    };

    void buildClassic( lv_obj_t *page );
    void buildArc( lv_obj_t *page );
    void buildSkyArcs( lv_obj_t *page );
    void buildDial( lv_obj_t *page );
    static void onDrawDial( lv_event_t *event );
    void placeSky( const Sky::Day &day, int64_t time );
    void applyItems();

    Layout m_layouts[StyleCount] = {};
    int m_style = 0;
    int m_items = 0;
    // Shared by the layouts: moved to the one shown.
    lv_obj_t *m_moon = nullptr;
    int m_moonPercent = -1;
    int m_skyMoonState[2] = { -1, -1 };  // percent and up of each sky moon as last drawn
    lv_point_precise_t m_horizonPoints[2] = {};
    lv_point_precise_t m_skyHorizonPoints[2] = {};
    Dial m_dial = {};
};
