#pragma once

#include <cstddef>
#include <cstdint>

// The texts and colours of a table part (TablePart), kept apart from LVGL so that the host tests
// can check them. A row has three cells: the name, then one or two values (a full-width line uses
// the first cell only). LVGL keeps only pointers to the texts it draws, so they live here for as
// long as the part.
class TableText
{
public:
    static const int MaxRows = 10;
    static const int Cells = 3;
    static const int CellSize = 32;
    static const uint32_t Grey = 0xA0A0A0;
    static const uint32_t White = 0xFFFFFF;

    TableText();

    // False, and nothing written, when the text is the same or the place is out of range; a
    // longer text is cut at a whole UTF-8 letter; null clears the cell.
    bool set( int row, int cell, const char *text );
    const char *text( int row, int cell ) const;

    // A full-width line in the room of the row's three cells (read back as cell 0); the same
    // rules as set().
    bool setLine( int row, const char *text );

    // The row's name and value colours (0xRRGGBB); false when they are the same.
    bool setColours( int row, uint32_t name, uint32_t value );
    uint32_t nameColour( int row ) const;
    uint32_t valueColour( int row ) const;

    // Whether anything changed since the last call.
    bool takeChanged();

    // The tops of a centred column, as LVGL's flex layout places it: the title (`titleHeight`),
    // then `count` rows of `heights`, `gap` between any two, in an area `areaHeight` high.
    // Returns the title's top and writes each row's top to `tops`.
    static int columnTops( int titleHeight, const int *heights, int count, int gap, int areaHeight, int *tops );

    // `text` cut to `maxLetters` letters with "..." at the end when it is longer, as LVGL's dots
    // mode shows it.
    static void fitWithDots( const char *text, int maxLetters, char *out, size_t size );

private:
    char m_texts[MaxRows][Cells][CellSize];
    uint32_t m_nameColours[MaxRows];
    uint32_t m_valueColours[MaxRows];
    bool m_changed = false;
};
