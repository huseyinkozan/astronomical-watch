#include "AlwaysOn.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Panel.h"
#include "esp_attr.h"
#include "sdkconfig.h"

#if CONFIG_WATCH_AOD_IDLE_MODE
static const bool s_idleMode = true;
#else
static const bool s_idleMode = false;
#endif

// AlwaysOn::Colour in RGB565 with the bytes swapped, as the panel takes them (Panel::buffer()).
// Idle mode shows each channel on or off, so only full colours are used.
static const uint16_t s_colours[3] = { 0xFFFF, 0xE0FF, 0x00F8 };

static const int s_moonRadius = 18;
static const int s_moonSize = 2 * s_moonRadius + 1;
static const int s_itemsGap = 24;     // between the time and the moon and date
static const int s_moonTextGap = 16;
static const int s_batteryGap = 20;

static const char *s_dayNames[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

// Kept in RTC memory: cleared on power-on and reset.
RTC_DATA_ATTR static bool s_active;
RTC_DATA_ATTR static bool s_screenCleared;
RTC_DATA_ATTR static int s_failedMinutes;
RTC_DATA_ATTR static AlwaysOn::Look s_look;
// The rectangle drawn last, cleared with the next drawing.
RTC_DATA_ATTR static int s_drawnX;
RTC_DATA_ATTR static int s_drawnY;
RTC_DATA_ATTR static int s_drawnWidth;
RTC_DATA_ATTR static int s_drawnHeight;

static const uint64_t s_retryUs = 60ULL * 1000 * 1000;
// A lasting RTC fault ends the face, so that it cannot keep the watch out of off mode.
static const int s_maxFailedMinutes = 30;

static uint8_t s_moonBits[( s_moonSize * s_moonSize + 7 ) / 8];
static AodGlyph s_moon;

static void addSprite( AlwaysOn::Face &face, int x, int y, const AodGlyph *glyph, const lv_font_t *font, uint32_t code,
                       AlwaysOn::Colour colour )
{
    if ( face.count < AlwaysOn::MaxSprites && glyph->width > 0 ) {
        face.sprites[face.count++] = { x, y, glyph, font, code, colour };
    }
}

// The lit part of the Moon and its rim, from the illuminated fraction: the terminator is half an
// ellipse, on the right while the Moon waxes (as seen from the northern hemisphere). Dotted:
// every second pixel of every second row, as the dotted glyphs.
static const AodGlyph *moonGlyph( int percent, bool waxing, bool dotted )
{
    memset( s_moonBits, 0, sizeof( s_moonBits ) );
    const double lit = percent / 100.0;
    for ( int y = 0; y < s_moonSize; ++y ) {
        const double dy = y - s_moonRadius;
        const double half = std::sqrt( std::fmax( 0.0, s_moonRadius * s_moonRadius - dy * dy ) );
        const double terminator = half * ( 1 - 2 * lit );
        for ( int x = 0; x < s_moonSize; ++x ) {
            const double dx = x - s_moonRadius;
            const double distance = std::sqrt( dx * dx + dy * dy );
            if ( distance > s_moonRadius + 0.5 ) {
                continue;
            }
            const bool rim = distance > s_moonRadius - 1.5;
            const bool litSide = waxing ? dx >= terminator : -dx >= terminator;
            const bool onGrid = !dotted || ( ( x | y ) & 1 ) == 0;
            if ( ( rim || litSide ) && onGrid ) {
                const int bit = y * s_moonSize + x;
                s_moonBits[bit / 8] |= static_cast<uint8_t>( 0x80 >> ( bit % 8 ) );
            }
        }
    }
    s_moon = { s_moonSize, s_moonSize, s_moonBits };
    return &s_moon;
}

// Days since 1970-01-01 of a Gregorian date (H. Hinnant's days_from_civil).
static int daysFromCivil( int year, int month, int day )
{
    year -= month <= 2 ? 1 : 0;
    const int era = ( year >= 0 ? year : year - 399 ) / 400;
    const int yearOfEra = year - era * 400;
    const int dayOfYear = ( 153 * ( month + ( month > 2 ? -3 : 9 ) ) + 2 ) / 5 + day - 1;
    const int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

// The small glyph of a code point, solid or dotted; null when there is none.
static const AodGlyph *smallGlyph( uint32_t code, bool dotted )
{
    for ( int i = 0; i < AOD_SMALL_COUNT; ++i ) {
        if ( aodSmallCodes[i] == code ) {
            return dotted ? &aodSmallDotted[i] : &aodSmall[i];
        }
    }
    return nullptr;
}

AlwaysOn::Face AlwaysOn::layout( const Look &look, const Rtc::DateTime &time, bool batteryLow )
{
    Face face = {};
    const AodStyle &style = aodStyles[look.style];
    const bool stacked = style.colon.width == 0;
    const bool dotted = style.dottedItems;
    const bool showBattery = batteryLow && ( look.items & BatteryItem );
    const bool showMoon = look.items & MoonItem;
    const bool showDate = look.items & DateItem;
    const lv_font_t *smallFont = dotted ? &aodSmallDottedFont : &aodSmallFont;

    char date[8] = "";
    if ( showDate ) {
        const int weekday = ( daysFromCivil( time.year, time.month, time.day ) + 4 ) % 7;
        snprintf( date, sizeof( date ), "%s %d", s_dayNames[weekday], time.day );
    }
    const int dateWidth = static_cast<int>( strlen( date ) ) * aodSmallAdvance;
    const int dateHeight = aodSmall[1].height;  // [0] is the space
    const int rowWidth = ( showMoon ? s_moonSize : 0 ) + ( showMoon && showDate ? s_moonTextGap : 0 ) + dateWidth;
    const int rowHeight = ( showMoon || showDate ) ? ( dateHeight > s_moonSize ? dateHeight : s_moonSize ) : 0;
    const AodGlyph *battery = smallGlyph( AOD_BATTERY_CODE, dotted );

    face.width = style.width > rowWidth ? style.width : rowWidth;
    int y = 0;
    if ( showBattery && !stacked ) {
        addSprite( face, ( face.width - battery->width ) / 2, y, battery, smallFont, AOD_BATTERY_CODE, Red );
        y += battery->height + s_batteryGap;
    }
    const int timeX = ( face.width - style.width ) / 2;
    const int digits[4] = { time.hour / 10, time.hour % 10, time.minute / 10, time.minute % 10 };
    const int slots[4] = { 0, 1, 3, 4 };
    for ( int i = 0; i < 4; ++i ) {
        addSprite( face, timeX + style.x[slots[i]], y + style.y[slots[i]], &style.digits[digits[i]], style.font,
                   static_cast<uint32_t>( '0' + digits[i] ), White );
    }
    if ( !stacked ) {
        addSprite( face, timeX + style.x[2], y + style.y[2], &style.colon, style.font, ':', White );
    }
    y += style.height;
    if ( rowHeight > 0 ) {
        y += s_itemsGap;
        int x = ( face.width - rowWidth ) / 2;
        if ( showMoon ) {
            addSprite( face, x, y + ( rowHeight - s_moonSize ) / 2,
                       moonGlyph( look.moonPercent, look.waxing, dotted ), nullptr, 0, Yellow );
            x += s_moonSize + s_moonTextGap;
        }
        for ( int i = 0; date[i] != '\0'; ++i ) {
            const AodGlyph *glyph = smallGlyph( static_cast<uint8_t>( date[i] ), dotted );
            if ( glyph != nullptr ) {
                addSprite( face, x, y + ( rowHeight - dateHeight ) / 2, glyph, smallFont,
                           static_cast<uint8_t>( date[i] ), White );
            }
            x += aodSmallAdvance;
        }
        y += rowHeight;
    }
    if ( showBattery && stacked ) {
        y += s_batteryGap;
        addSprite( face, ( face.width - battery->width ) / 2, y, battery, smallFont, AOD_BATTERY_CODE, Red );
        y += battery->height;
    }
    face.height = y;
    return face;
}

// Draws the rectangle (even position and size) band by band: black, with the face's bitmaps
// where they fall into it.
static void render( Panel &panel, const AlwaysOn::Face &face, int faceX, int faceY, int left, int top, int width,
                    int height )
{
    uint16_t *pixels = Panel::buffer();
    const int bandRows = ( Panel::BufferPixels / width ) & ~1;
    for ( int bandTop = top; bandTop < top + height; bandTop += bandRows ) {
        const int rows = ( top + height - bandTop < bandRows ) ? top + height - bandTop : bandRows;
        memset( pixels, 0, sizeof( uint16_t ) * rows * width );
        for ( int i = 0; i < face.count; ++i ) {
            const AlwaysOn::Sprite &sprite = face.sprites[i];
            const AodGlyph &glyph = *sprite.glyph;
            const uint16_t colour = s_colours[sprite.colour];
            const int spriteX = faceX + sprite.x;
            const int spriteY = faceY + sprite.y;
            const int firstRow = spriteY > bandTop ? spriteY : bandTop;
            const int endRow = ( spriteY + glyph.height < bandTop + rows ) ? spriteY + glyph.height : bandTop + rows;
            for ( int row = firstRow; row < endRow; ++row ) {
                const int firstBit = ( row - spriteY ) * glyph.width;
                uint16_t *line = pixels + ( row - bandTop ) * width;
                for ( int x = 0; x < glyph.width; ++x ) {
                    const int bit = firstBit + x;
                    const int screenX = spriteX + x;
                    if ( ( glyph.bits[bit / 8] & ( 0x80 >> ( bit % 8 ) ) ) && screenX >= left && screenX < left + width ) {
                        line[screenX - left] = colour;
                    }
                }
            }
        }
        panel.draw( left, bandTop, width, rows, pixels );
    }
}

static int validStyle( int style )
{
    return ( style >= 0 && style < AOD_STYLE_COUNT ) ? style : 0;
}

#if CONFIG_WATCH_DEV_LOGGING
void AlwaysOn::capture( int style )
{
    const Look look = { validStyle( style ), MoonItem | DateItem | BatteryItem, 45, true };
    const Rtc::DateTime time = { 2026, 6, 21, 19, 19, 0 };
    const Face face = layout( look, time, true );
    const int faceX = ( ( Panel::WIDTH - face.width ) / 2 ) & ~1;
    const int faceY = ( ( Panel::HEIGHT - face.height ) / 2 ) & ~1;
    Panel panel;
    panel.record();
    printf( "SHOT BEGIN -1 %d %d always-on-%d\n", Panel::WIDTH, Panel::HEIGHT, look.style );
    panel.clear();
    render( panel, face, faceX, faceY, faceX, faceY, ( face.width + 1 ) & ~1, ( face.height + 1 ) & ~1 );
    printf( "SHOT END\n" );
    fflush( stdout );
}
#endif

void AlwaysOn::begin( const Look &look )
{
    s_active = true;
    s_screenCleared = false;
    s_failedMinutes = 0;
    s_look = look;
    s_look.style = validStyle( look.style );
    s_drawnWidth = 0;
    s_drawnHeight = 0;
}

void AlwaysOn::end()
{
    s_active = false;
}

bool AlwaysOn::isActive() const
{
    return s_active;
}

uint64_t AlwaysOn::update( const Rtc::DateTime &time, bool timeRead, bool batteryLow )
{
    // A failed or corrupt read must not turn into a long sleep: try again in a minute.
    if ( !timeRead || time.hour > 23 || time.minute > 59 || time.second > 59 ) {
        if ( ++s_failedMinutes >= s_maxFailedMinutes ) {
            s_active = false;
            return 0;
        }
        return s_retryUs;
    }
    s_failedMinutes = 0;

    // A wake up to 2 s early (RC timer drift) already shows the coming minute.
    int minuteOfDay = time.hour * 60 + time.minute;
    int secondsToNext = 60 - time.second;
    if ( time.second >= 58 ) {
        minuteOfDay = ( minuteOfDay + 1 ) % ( 24 * 60 );
        secondsToNext += 60;
    }

    Panel panel;
    if ( !panel.open() ) {
        s_active = false;
        return 0;
    }
    if ( !s_screenCleared ) {
        // Dim first: the panel still runs at the active-mode brightness.
        panel.setBrightness( CONFIG_WATCH_AOD_BRIGHTNESS );
        if ( s_idleMode ) {
            panel.setIdleMode();
        }
        panel.clear();
        s_screenCleared = true;
    }

#if !CONFIG_WATCH_AOD_BLANK
    Rtc::DateTime shown = time;
    shown.hour = static_cast<uint8_t>( minuteOfDay / 60 );
    shown.minute = static_cast<uint8_t>( minuteOfDay % 60 );
    const Face face = layout( s_look, shown, batteryLow );
    // Burn-in: the face moves within +-8 pixels, one step per minute.
    const int faceX = ( ( ( Panel::WIDTH - face.width ) / 2 ) & ~1 ) + ( minuteOfDay % 5 - 2 ) * 4;
    const int faceY = ( ( ( Panel::HEIGHT - face.height ) / 2 ) & ~1 ) + ( ( minuteOfDay / 5 ) % 5 - 2 ) * 4;
    const int width = ( face.width + 1 ) & ~1;
    const int height = ( face.height + 1 ) & ~1;
    // One rectangle covers the old face and the new one, so that the old one is cleared too.
    int left = faceX;
    int top = faceY;
    int right = faceX + width;
    int bottom = faceY + height;
    if ( s_drawnWidth > 0 ) {
        left = s_drawnX < left ? s_drawnX : left;
        top = s_drawnY < top ? s_drawnY : top;
        right = s_drawnX + s_drawnWidth > right ? s_drawnX + s_drawnWidth : right;
        bottom = s_drawnY + s_drawnHeight > bottom ? s_drawnY + s_drawnHeight : bottom;
    }
    render( panel, face, faceX, faceY, left, top, right - left, bottom - top );
    s_drawnX = faceX;
    s_drawnY = faceY;
    s_drawnWidth = width;
    s_drawnHeight = height;
#endif
    // The brightness command is sent after the drawing has finished.
    panel.setBrightness( CONFIG_WATCH_AOD_BRIGHTNESS );
    Panel::holdPins();

    // Land just after the minute changes; the RTC read on the next wake corrects any drift.
    return static_cast<uint64_t>( secondsToNext ) * 1000 * 1000 + 100 * 1000;
}
