#pragma once

// The order and visibility of a page's parts or of the face styles, as the setup portal sets
// them (Settings::ClockOrder and the like): the order is a list of part numbers in 3-bit fields
// (the first part in the lowest bits, up to eight), the hidden ones a bit mask. A value that is
// not a permutation (0, the default) means the natural order. Plain C++ for the host tests.
class PartOrder
{
public:
    static const int MaxParts = 8;

    // Writes the parts to show, in order, to `parts` and returns how many; at least one, even if
    // everything is hidden. `alwaysShown`: a part that cannot be hidden (-1: none).
    static int decode( int order, int hidden, int count, int alwaysShown, int parts[MaxParts] );

    // The part `steps` places after `current` in `parts` (wrapping); `current` when it is not
    // there.
    static int step( const int *parts, int shown, int current, int steps );

    // The position of `part` in `parts`, -1 when it is not shown.
    static int position( const int *parts, int shown, int part );
};
