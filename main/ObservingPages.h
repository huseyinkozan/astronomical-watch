#pragma once

#include <cstdint>

#include "Astrolabe.h"
#include "Astronomy.h"
#include "Motion.h"
#include "OpticsParts.h"
#include "Settings.h"
#include "SkyParts.h"
#include "StackPage.h"
#include "Tilt.h"
#include "lvgl.h"

// The observing page (docs/superpowers/plans/2026-11-21-tools.md): its parts one over the other
// (StackPage). Display builds it and calls it with the display lock held. Polar alignment, the
// Milky Way window and the deep-sky finder are SkyParts', the eyepiece and photo calculators
// OpticsParts'; the astrolabe is this class's own.
//
// Astrolabe, in five modes stepped by a tap on the title:
// - dial: the plate for the location (the horizon, almucantars every 15 degrees, the tropics and
//   the equator) under the rete (the ecliptic, the Sun, 20 bright stars; Astrolabe). A sideways
//   drag turns the time (4 min a pixel, up to a day either way; the page swipe stops meanwhile),
//   a tap returns to now.
// - the alidade on the Sun and on Polaris: the top edge's altitude from the accelerometer (Tilt,
//   read 25 times a second only in these modes), on a quadrant; the Sun's gives the local time
//   (the morning or afternoon solution nearest the clock), Polaris' the latitude. A tap holds the
//   angle.
// - a horizontal sundial for the latitude with the shadow of now, the solar and the clock time.
// - the analemma: the Sun's declination against the equation of time over the year, today marked.
class ObservingPages
{
public:
    // A point of a drawing in pixels from its centre.
    struct Pixel
    {
        int16_t x;
        int16_t y;
    };

    enum Part
    {
        AstrolabePart,
        PolarPart,
        MilkyWayPart,
        DeepSkyPart,
        EyepiecePart,
        PhotoPart,
        PartCount  // the parts are numbered in the saved orders (PartOrder): new ones at the end
    };

    // `motion`: the accelerometer of the alidade (nullptr: none).
    void build( lv_obj_t *tile, Settings &settings, Motion *motion );

    // The objects were deleted with the tile's contents; the values stay for the next build.
    void forget();

    StackPage &stack();

    // Display: the part on screen (-1: none); the alidade's modes read the accelerometer while
    // the astrolabe shows, and some parts compute only while they show.
    void setShown( int part );

    // The dial takes sideways drags: the page swipe stops while it shows (Display). A change of
    // mode sends LV_EVENT_VALUE_CHANGED to the tile, as a part change does, so that Display asks
    // again.
    bool swipesSideways() const;

    // Every update of the sky (Display::showPlanets).
    void show( int64_t now, const Astronomy::Observer &observer, int utcOffsetMinutes, bool located );

    // Development (host command "B <name>"): mode (the next mode), "drag <minutes>" (the dial's
    // time), now, hold (holds or releases the alidade's angle); flip, scroll <px> (SkyParts),
    // eyepiece, calc (OpticsParts).
    bool press( const char *name );

private:
    static void onTick( lv_timer_t *timer );
    static void onEvent( lv_event_t *event );
    static void onDialEvent( lv_event_t *event );
    static void onDrawDial( lv_event_t *event );
    static void onDrawQuadrant( lv_event_t *event );
    static void onDrawSundial( lv_event_t *event );
    static void onDrawAnalemma( lv_event_t *event );
    void stepMode();
    void updateSensor();
    void readTilt();
    // The plate's lines for the latitude, the year's analemma (into m_points).
    void computePlate();
    void computeAnalemma();
    // The rete's places at the dial's time.
    void computeRete();
    void showMode();
    void showDial();
    void showAlidade();
    void showSundial();
    void showAnalemma();

    StackPage m_stack;
    lv_obj_t *m_tile = nullptr;
    lv_timer_t *m_tick = nullptr;
    Settings *m_settings = nullptr;
    Motion *m_motion = nullptr;
    SkyParts m_sky;
    OpticsParts m_optics;
    int m_shownPart = -1;

    int64_t m_now = 0;
    Astronomy::Observer m_observer = { 0, 0 };
    int m_utcOffsetMinutes = 0;
    bool m_located = false;
    int64_t m_shownMinute = -1;  // the minute last drawn, so that a second's update draws nothing

    lv_obj_t *m_mode = nullptr;
    lv_obj_t *m_modeLabel = nullptr;
    lv_obj_t *m_rows[3] = {};  // the text rows under the drawing
    lv_obj_t *m_hint = nullptr;
    lv_obj_t *m_area = nullptr;  // the alidade's tap area

    // Dial: the plate's polylines in screen pixels relative to the dial's centre, kept in
    // m_points (allocated while the page is built).
    static const int PlateLines = 6;
    static const int LinePoints = 96;
    static const int AnalemmaPoints = 92;
    Pixel *m_points = nullptr;
    int m_lineCounts[PlateLines] = {};
    double m_plateLatitude = -999;
    int m_analemmaYear = -1;
    lv_obj_t *m_dial = nullptr;
    int m_dragMinutes = 0;
    bool m_pressed = false;
    bool m_axisChosen = false;
    bool m_sideways = false;
    int m_pressX = 0;
    int m_pressY = 0;
    int m_dragStart = 0;
    // The rete at the dial's time.
    Astrolabe::Circle m_ecliptic = { 0, 0, 1 };
    Pixel m_stars[Astrolabe::StarCount] = {};
    bool m_starUp[Astrolabe::StarCount] = {};
    bool m_starShown[Astrolabe::StarCount] = {};
    Pixel m_sun = { 0, 0 };
    bool m_sunUp = false;
    Pixel m_zenith = { 0, 0 };
    // The stars of the date (precession over a day's drag is far below a pixel).
    double m_starRa[Astrolabe::StarCount] = {};
    double m_starDec[Astrolabe::StarCount] = {};
    bool m_starsPrecessed = false;
    int64_t m_precessedAt = 0;

    // Alidade.
    lv_obj_t *m_quadrant = nullptr;
    lv_obj_t *m_angle = nullptr;
    Tilt m_tilt;
    bool m_held = false;
    double m_heldAltitude = 0;
    double m_pressAltitude = 0;
    double m_drawnAltitude = -1;

    // Sundial and analemma.
    lv_obj_t *m_sundial = nullptr;
    lv_obj_t *m_hourLabels[5] = {};
    double m_sunHourAngle = 0;
    double m_sunAltitude = 0;
    lv_obj_t *m_analemma = nullptr;
    Pixel m_today = { 0, 0 };
};
