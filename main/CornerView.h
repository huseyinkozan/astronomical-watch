#pragma once

#include <cstdint>

#include "CornerTable.h"
#include "Corners.h"
#include "lvgl.h"

// The corner gauges of active mode (Corners), on every page: a channel in a dim tone of the
// gauge's own colour with round ends, filled by the value, and a notch in the middle as long as
// the gauge is wide. On the always-on page they take the always-on look, pixel for pixel as the
// minute wakes draw it (AlwaysOn::cornerPixel).
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
    // Once LVGL runs, before the gauges are drawn.
    void build();

    void show( const Corners::Gauge gauges[Corners::CornerCount] );

    void setAlwaysOnLook( bool alwaysOn );

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
    };

    void updateLook( int corner );
    void invalidate( int corner );
    uint16_t colour( int corner, const CornerPixel &pixel, int along, int x, int y ) const;

    Corners::Gauge m_gauges[Corners::CornerCount] = {};
    Look m_looks[Corners::CornerCount] = {};
    bool m_alwaysOn = false;
    bool m_built = false;
};
