#pragma once

#include <cstdint>

#include "EmergencyCard.h"
#include "TablePart.h"
#include "lvgl.h"

// Two parts of the tools page (ToolPages): the metronome and the emergency card, both hidden by
// default (the metronome by the setting's default, the card while it is empty). Called with the
// display lock held.
//
// - Metronome: a tempo from 30 to 240 beats a minute (- and + keys, a long press steps by 5; a
//   CPR key sets 110), the beats in a bar (none, 2, 3, 4; a tap on the dots steps them; the
//   first beat higher), the beat shown by the dots and heard through the ringer at the alarm's
//   lowest level (Ringer::startBeats; the codec runs only meanwhile). It stops when its part is
//   left or active mode ends. While it runs the display timeout does not end active mode, up to
//   10 min after the last tap on the part (shown); then active mode ends as at a timeout (main).
// - Emergency card: the portal's name, blood type, allergies, medication, contact and phone and
//   a note, as rows (TablePart) under a red title; long values wrap, an empty one shows "-".
class ToolParts
{
public:
    void build( lv_obj_t *metronome, lv_obj_t *card, const EmergencyCard &emergency );
    void forget();

    // A new card from the portal.
    void showCard( const EmergencyCard &emergency );

    // Display: whether the metronome is on screen; hiding it stops the beats.
    void setMetronomeShown( bool shown );

    // Seconds until the running metronome ends active mode, -1 while it does not run.
    int metronomeSecondsLeft() const;

    // The tools page's tick (40 ms): the dots and the time left.
    void tick();

    // Development (host command "B <name>"): beat (start or stop), "tempo <bpm>", bar (the next
    // beats in a bar), cpr.
    bool press( const char *name );

private:
    static void onKeyEvent( lv_event_t *event );
    static void onDrawDots( lv_event_t *event );
    void setTempo( int bpm );
    void startStop( bool run );
    // The beat now and how far into it (0..1), -1 while stopped.
    int beatNow( double *phase ) const;
    void showMetronome();

    // Metronome.
    lv_obj_t *m_tempo = nullptr;
    lv_obj_t *m_dots = nullptr;
    lv_obj_t *m_minus = nullptr;
    lv_obj_t *m_cpr = nullptr;
    lv_obj_t *m_plus = nullptr;
    lv_obj_t *m_startStop = nullptr;
    lv_obj_t *m_startLabel = nullptr;
    lv_obj_t *m_status = nullptr;
    bool m_shown = false;
    bool m_running = false;
    int64_t m_startUs = 0;    // the first beat of the running clock (esp_timer time)
    int64_t m_untilUs = 0;    // the end of active mode's hold
    int m_litDot = -1;        // the dot drawn lit, so that the dots are drawn again only on a change
    int m_shownSeconds = -1;  // the time left shown

    // Emergency card.
    TablePart m_cardTable;
};
