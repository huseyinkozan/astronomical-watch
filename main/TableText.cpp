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

bool TableText::set( int row, int cell, const char *text )
{
    if ( !inRange( row, cell ) ) {
        return false;
    }
    if ( text == nullptr ) {
        text = "";
    }
    const size_t length = cutLength( text, CellSize - 1 );
    char *target = m_texts[row][cell];
    if ( std::strlen( target ) == length && std::strncmp( target, text, length ) == 0 ) {
        return false;
    }
    std::memcpy( target, text, length );
    target[length] = '\0';
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
