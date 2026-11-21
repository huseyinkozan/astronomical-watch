#pragma once

#include "Settings.h"
#include "TablePart.h"
#include "lvgl.h"

// Two parts of the observing page (ObservingPages): the eyepiece calculator and the night photo
// calculators, from the equipment the setup portal set (Settings, Optics). Called with the
// display lock held.
//
// - Eyepieces: the telescope, then one eyepiece (a tap anywhere steps to the next one set):
//   magnification, exit pupil, true field, limiting magnitude and Dawes' limit.
// - Night photo, three calculators stepped by a tap on the title: the longest exposure without
//   star trails (500 and NPF rules) for the lens and sensor, the exposure behind an ND filter,
//   and a star-trail series' frames and arc.
// The choices are kept in RTC memory; the settings are read again each time a part shows.
class OpticsParts
{
public:
    void build( lv_obj_t *eyepiece, lv_obj_t *photo, Settings &settings );
    void forget();

    // Fills the parts from the settings again (a part comes into view).
    void refresh();

    // Development (host command "B <name>"): eyepiece (the next one), calc (the next calculator).
    bool press( const char *name );

private:
    static void onTap( lv_event_t *event );
    void showEyepiece();
    void showPhoto();

    Settings *m_settings = nullptr;
    TablePart m_eyepieceTable;
    TablePart m_photoTable;
    lv_obj_t *m_eyepieceTap = nullptr;
    lv_obj_t *m_photoTap = nullptr;
};
