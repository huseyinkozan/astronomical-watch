#include "FaceConfig.h"

#include <cstdio>
#include <cstdlib>

static const int s_numbersPerPage = 5 + 2 * FaceConfig::SlotCount + 1;

static int itemLimit( int slot )
{
    return slot < FaceConfig::TextSlotCount ? static_cast<int>( FaceConfig::TextItemCount )
                                            : static_cast<int>( FaceConfig::CornerItemCount );
}

// How many values the parameter of a slot holding `item` can take: a city or a counter index, else
// only 0.
static int parameterLimit( int slot, int item )
{
    if ( slot < FaceConfig::TextSlotCount && item == FaceConfig::CityTime ) {
        return FaceConfig::CityCount;
    }
    if ( slot < FaceConfig::TextSlotCount && item == FaceConfig::DayCounter ) {
        return FaceConfig::CounterCount;
    }
    return 1;
}

void FaceConfig::defaults( Pages &pages )
{
    pages = {};
    pages.count = 2;

    Page &segments = pages.pages[0];
    segments.layout = Large;
    segments.clock = DigitalClock;
    segments.digital = SevenSegment;
    segments.items[TopEdge] = ShortDate;
    segments.items[BottomInner] = NextSunEvent;
    segments.items[BottomEdge] = HijriDate;
    segments.items[TopLeft] = WeekdayIcon;
    segments.items[BottomLeft] = MoonIcon;
    segments.items[BottomRight] = DayOfMonth;
    segments.alwaysOn = ( 1 << TopEdge ) | ( 1 << TopLeft ) | ( 1 << BottomLeft ) | ( 1 << BottomRight );

    // Like the classic face of earlier versions.
    Page &mono = pages.pages[1];
    mono.layout = Large;
    mono.clock = DigitalClock;
    mono.digital = MonoDigits;
    mono.items[TopInner] = LongDate;
    mono.items[BottomInner] = HijriDate;
    mono.items[BottomEdge] = NextSunEvent;
    mono.items[BottomLeft] = MoonIcon;
    mono.alwaysOn = ( 1 << TopInner ) | ( 1 << BottomLeft );
}

static bool pageValid( const FaceConfig::Page &page )
{
    if ( page.layout >= FaceConfig::TemplateCount || page.clock >= FaceConfig::ClockTypeCount ||
         page.digital >= FaceConfig::DigitalStyleCount || page.analog >= FaceConfig::AnalogStyleCount ) {
        return false;
    }
    for ( int slot = 0; slot < FaceConfig::SlotCount; ++slot ) {
        if ( page.items[slot] >= itemLimit( slot ) || page.parameters[slot] >= parameterLimit( slot, page.items[slot] ) ) {
            return false;
        }
    }
    return true;
}

bool FaceConfig::valid( const Pages &pages )
{
    if ( pages.count < 1 || pages.count > MaxPages ) {
        return false;
    }
    for ( int i = 0; i < pages.count; ++i ) {
        if ( !pageValid( pages.pages[i] ) ) {
            return false;
        }
    }
    return true;
}

static void clampPage( FaceConfig::Page &page )
{
    if ( page.layout >= FaceConfig::TemplateCount ) {
        page.layout = 0;
    }
    if ( page.clock >= FaceConfig::ClockTypeCount ) {
        page.clock = 0;
    }
    if ( page.digital >= FaceConfig::DigitalStyleCount ) {
        page.digital = 0;
    }
    if ( page.analog >= FaceConfig::AnalogStyleCount ) {
        page.analog = 0;
    }
    for ( int slot = 0; slot < FaceConfig::SlotCount; ++slot ) {
        if ( page.items[slot] >= itemLimit( slot ) ) {
            page.items[slot] = 0;
        }
        if ( page.parameters[slot] >= parameterLimit( slot, page.items[slot] ) ) {
            page.parameters[slot] = 0;
        }
    }
}

void FaceConfig::clamp( Pages &pages )
{
    if ( pages.count < 1 ) {
        pages.count = 1;
    }
    if ( pages.count > MaxPages ) {
        pages.count = MaxPages;
    }
    for ( int i = 0; i < pages.count; ++i ) {
        clampPage( pages.pages[i] );
    }
}

int FaceConfig::encode( const Pages &pages, char *text, int size )
{
    int length = 0;
    for ( int i = 0; i < pages.count; ++i ) {
        const Page &page = pages.pages[i];
        int numbers[s_numbersPerPage] = { page.layout, page.clock, page.digital, page.analog, page.flags };
        for ( int slot = 0; slot < SlotCount; ++slot ) {
            numbers[5 + slot] = page.items[slot];
            numbers[5 + SlotCount + slot] = page.parameters[slot];
        }
        numbers[s_numbersPerPage - 1] = page.alwaysOn;
        for ( int n = 0; n < s_numbersPerPage; ++n ) {
            const char *separator = n > 0 ? "," : ( i > 0 ? ";" : "" );
            const int written = snprintf( text + length, size - length, "%s%d", separator, numbers[n] );
            if ( written < 0 || written >= size - length ) {
                return -1;
            }
            length += written;
        }
    }
    return length;
}

bool FaceConfig::decode( const char *text, Pages &pages )
{
    Pages result = {};
    const char *cursor = text;
    while ( true ) {
        if ( result.count >= MaxPages ) {
            return false;
        }
        int numbers[s_numbersPerPage] = {};
        for ( int n = 0; n < s_numbersPerPage; ++n ) {
            char *end = nullptr;
            const long value = strtol( cursor, &end, 10 );
            if ( end == cursor || value < 0 || value > 255 ) {
                return false;
            }
            numbers[n] = static_cast<int>( value );
            cursor = end;
            const char expected = n < s_numbersPerPage - 1 ? ',' : ';';
            if ( *cursor == expected ) {
                ++cursor;
            } else if ( !( n == s_numbersPerPage - 1 && *cursor == '\0' ) ) {
                return false;
            }
        }
        Page &page = result.pages[result.count++];
        page.layout = static_cast<uint8_t>( numbers[0] );
        page.clock = static_cast<uint8_t>( numbers[1] );
        page.digital = static_cast<uint8_t>( numbers[2] );
        page.analog = static_cast<uint8_t>( numbers[3] );
        page.flags = static_cast<uint8_t>( numbers[4] );
        for ( int slot = 0; slot < SlotCount; ++slot ) {
            page.items[slot] = static_cast<uint8_t>( numbers[5 + slot] );
            page.parameters[slot] = static_cast<uint8_t>( numbers[5 + SlotCount + slot] );
        }
        page.alwaysOn = static_cast<uint8_t>( numbers[s_numbersPerPage - 1] );
        if ( *cursor == '\0' ) {
            break;
        }
    }
    clamp( result );
    pages = result;
    return true;
}

int FaceConfig::shown( const Pages &pages, int list[MaxPages] )
{
    int count = 0;
    for ( int i = 0; i < pages.count && i < MaxPages; ++i ) {
        if ( ( pages.pages[i].flags & HiddenFlag ) == 0 ) {
            list[count++] = i;
        }
    }
    if ( count == 0 ) {
        list[count++] = 0;
    }
    return count;
}

bool FaceConfig::alwaysOnSlot( const Page &page, int slot )
{
    if ( slot < 0 || slot >= SlotCount || ( page.alwaysOn & ( 1 << slot ) ) == 0 ) {
        return false;
    }
    if ( slot < TextSlotCount ) {
        return page.items[slot] != Seconds;
    }
    return page.items[slot] != SecondCorner;
}
