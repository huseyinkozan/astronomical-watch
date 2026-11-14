#pragma once

#include "Corners.h"
#include "lvgl.h"

// The corner gauges of active mode (Corners), drawn on LVGL's top layer so that every page shows
// them: a channel in a dim tone of the gauge's own colour, filled by the value, with notches at
// both ends and in the middle as long as the gauge is wide. On the always-on page they take the
// always-on look: thinner, without the channel, in the colours of idle mode. Called with the
// display lock held.
class CornerView
{
public:
    void build();

    void show( const Corners::Gauge gauges[Corners::CornerCount] );

    void setAlwaysOnLook( bool alwaysOn );

private:
    void apply( int corner );
    void placeNotches( int corner );

    lv_obj_t *m_arcs[Corners::CornerCount] = {};
    // Three notches per corner: lower end, middle, upper end.
    lv_obj_t *m_notches[Corners::CornerCount][3] = {};
    lv_point_precise_t m_notchPoints[Corners::CornerCount][3][2] = {};
    Corners::Gauge m_gauges[Corners::CornerCount] = {};
    bool m_alwaysOn = false;
};
