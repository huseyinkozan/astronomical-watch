#pragma once

#include "PageMarks.h"
#include "lvgl.h"

// A page of the tileview whose parts lie one over the other: a vertical swipe steps between them
// (up: the next one slides in from below; after the last comes the first), and marks along the
// right edge show which one is shown. The clock, information, tools, planets and settings pages use it. Their
// name stays at the top between the two upper corner gauges, over a line whose ends bend up to
// the gauges, so that every part shows which page it belongs to. A part change sends
// LV_EVENT_VALUE_CHANGED to the tile. Called with the display lock held.
class StackPage
{
public:
    static const int MaxParts = PageMarks::MaxCount;
    // The parts' contents start this much lower than without the page name (their top padding).
    static const int HeaderPad = 12;

    void build( lv_obj_t *tile, int count, const char *title );

    // The container of part `index`, full screen, for its contents.
    lv_obj_t *part( int index ) const;

    // `direction`: 1 after a swipe up (the part comes in from below), -1 after a swipe down, 0 at once.
    // `index` is a part number; a part left out of the order is shown too (screenshots).
    void show( int index, int direction );

    // The parts to show and their order (PartOrder::decode); all of them by default. The part
    // shown moves to the first of them when it is left out.
    void setOrder( const int *parts, int count );

    int current() const;

    // Shows the part `steps` places on in the order, as a swipe does (the deep-sky list's swipe
    // past its end).
    void step( int steps );

    // Shows `part` again after the page was built anew, or the first part shown when `part` has
    // been hidden meanwhile.
    void restore( int part );

    // The parts were deleted with the tile's contents (a page freed far from the shown one): the
    // tile's swipe handler goes, the order and the part shown stay for the next build.
    void forget();

    // While locked a vertical swipe steps no part (the torch's brightness drag).
    void setSwipeLocked( bool locked );

    // Hides the page name and its line (the torch fills the screen).
    void setHeaderHidden( bool hidden );

    // The page name and the line under it on `tile` (also the launcher's); `line` and `name`
    // receive the objects when given.
    static void buildHeader( lv_obj_t *tile, const char *title, lv_obj_t **line = nullptr, lv_obj_t **name = nullptr );

private:
    static void onGesture( lv_event_t *event );
    static void onDrawHeaderLine( lv_event_t *event );

    lv_obj_t *m_tile = nullptr;
    lv_obj_t *m_parts[MaxParts] = {};
    int m_count = 0;
    int m_order[MaxParts] = {};
    int m_shown = 0;
    int m_current = 0;
    bool m_swipeLocked = false;
    lv_obj_t *m_headerLine = nullptr;
    lv_obj_t *m_headerName = nullptr;
    PageMarks m_marks;
};
