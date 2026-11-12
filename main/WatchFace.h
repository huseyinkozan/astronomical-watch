#pragma once

#include "Rtc.h"
#include "Sky.h"
#include "lvgl.h"

// The watch face of active mode in one of three layouts, chosen by a vertical swipe (Display) or
// in the setup portal: 0 classic, 1 day arc, 2 hours over minutes. Each layout shows the items
// that Settings::FaceItems selects. Called with the display lock held.
class WatchFace
{
public:
    // Bits of Settings::FaceItems.
    enum Item
    {
        DateItem = 1,     // day name, date and Hijri date
        SunItem = 2,      // the next sunrise or sunset
        MoonItem = 4,     // the Moon's phase
        BatteryItem = 8   // battery percentage
    };

    static const int StyleCount = 3;

    void build( lv_obj_t *page );

    // `direction`: 1 after a swipe up (the new layout comes in from below), -1 after a swipe down,
    // 0 at once.
    void setStyle( int style, int items, int direction = 0 );

    void showTime( const Rtc::DateTime &localTime, int batteryPercent, bool usb );

    void showSky( const Sky::Day &day, const Sky::Now &now, int utcOffsetMinutes );

private:
    // The objects of one layout; null where the layout has none.
    struct Layout
    {
        lv_obj_t *root;
        lv_obj_t *time;     // HH:MM, or HH over MM: the hours
        lv_obj_t *minutes;  // HH over MM: the minutes
        lv_obj_t *seconds;
        lv_obj_t *date;
        lv_obj_t *hijri;
        lv_obj_t *battery;
        lv_obj_t *moonText;
        lv_obj_t *sunName;
        lv_obj_t *sunTime;
        lv_obj_t *sunLeft;
        lv_obj_t *arc;
        lv_obj_t *horizon;
        lv_obj_t *sunDot;
        lv_obj_t *riseText;
        lv_obj_t *setText;
        int moonX;
        int moonY;
    };

    void buildClassic( lv_obj_t *page );
    void buildArc( lv_obj_t *page );
    void buildStacked( lv_obj_t *page );
    void drawMoon( int percent, bool waxing );
    void applyItems();

    Layout m_layouts[StyleCount] = {};
    int m_style = 0;
    int m_items = 0;
    // Shared by the layouts: moved to the one shown.
    lv_obj_t *m_moon = nullptr;
    int m_moonPercent = -1;
    lv_point_precise_t m_horizonPoints[2] = {};
};
