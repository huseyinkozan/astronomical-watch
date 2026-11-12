#pragma once

#include "AlwaysOn.h"
#include "Rtc.h"
#include "lvgl.h"

// The always-on page of active mode, left of the watch face: the always-on face as it will look
// (at full brightness; the watch dims it), from the same layout and bitmaps (AlwaysOn::layout and
// the LVGL fonts of aodStyles). Display swaps the style on a vertical swipe. Called with the
// display lock held.
class AodPage
{
public:
    void build( lv_obj_t *page );

    // Draws `look` for the local time; the battery mark is shown as when the battery is low.
    // `direction`: 1 after a swipe up (the face slides in from below), -1 after a swipe down.
    void show( const AlwaysOn::Look &look, const Rtc::DateTime &localTime, int direction = 0 );

private:
    lv_obj_t *m_page = nullptr;
    lv_obj_t *m_content = nullptr;  // holds the labels and the moon, for the slide
    lv_obj_t *m_labels[AlwaysOn::MaxSprites] = {};
    lv_obj_t *m_moon = nullptr;
};
