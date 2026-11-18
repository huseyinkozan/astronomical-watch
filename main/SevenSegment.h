#pragma once

// The seven-segment digits of the faces (docs/superpowers/specs/2026-11-18-face-templates-design.md):
// hexagonal segments with pointed ends, a digit 0.62 times as wide as it is high (about 1/phi),
// segments a height / 11 thick with gaps of a height / 40 at the joints, upright. The active
// face fills them, the always-on face fills them with its dot grid. Plain C++ for the host tests.
class SevenSegment
{
public:
    struct Point
    {
        float x;
        float y;
    };

    struct Hexagon
    {
        Point p[6];
    };

    static float digitWidth( float height );

    // The lit segments of `digit` (0 to 9) with the digit's top-left corner at (0, 0); returns
    // how many.
    static int segments( int digit, float height, Hexagon out[7] );

    // The side of the colon's squares, which sit at 0.3 and 0.7 of the height.
    static float colonSize( float height );

    // Four digits and the colon: 0.16 of a digit's width between the digits of a pair, 0.18 on
    // each side of the colon.
    static float timeWidth( float height );

    // The digits' left edges and the colon's centre in a time block starting at 0.
    static void layout( float height, float x[4], float &colonX );
};
