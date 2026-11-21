#pragma once

#include "lvgl.h"

// The marks of a page that steps through its contents with a vertical swipe (information parts,
// always-on styles, face layouts): one short bar per item along the right edge, in line with the
// corner gauges and clear of them, the current one light. `horizontal` puts them along the bottom
// edge instead, for the pages swiped sideways. Called with the display lock held.
class PageMarks
{
public:
    static const int MaxCount = 8;

    void build( lv_obj_t *parent, int count, bool horizontal = false );

    // A new number of items (the parts shown, set in the portal); the marks are laid out again.
    void setCount( int count );

    void show( int index );

    // Shows the marks for a moment after a step, then hides them again (the watch face).
    void flash( int index );

    void setVisible( bool visible );

    // The marks were deleted with their page.
    void forget();

    // A scroll position instead of items (the launcher's list), in the same place and colours:
    // a dim track over the marks' span and a light part for the share of the list on screen.
    // `top`: the list's scroll position, `visible`: its height, `total`: its content's height.
    void buildScroll( lv_obj_t *parent );
    void showScroll( int top, int visible, int total );

private:
    static void onHide( lv_timer_t *timer );

    lv_obj_t *m_marks[MaxCount] = {};
    int m_count = 0;
    bool m_horizontal = false;
    bool m_visible = true;
    lv_timer_t *m_hideTimer = nullptr;
};
