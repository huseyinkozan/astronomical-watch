#pragma once

#include "lvgl.h"

// The cycle page's symbols (docs/superpowers/specs/2026-11-22-cycle-page-design.md, "Symbols"),
// drawn into an LVGL layer: boxes are days (filled: passed, outlined: to come), drops bleeding
// (sized by the flow; estimated ones dim, half filled in the red night mode), rings fertile days
// (larger: the estimated ovulation), the ring open with a sperm inside: intercourse. Shapes carry
// the meaning, so that the night mode's single red keeps it.
namespace CycleDraw
{
// Colours: bleeding red, fertile blue, days grey, warnings amber, and their dim tones.
lv_color_t red();
lv_color_t dimRed();
lv_color_t blue();
lv_color_t dimBlue();
lv_color_t grey();
lv_color_t dimGrey();
lv_color_t amber();

// The drop's scale for a flow of 1 (light) to 3 (heavy).
float flowScale( int flow );

// A drop centred at (x, y), its tip up; about 9 x 12 px at scale 1. `half`: the outline with
// the lower half filled (an estimated drop at night).
void drop( lv_layer_t *layer, int x, int y, float scale, lv_color_t colour, bool half );

// A fertile day's ring (radius 5, or 7 and thicker for the ovulation).
void egg( lv_layer_t *layer, int x, int y, bool big, lv_color_t colour );

// The ring open towards `outAngle` (degrees, clockwise from 3 o'clock), a dot inside and its
// tail leaving through the opening.
void spermEgg( lv_layer_t *layer, int x, int y, int outAngle, lv_color_t colour );

// A day's box on a circle around (cx, cy): from angle a0 to a1 (degrees, clockwise from 3
// o'clock), between radius r - 8 and r + 8; filled or outlined.
void dayBox( lv_layer_t *layer, int cx, int cy, int r, float a0, float a1, bool filled, lv_color_t colour );

// The Moon's phase (0 new, 0.5 full) as a disc of `radius` lit on its right while it waxes.
void moon( lv_layer_t *layer, int x, int y, int radius, double phase, lv_color_t lit, lv_color_t dark );

// Whether `box` meets the area being drawn (the parts are drawn band by band).
bool inClip( const lv_layer_t *layer, const lv_area_t &box );
}  // namespace CycleDraw
