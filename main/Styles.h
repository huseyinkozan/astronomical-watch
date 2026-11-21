#pragma once

#include "lvgl.h"

// Styles shared by many objects of active mode. LVGL gives an object its own copy of every style
// set with lv_obj_set_style_*; one shared style per look keeps that heap free (the pages are
// built and freed in active mode, where the setup portal needs the room). Used in the LVGL task
// only; a later lv_obj_set_style_* on one object still overrides the shared value there.
class Styles
{
public:
    // The text look: font and colour, from a small table of the pairs in use (a pair beyond
    // the table falls back to the object's own styles).
    static void text( lv_obj_t *object, const lv_font_t *font, lv_color_t colour );

    // Text centred within the object.
    static void centred( lv_obj_t *object );

    // The settings rows and the clock page's buttons: black with a mid-grey outline, lit while
    // pressed; `radius` 24 (rows, with 24 px side padding) or 18 (buttons, no padding).
    static void row( lv_obj_t *object );
    static void button( lv_obj_t *object );

    // The launcher's rows, on an object without styles (lv_obj_remove_style_all): a faint 1 px
    // outline and no background, cheap to draw while the list scrolls; lit while pressed, a
    // brighter outline while checked (LV_STATE_CHECKED: the part last opened). The
    // size and the label's place come from the shared style too: about 15 KB of heap went to the
    // launcher with them set on each row.
    static const int LauncherRowWidth = 306;
    static const int LauncherRowHeight = 68;
    static void launcherRow( lv_obj_t *object );
};
