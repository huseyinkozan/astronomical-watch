#pragma once

#include "AlwaysOn.h"
#include "Rtc.h"
#include "lvgl.h"

// The always-on page of active mode, left of the watch face: the always-on face as it will look
// (at full brightness; the watch dims it), from the same layout, bitmaps and shapes
// (AlwaysOn::layout, the LVGL fonts of aodStyles, LVGL's drawing for the shapes). Display swaps
// the style on a vertical swipe. Called with the display lock held.
class AodPage
{
public:
    void build( lv_obj_t *page );

    // Draws `look` for the local time (`utcNow` the same moment as Unix time; the corner gauges
    // are drawn by CornerView). `direction`: 1 after a swipe up (the face slides in from below),
    // -1 after a swipe down.
    void show( const AlwaysOn::Look &look, const Rtc::DateTime &localTime, int64_t utcNow, int direction = 0 );

private:
    lv_obj_t *m_page = nullptr;
    lv_obj_t *m_content = nullptr;  // holds the labels and the moon, for the slide
    lv_obj_t *m_labels[AlwaysOn::MaxSprites] = {};
    lv_obj_t *m_moon = nullptr;     // in the corner (digit styles)
    lv_obj_t *m_skyMoon = nullptr;  // on its path (sky styles)
    // The shapes of the sky styles, drawn by onDrawShapes.
    static void onDrawShapes( lv_event_t *event );
    lv_obj_t *m_shapesObject = nullptr;
    AlwaysOn::Shape m_shapes[AlwaysOn::MaxShapes] = {};
    int m_shapeCount = 0;
    int m_faceX = 0;
    int m_faceY = 0;
};
