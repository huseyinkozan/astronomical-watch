#include "TableText.h"

#include <cstring>

TableText::TableText()
{
    std::memset( m_texts, 0, sizeof( m_texts ) );
    for ( int i = 0; i < MaxRows; ++i ) {
        m_nameColours[i] = Grey;
        m_valueColours[i] = White;
    }
}

static bool inRange( int row, int cell )
{
    return row >= 0 && row < TableText::MaxRows && cell >= 0 && cell < TableText::Cells;
}

// The length of `text` cut to at most `room` bytes without splitting a UTF-8 letter.
static size_t cutLength( const char *text, size_t room )
{
    size_t length = std::strlen( text );
    if ( length <= room ) {
        return length;
    }
    length = room;
    // Continuation bytes are 10xxxxxx: step back to the start of the letter they belong to.
    while ( length > 0 && ( static_cast<unsigned char>( text[length] ) & 0xC0 ) == 0x80 ) {
        --length;
    }
    return length;
}

// Writes `text` cut to `room` - 1 bytes to `target`; false when it was there already.
static bool store( char *target, size_t room, const char *text )
{
    if ( text == nullptr ) {
        text = "";
    }
    const size_t length = cutLength( text, room - 1 );
    if ( std::strlen( target ) == length && std::strncmp( target, text, length ) == 0 ) {
        return false;
    }
    std::memcpy( target, text, length );
    target[length] = '\0';
    return true;
}

bool TableText::set( int row, int cell, const char *text )
{
    if ( !inRange( row, cell ) || !store( m_texts[row][cell], CellSize, text ) ) {
        return false;
    }
    m_changed = true;
    return true;
}

bool TableText::setLine( int row, const char *text )
{
    // The row's cells lie one after the other.
    if ( !inRange( row, 0 ) || !store( m_texts[row][0], Cells * CellSize, text ) ) {
        return false;
    }
    m_changed = true;
    return true;
}

const char *TableText::text( int row, int cell ) const
{
    return inRange( row, cell ) ? m_texts[row][cell] : "";
}

bool TableText::setColours( int row, uint32_t name, uint32_t value )
{
    if ( !inRange( row, 0 ) || ( m_nameColours[row] == name && m_valueColours[row] == value ) ) {
        return false;
    }
    m_nameColours[row] = name;
    m_valueColours[row] = value;
    m_changed = true;
    return true;
}

uint32_t TableText::nameColour( int row ) const
{
    return inRange( row, 0 ) ? m_nameColours[row] : Grey;
}

uint32_t TableText::valueColour( int row ) const
{
    return inRange( row, 0 ) ? m_valueColours[row] : White;
}

bool TableText::takeChanged()
{
    const bool changed = m_changed;
    m_changed = false;
    return changed;
}

// The byte where letter `letters` starts (or the end of the text).
static size_t letterStart( const char *text, int letters )
{
    size_t i = 0;
    int count = 0;
    while ( text[i] != '\0' ) {
        if ( ( static_cast<unsigned char>( text[i] ) & 0xC0 ) != 0x80 ) {
            if ( count == letters ) {
                return i;
            }
            ++count;
        }
        ++i;
    }
    return i;
}

void TableText::fitWithDots( const char *text, int maxLetters, char *out, size_t size )
{
    if ( size == 0 ) {
        return;
    }
    out[0] = '\0';
    if ( text == nullptr || maxLetters < 3 ) {
        return;
    }
    size_t length = std::strlen( text );
    const bool tooLong = text[letterStart( text, maxLetters )] != '\0';
    if ( tooLong ) {
        length = letterStart( text, maxLetters - 3 );
    }
    length = cutLength( text, length < size - 4 ? length : size - 4 );
    std::memcpy( out, text, length );
    out[length] = '\0';
    if ( tooLong ) {
        std::strcat( out, "..." );
    }
}

int TableText::columnTops( int titleHeight, const int *heights, int count, int gap, int areaHeight, int *tops )
{
    int total = titleHeight;
    for ( int i = 0; i < count; ++i ) {
        total += gap + heights[i];
    }
    const int top = ( areaHeight - total ) / 2;
    int y = top + titleHeight;
    for ( int i = 0; i < count; ++i ) {
        y += gap;
        tops[i] = y;
        y += heights[i];
    }
    return top;
}

void TableText::limitHeights( int *heights, int count, int lineHeight, int room )
{
    for ( int i = 0; i < count; ++i ) {
        const int extra = heights[i] - lineHeight;
        if ( extra <= 0 ) {
            continue;
        }
        // As many of its extra lines as the room takes.
        const int fits = extra <= room ? extra : room - room % lineHeight;
        heights[i] = lineHeight + fits;
        room -= fits;
    }
}
