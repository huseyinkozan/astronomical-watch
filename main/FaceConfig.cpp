#include "FaceConfig.h"

#include <cstdio>
#include <cstdlib>

static const int s_numbersPerPage = 5 + 2 * FaceConfig::SlotCount + 1;

static int gaugeParameterLimit( int item );

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
    if ( slot >= FaceConfig::TextSlotCount && item == FaceConfig::AlarmDays ) {
        return FaceConfig::AlarmCount;
    }
    return 1;
}

// The owner's pages of 2026-11-19, as encode() writes them (read from the watch with the
// development command "M"), with the Wi-Fi icon bottom right on the first four and the emergency
// card's blood type on every bottom edge, also on the always-on face (2026-11-22), in the owner's
// order of 2026-11-22 (the arcs first):
// 1. Full arcs around an analog Classic clock with the seconds hand, Milky Way and planets; the
//    time on the top edge, the short date below it; the weekday ring, alarm 1's days,
//    the Moon's phase, the Wi-Fi icon.
// 2. Full arcs, seven-segment digits, Milky Way and planets; short date, next sunrise or sunset;
//    the weekday ring, the Moon's phase and the Wi-Fi icon in the corners.
// 3. Half arcs, JetBrains Mono, Milky Way and planets; the weekday ring, the Sun's
//    altitude, the Moon's phase, the Wi-Fi icon.
// 4. Large seven-segment digits with the slots of earlier versions.
// 5. Large JetBrains Mono, the short date and the next sunrise or sunset below, no corners.
// The corner gauges: the day or night, the battery, the Moon's pass and the display timeout.
static const char *const s_defaultPages =
    "1,1,1,0,7,12,2,0,20,2,13,1,17,0,0,0,0,0,0,0,0,27;"
    "1,0,1,0,6,2,4,0,20,2,0,1,17,0,0,0,0,0,0,0,0,25;"
    "2,0,0,0,6,0,0,0,20,2,8,1,17,0,0,0,0,0,0,0,0,88;"
    "0,0,1,0,0,2,5,0,20,2,8,1,17,0,0,0,0,0,0,0,0,89;"
    "0,0,0,0,0,0,2,4,20,0,0,0,0,0,0,0,0,0,0,0,0,8"
    "/1,2,3,19,0,0,0,0";

void FaceConfig::defaults( Pages &pages )
{
    pages = {};
    if ( !decode( s_defaultPages, pages ) ) {
        // Not reached while the host tests pass: one Large page with the Mono digits.
        pages = {};
        pages.count = 1;
        pages.pages[0].layout = Large;
        pages.pages[0].clock = DigitalClock;
    }
}

FaceConfig::ClockPlace FaceConfig::clockPlace( int layout )
{
    // The mock-up's places: Full arcs inside the Moon's orbit (r 122), Half arcs below the
    // horizon (y 232) and above the bottom slots.
    if ( layout == FullArcs ) {
        return { 251, 64, 100 };
    }
    if ( layout == HalfArcs ) {
        return { 310, 64, 70 };
    }
    return { 251, 122, 140 };
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
    for ( int i = 0; i < 4; ++i ) {
        if ( pages.gauges[i] >= GaugeItemCount || pages.gaugeParameters[i] >= gaugeParameterLimit( pages.gauges[i] ) ) {
            return false;
        }
    }
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

static int gaugeParameterLimit( int item )
{
    if ( item == FaceConfig::DayCounterGauge ) {
        return 16;
    }
    return item == FaceConfig::HourGauge ? 2 : 1;
}

static void clampGauges( FaceConfig::Pages &pages )
{
    for ( int i = 0; i < 4; ++i ) {
        if ( pages.gauges[i] >= FaceConfig::GaugeItemCount ) {
            pages.gauges[i] = FaceConfig::NoGauge;
        }
        if ( pages.gaugeParameters[i] >= gaugeParameterLimit( pages.gauges[i] ) ) {
            pages.gaugeParameters[i] = 0;
        }
    }
}

void FaceConfig::clamp( Pages &pages )
{
    clampGauges( pages );
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
    // The corner gauges after the pages: "/" and their four items and four parameters.
    for ( int n = 0; n < 8; ++n ) {
        const int value = n < 4 ? pages.gauges[n] : pages.gaugeParameters[n - 4];
        const int written = snprintf( text + length, size - length, "%s%d", n == 0 ? "/" : ",", value );
        if ( written < 0 || written >= size - length ) {
            return -1;
        }
        length += written;
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
            } else if ( !( n == s_numbersPerPage - 1 && ( *cursor == '\0' || *cursor == '/' ) ) ) {
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
        if ( *cursor == '\0' || *cursor == '/' ) {
            break;
        }
    }
    // The corner gauges; an older string without them gets the default four (named here: the
    // defaults themselves are decoded from a string).
    static const uint8_t s_defaultGauges[4] = { DayNightGauge, BatteryGauge, MoonPassGauge, TimeoutGauge };
    for ( int i = 0; i < 4; ++i ) {
        result.gauges[i] = s_defaultGauges[i];
        result.gaugeParameters[i] = 0;
    }
    if ( *cursor == '/' ) {
        ++cursor;
        for ( int n = 0; n < 8; ++n ) {
            char *end = nullptr;
            const long value = strtol( cursor, &end, 10 );
            if ( end == cursor || value < 0 || value > 255 || ( n < 7 && *end != ',' ) || ( n == 7 && *end != '\0' ) ) {
                return false;
            }
            if ( n < 4 ) {
                result.gauges[n] = static_cast<uint8_t>( value );
            } else {
                result.gaugeParameters[n - 4] = static_cast<uint8_t>( value );
            }
            cursor = end + ( n < 7 ? 1 : 0 );
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

int FaceConfig::textWidth( const Page &page, int slot )
{
    // Measured on the panel with the 26 px font (16 px a character): 11 characters between the
    // gauges' ends at the edges; an inner slot keeps clear of a corner's icon or three letters
    // (13 characters), or reaches the gauges (20 characters).
    if ( slot == TopEdge || slot == BottomEdge ) {
        return 176;
    }
    if ( slot != TopInner && slot != BottomInner ) {
        return 0;
    }
    const int left = slot == TopInner ? TopLeft : BottomLeft;
    const int right = slot == TopInner ? TopRight : BottomRight;
    return page.items[left] != NoCorner || page.items[right] != NoCorner ? 208 : 320;
}
