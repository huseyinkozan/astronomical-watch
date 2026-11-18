#pragma once

#include "FaceConfig.h"
#include "FaceText.h"
#include "lvgl.h"

// The watch face of active mode: one face page (FaceConfig) on the face tile, its clock and
// the eight slots around it. Called with the display lock held.
class FacePage
{
public:
    void build( lv_obj_t *tile );

    // `direction`: 1 after a swipe up (the new page comes in from below), -1 after a swipe down,
    // 0 at once.
    void show( const FaceConfig::Page &page, int direction = 0 );

    // Once a second.
    void update( const FaceText::Inputs &inputs );

    // The Moon's illuminated fraction in percent and whether it is waxing, for the moon icon.
    void setMoon( int percent, bool waxing );

private:
    lv_obj_t *m_root = nullptr;
    FaceConfig::Page m_page = {};
    int m_moonPercent = -1;
    bool m_waxing = true;
};
