#pragma once

#include "TableText.h"
#include "lvgl.h"

// A part of a stacked page that shows a title and rows of a name and one or two values, drawn by
// one LVGL object from its own texts (TableText): one object instead of two or three labels a
// row (the information page's tables cost 43 KB of internal heap as labels). Used in the LVGL
// task with the display lock held.
class TablePart
{
public:
    enum RowKind : uint8_t
    {
        TwoValues,  // the name, then two values in their columns
        OneValue,   // the name, then one value over both value columns
        Line,       // one centred line over the whole width
    };

    struct Layout
    {
        const char *title;
        int rowCount;
        RowKind kinds[TableText::MaxRows];
        // The title's top in the part; -1: the title and rows centred vertically, as a column.
        int top;
        int titleGap;  // from the title's top to the first row's top
        int pitch;     // from one row's top to the next
        int nameX;
        int nameWidth;
        int valueX;
        int valueWidth;  // of one value column
        int dotsLetters; // > 0: a longer one-value text ends in dots after this many letters
    };

    // The table fills `parent`'s content area.
    void build( lv_obj_t *parent, const Layout &layout );

    TableText &text();

    // Draws the part again when a text or colour changed since the last refresh.
    void refresh();

private:
    static void onDraw( lv_event_t *event );
    void drawCell( lv_layer_t *layer, const lv_area_t &coords, int x, int y, int width, const char *text,
                   uint32_t colour, lv_text_align_t align ) const;

    lv_obj_t *m_object = nullptr;
    Layout m_layout = {};
    TableText m_text;
    char m_dots[TableText::MaxRows][TableText::CellSize] = {};
};
