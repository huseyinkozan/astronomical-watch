#pragma once

#include "PageMarks.h"
#include "lvgl.h"

// A page of the tileview whose parts lie one over the other: a vertical swipe steps between them
// (up: the next one slides in from below; after the last comes the first), and marks along the
// right edge show which one is shown. The information page and the settings page use it. A part
// change sends LV_EVENT_VALUE_CHANGED to the tile. Called with the display lock held.
class StackPage
{
public:
    static const int MaxParts = PageMarks::MaxCount;

    void build( lv_obj_t *tile, int count );

    // The container of part `index`, full screen, for its contents.
    lv_obj_t *part( int index ) const;

    // `direction`: 1 after a swipe up (the part comes in from below), -1 after a swipe down, 0 at once.
    void show( int index, int direction );

    int current() const;

private:
    static void onGesture( lv_event_t *event );

    lv_obj_t *m_tile = nullptr;
    lv_obj_t *m_parts[MaxParts] = {};
    int m_count = 0;
    int m_current = 0;
    PageMarks m_marks;
};
