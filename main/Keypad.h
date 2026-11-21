#pragma once

#include "Calculator.h"
#include "lvgl.h"

// The calculator's keys (Calculator's pads: 5 rows of 4, C and = in amber), drawn by one object
// instead of an object per key, which keeps the heap free for the setup portal's Wi-Fi. A key
// takes the tap when the finger lifts on it; sliding off it or a swipe cancels the tap, so that
// the parts' vertical swipe goes on over the keys. Used by the calculator and by the unit
// converter's entry. Called with the display lock held.
class Keypad
{
public:
    // About 78 x 58 px a key (the mock-ups), 6 px apart: inside the corner gauges from x 40 to
    // 370 and down to 454 on the screen.
    static const int KeyWidth = 78;
    static const int KeyHeight = 58;
    static const int Gap = 6;
    static const int Width = Calculator::Columns * ( KeyWidth + Gap ) - Gap;
    static const int Height = Calculator::Rows * ( KeyHeight + Gap ) - Gap;

    // At (`x`, `y`) of `parent`, showing `calculator`'s pad; `onKey` with `user` gets each key
    // tapped (the caller presses it, then calls refresh()).
    void build( lv_obj_t *parent, int x, int y, Calculator &calculator, void ( *onKey )( void *user, Calculator::Key key ),
                void *user );
    // The object was deleted with its parent.
    void forget();
    // The pad or a label changed (fn, DEG).
    void refresh();

private:
    static void onEvent( lv_event_t *event );
    static void onDraw( lv_event_t *event );
    // The first cell of the key under `point` (screen coordinates), -1 in a gap or outside.
    int cellAt( const lv_point_t &point ) const;
    void setPressed( int cell );

    lv_obj_t *m_object = nullptr;
    Calculator *m_calculator = nullptr;
    void ( *m_onKey )( void *user, Calculator::Key key ) = nullptr;
    void *m_user = nullptr;
    int m_pressed = -1;   // the key lit under the finger
    int m_released = -1;  // the key the finger left, until LVGL says whether it was a tap
};
