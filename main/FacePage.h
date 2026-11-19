#pragma once

#include "Corners.h"
#include "FaceConfig.h"
#include "FaceText.h"
#include "MilkyWay.h"
#include "Sky.h"
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

    // The Moon's illuminated fraction in percent and whether it is waxing, for the moon icons.
    void setMoon( int percent, bool waxing );

    // The arc templates' data: the Sun's and the Moon's rises and sets, this moment's sky (the
    // galactic centre, the Sun's altitude, the hemisphere); `located` false: no location set
    // (no bodies, no band). Once a second; the arcs are placed again when the minute changes.
    void setSky( const Corners::Input &corners, const Sky::Now &now, bool located );

private:
    static void onDrawSegments( lv_event_t *event );
    static void onDrawAnalog( lv_event_t *event );
    static void onDrawArcs( lv_event_t *event );
    void placeSky();
    bool secondsHand() const;
    static void onDrawWeekday( lv_event_t *event );
    void apply();
    void paintMoon();

    lv_obj_t *m_root = nullptr;
    lv_obj_t *m_mono = nullptr;      // HH:MM in JetBrains Mono
    lv_obj_t *m_noTime = nullptr;    // "--:--" in the analog dial while the clock was never set
    lv_obj_t *m_segments = nullptr;  // HH:MM in seven segments (onDrawSegments)
    lv_obj_t *m_analog = nullptr;    // the analog clock (onDrawAnalog)
    lv_obj_t *m_arcs = nullptr;      // the orbits or the paths and the Milky Way (onDrawArcs)
    lv_obj_t *m_skyMoon = nullptr;   // the Moon on its orbit or path
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
    int m_digitHeight = 122;  // the clock's size in this template (FaceConfig::clockPlace)
    int m_analogRadius = 140;
    int m_clockY = 251;
    // The sky, and what placeSky() made of it: the Sun's and the Moon's places on the arcs
    // (Corners::arcPlace) and the Milky Way band.
    Corners::Input m_corners = {};
    Sky::Now m_sky = {};
    bool m_located = false;
    int64_t m_skyMinute = -1;
    Corners::ArcPlace m_bodies[2] = {};  // the Sun, the Moon
    MilkyWay::Band m_band = {};
    MilkyWay::DialBand m_dialBand = {};  // the Full arcs' band
    bool m_bandShown = false;
    bool m_bandBright = false;
    int m_moonPercent = -1;
    bool m_waxing = true;
    bool m_moonPainted = false;
};
