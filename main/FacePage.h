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

    // Once a second. `inputs.table` must stay valid until the next call.
    void update( const FaceText::Inputs &inputs );

    // The Moon's illuminated fraction in percent and whether it is waxing, for the moon icon.
    void setMoon( int percent, bool waxing );

private:
    static void onDrawSegments( lv_event_t *event );
    static void onDrawAnalog( lv_event_t *event );
    bool secondsHand() const;
    static void onDrawWeekday( lv_event_t *event );
    void apply();
    void paintMoon();

    lv_obj_t *m_root = nullptr;
    lv_obj_t *m_mono = nullptr;      // HH:MM in JetBrains Mono
    lv_obj_t *m_segments = nullptr;  // HH:MM in seven segments (onDrawSegments)
    lv_obj_t *m_analog = nullptr;    // the analog clock (onDrawAnalog)
    lv_obj_t *m_texts[FaceConfig::TextSlotCount] = {};
    lv_obj_t *m_cornerTexts[FaceConfig::SlotCount - FaceConfig::TextSlotCount] = {};
    lv_obj_t *m_moon = nullptr;
    lv_obj_t *m_weekday = nullptr;  // the notches (onDrawWeekday) and the letter
    lv_obj_t *m_weekdayLetter = nullptr;
    FaceConfig::Page m_page = {};
    FaceText::Inputs m_inputs = {};
    int m_hour = -1;  // the time the clock objects show
    int m_minute = -1;
    int m_second = -1;
    int m_weekdayShown = -1;  // 0 Monday
    int m_moonPercent = -1;
    bool m_waxing = true;
    bool m_moonPainted = false;
};
