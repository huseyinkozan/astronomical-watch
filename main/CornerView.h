#pragma once

#include <cstdint>

#include "CornerTable.h"
#include "Corners.h"
#include "lvgl.h"

// The corner gauges of active mode (Corners), on every page: a channel in a dim tone of the
// gauge's own colour with round ends, filled by the value, and a notch in the middle as long as
// the gauge is wide. While the battery charges, a white part grows step by step from its value to
// the full end and starts again.
//
// Not LVGL objects: four arcs with round ends cost about a quarter of every frame while the pages
// were swiped (docs/hardware.md). Instead paint() writes the ring's pixels into each band LVGL
// has rendered, before it goes to the panel, their colours computed from the ring's table in
// flash (CornerTable.h) and the gauge's value. The content keeps clear of the ring (the visible
// area, CLAUDE.md), so nothing is under it. Called with the display lock held (paint() from the
// LVGL task, which holds it).
class CornerView
{
public:
    // Once LVGL runs, before the gauges are drawn; starts the charging animation's timer.
    void build();

    void show( const Corners::Gauge gauges[Corners::CornerCount] );

    // From any task, without the display lock: the display timeout's share left (thousandths).
    // Its gauges follow it ten times a second and grow back in steps after a touch.
    static void setTimeoutLeft( int left );

    // From any task: the cycle gauge's phases (its mean length, bleeding days and fertile window
    // in cycle days) and whether to colour them (the cycle page unlocked); neutral grey otherwise.
    static void setCycle( int length, int period, int fertileFirst, int fertileLast, bool colour );

    // Writes the gauges into a rendered band: `pixels` hold `area` row after row, in LVGL's
    // RGB565 (before the flush swaps the bytes).
    void paint( const lv_area_t &area, uint16_t *pixels ) const;

private:
    // What paint() needs of a gauge in the active look, worked out when it changes.
    struct Look
    {
        float channel[3];  // red, green, blue, 0..255
        float fill[3];
        bool empty;
        int start;  // along the gauge (CORNER_ALONG_SCALE)
        int end;
        bool charging;
    };

    void updateLook( int corner );
    void invalidate( int corner );
    uint16_t colour( int corner, const CornerPixel &pixel, int along ) const;
    static void onChargeStep( lv_timer_t *timer );
    static void onTimeoutStep( lv_timer_t *timer );

    Corners::Gauge m_gauges[Corners::CornerCount] = {};
    Look m_looks[Corners::CornerCount] = {};
    bool m_built = false;
    // The charging animation's step: 0 shows the value alone (CornerView.cpp).
    int m_chargeStep = 0;
    uint32_t m_cycleKey = 0;  // the cycle colours last drawn (CornerView.cpp)
};
