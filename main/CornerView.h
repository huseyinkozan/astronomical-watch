#pragma once

#include "Corners.h"
#include "lvgl.h"

// The corner gauges of active mode (Corners), drawn on LVGL's top layer so that every page shows
// them. On the always-on page they take the always-on look: thinner, without the track, in the
// colours of idle mode. Called with the display lock held.
class CornerView
{
public:
    void build();

    void show( const Corners::Gauge gauges[Corners::CornerCount] );

    void setAlwaysOnLook( bool alwaysOn );

    // The settings and setup pages hide the gauges: their full-width rows reach into the corners.
    void setHidden( bool hidden );

private:
    void apply( int corner );

    lv_obj_t *m_arcs[Corners::CornerCount] = {};
    Corners::Gauge m_gauges[Corners::CornerCount] = {};
    bool m_alwaysOn = false;
};
