#include "AlwaysOn.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sys/time.h>

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
static const uint16_t s_colours[5] = { 0xFFFF, 0xE0FF, 0x00F8, 0xE007, 0x1F00 };

static const int s_moonRadius = 18;
static const int s_moonSize = 2 * s_moonRadius + 1;
static const int s_itemsGap = 24;  // between the time and the date

static const char *s_dayNames[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

// Kept in RTC memory: cleared on power-on and reset.
RTC_DATA_ATTR static bool s_active;
RTC_DATA_ATTR static bool s_screenCleared;
RTC_DATA_ATTR static int s_failedMinutes;
RTC_DATA_ATTR static int s_minutes;
RTC_DATA_ATTR static AlwaysOn::Look s_look;
// The rectangle drawn last, cleared with the next drawing.
RTC_DATA_ATTR static int s_drawnX;
RTC_DATA_ATTR static int s_drawnY;
RTC_DATA_ATTR static int s_drawnWidth;
RTC_DATA_ATTR static int s_drawnHeight;
// The corner gauges drawn last, in pixels along the gauge; kind -1 before the first drawing.
RTC_DATA_ATTR static int s_drawnGauges[Corners::CornerCount][3];

// Corner gauges: 2 px wide; a corner is redrawn as an even-sized box when its gauge changed by a
// pixel (the quarter circle is about 184 px long).
static const int s_gaugeWidth = 2;
static const int s_gaugePixels = 184;
static const int s_cornerBox = 122;

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

const AodGlyph *AlwaysOn::moon( const Look &look )
{
    if ( !( look.items & MoonItem ) ) {
        return nullptr;
    }
    return moonGlyph( look.moonPercent, look.waxing, aodStyles[look.style].dottedItems );
}

AlwaysOn::Face AlwaysOn::layout( const Look &look, const Rtc::DateTime &time )
{
    Face face = {};
    const AodStyle &style = aodStyles[look.style];
    const bool dotted = style.dottedItems;
    const bool showDate = look.items & DateItem;
    const lv_font_t *smallFont = dotted ? &aodSmallDottedFont : &aodSmallFont;

    char date[8] = "";
    // A time not read yet (all zero) has no weekday: the name table must not be left.
    const bool dateValid = time.month >= 1 && time.month <= 12 && time.day >= 1 && time.day <= 31;
    if ( showDate && dateValid ) {
        const int weekday = ( ( daysFromCivil( time.year, time.month, time.day ) + 4 ) % 7 + 7 ) % 7;
        snprintf( date, sizeof( date ), "%s %d", s_dayNames[weekday], time.day );
    }
    const int dateWidth = static_cast<int>( strlen( date ) ) * aodSmallAdvance;
    const int dateHeight = aodSmall[1].height;  // [0] is the space

    face.width = style.width > dateWidth ? style.width : dateWidth;
    int y = 0;
    const int timeX = ( face.width - style.width ) / 2;
    const int digits[4] = { time.hour / 10, time.hour % 10, time.minute / 10, time.minute % 10 };
    const int slots[4] = { 0, 1, 3, 4 };
    for ( int i = 0; i < 4; ++i ) {
        addSprite( face, timeX + style.x[slots[i]], y + style.y[slots[i]], &style.digits[digits[i]], style.font,
                   static_cast<uint32_t>( '0' + digits[i] ), White );
    }
    addSprite( face, timeX + style.x[2], y + style.y[2], &style.colon, style.font, ':', White );
    y += style.height;
    if ( showDate ) {
        y += s_itemsGap;
        int x = ( face.width - dateWidth ) / 2;
        for ( int i = 0; date[i] != '\0'; ++i ) {
            const AodGlyph *glyph = smallGlyph( static_cast<uint8_t>( date[i] ), dotted );
            if ( glyph != nullptr ) {
                addSprite( face, x, y, glyph, smallFont, static_cast<uint8_t>( date[i] ), White );
            }
            x += aodSmallAdvance;
        }
        y += dateHeight;
    }
    face.height = y;
    return face;
}

int AlwaysOn::gaugeColour( Corners::Kind kind )
{
    // By Corners::Kind; orange and the Moon's dark grey have no idle-mode colour.
    static const int colours[] = { -1, Yellow, Blue, Green, Yellow, Red, White, White, White };
    return colours[kind];
}

// The notches: 2 x 2 pixels across each gauge at both ends and in the middle, white. Their
// top-left pixels are computed once per wake, not per pixel.
static int s_notchX[Corners::CornerCount * 3];
static int s_notchY[Corners::CornerCount * 3];
static bool s_notchesReady = false;

static void computeNotches()
{
    const float radius = Corners::OuterRadius - s_gaugeWidth / 2.0f;
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        int centreX = 0;
        int centreY = 0;
        int lower = 0;
        int upper = 0;
        Corners::geometry( static_cast<Corners::Corner>( i ), Panel::WIDTH, Panel::HEIGHT, centreX, centreY, lower,
                           upper );
        for ( int k = 0; k < 3; ++k ) {
            const float angle = ( lower + ( upper - lower ) * k / 2.0f ) * static_cast<float>( M_PI ) / 180.0f;
            // The 2 x 2 block whose centre lies nearest to the notch point.
            s_notchX[i * 3 + k] = static_cast<int>( floorf( centreX + radius * cosf( angle ) - 0.5f ) );
            s_notchY[i * 3 + k] = static_cast<int>( floorf( centreY + radius * sinf( angle ) - 0.5f ) );
        }
    }
    s_notchesReady = true;
}

static bool notchPixel( int x, int y )
{
    for ( int i = 0; i < Corners::CornerCount * 3; ++i ) {
        const int dx = x - s_notchX[i];
        const int dy = y - s_notchY[i];
        if ( dx >= 0 && dx < 2 && dy >= 0 && dy < 2 ) {
            return true;
        }
    }
    return false;
}

// The colour of pixel (x, y) on a corner gauge, or 0 when no gauge lights it. The Moon below the
// horizon is dotted.
static uint16_t gaugePixel( const Corners::Gauge *gauges, int x, int y )
{
    const int outer = Corners::OuterRadius * Corners::OuterRadius;
    const int inner = ( Corners::OuterRadius - s_gaugeWidth ) * ( Corners::OuterRadius - s_gaugeWidth );
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        int centreX = 0;
        int centreY = 0;
        int lower = 0;
        int upper = 0;
        Corners::geometry( static_cast<Corners::Corner>( i ), Panel::WIDTH, Panel::HEIGHT, centreX, centreY, lower,
                           upper );
        const int dx = x - centreX;
        const int dy = y - centreY;
        const bool left = i == Corners::TopLeft || i == Corners::BottomLeft;
        const bool top = i == Corners::TopLeft || i == Corners::TopRight;
        if ( ( left ? dx > 0 : dx < 0 ) || ( top ? dy > 0 : dy < 0 ) ) {
            continue;
        }
        const int distance = dx * dx + dy * dy;
        if ( distance > outer || distance <= inner ) {
            continue;
        }
        const Corners::Gauge &gauge = gauges[i];
        const int colour = AlwaysOn::gaugeColour( gauge.kind );
        if ( colour < 0 || ( gauge.kind == Corners::MoonDown && ( ( x + y ) & 3 ) != 0 ) ) {
            return 0;
        }
        float angle = atan2f( static_cast<float>( dy ), static_cast<float>( dx ) ) * 180.0f / static_cast<float>( M_PI );
        const int lowest = lower < upper ? lower : upper;
        if ( angle < lowest - 1 ) {
            angle += 360.0f;
        }
        const int along = static_cast<int>( ( angle - lower ) * 1000.0f / ( upper - lower ) );
        return ( along >= gauge.start && along <= gauge.end ) ? s_colours[colour] : 0;
    }
    return 0;
}

// Draws the part of `glyph` (top-left corner at x, y on the screen) that falls into the band.
static void drawGlyph( uint16_t *pixels, int bandTop, int rows, int left, int width, const AodGlyph &glyph, int x,
                       int y, uint16_t colour )
{
    const int firstRow = y > bandTop ? y : bandTop;
    const int endRow = ( y + glyph.height < bandTop + rows ) ? y + glyph.height : bandTop + rows;
    for ( int row = firstRow; row < endRow; ++row ) {
        const int firstBit = ( row - y ) * glyph.width;
        uint16_t *line = pixels + ( row - bandTop ) * width;
        for ( int column = 0; column < glyph.width; ++column ) {
            const int bit = firstBit + column;
            const int screenX = x + column;
            if ( ( glyph.bits[bit / 8] & ( 0x80 >> ( bit % 8 ) ) ) && screenX >= left && screenX < left + width ) {
                line[screenX - left] = colour;
            }
        }
    }
}

// Draws the rectangle (even position and size) band by band: black, with the face's bitmaps, the
// moon (null: none) at its fixed place and the corner gauges (when given) where they fall into it.
static void render( Panel &panel, const AlwaysOn::Face &face, int faceX, int faceY, int left, int top, int width,
                    int height, const AodGlyph *moon, const Corners::Gauge *gauges )
{
    uint16_t *pixels = Panel::buffer();
    if ( !s_notchesReady ) {
        computeNotches();
    }
    const int bandRows = ( Panel::BufferPixels / width ) & ~1;
    for ( int bandTop = top; bandTop < top + height; bandTop += bandRows ) {
        const int rows = ( top + height - bandTop < bandRows ) ? top + height - bandTop : bandRows;
        memset( pixels, 0, sizeof( uint16_t ) * rows * width );
        for ( int i = 0; i < face.count; ++i ) {
            const AlwaysOn::Sprite &sprite = face.sprites[i];
            drawGlyph( pixels, bandTop, rows, left, width, *sprite.glyph, faceX + sprite.x, faceY + sprite.y,
                       s_colours[sprite.colour] );
        }
        if ( moon != nullptr ) {
            // White: yellow read as the Sun's state.
            drawGlyph( pixels, bandTop, rows, left, width, *moon, AlwaysOn::MoonX, AlwaysOn::MoonY,
                       s_colours[AlwaysOn::White] );
        }
        for ( int row = bandTop; gauges != nullptr && row < bandTop + rows; ++row ) {
            if ( row >= s_cornerBox && row < Panel::HEIGHT - s_cornerBox ) {
                continue;
            }
            uint16_t *line = pixels + ( row - bandTop ) * width;
            for ( int x = left; x < left + width; ++x ) {
                if ( x >= s_cornerBox && x < Panel::WIDTH - s_cornerBox ) {
                    continue;
                }
                const uint16_t colour = notchPixel( x, row ) ? s_colours[AlwaysOn::White] : gaugePixel( gauges, x, row );
                if ( colour != 0 ) {
                    line[x - left] = colour;
                }
            }
        }
        panel.draw( left, bandTop, width, rows, pixels );
    }
}

#if CONFIG_WATCH_AOD_CYCLE_MIN > 0
int AlwaysOn::cycleStyle( uint32_t seconds )
{
    // Blank, Dense, Thin (aodStyles), after an hour of blank. Run V used Sparse and Light, which
    // are gone; the result (no visible cost of lit pixels) does not depend on the style.
    static const int styles[3] = { -1, 2, 0 };
    const uint32_t settleS = 3600;
    if ( seconds < settleS ) {
        return -1;
    }
    return styles[( ( seconds - settleS ) / ( CONFIG_WATCH_AOD_CYCLE_MIN * 60 ) ) % 3];
}
#endif

static int validStyle( int style )
{
    return ( style >= 0 && style < AOD_STYLE_COUNT ) ? style : 0;
}

#if CONFIG_WATCH_DEV_LOGGING
void AlwaysOn::capture( int style )
{
    const Look look = { validStyle( style ), MoonItem | DateItem, 45, true, {}, 25, 2 };
    const Rtc::DateTime time = { 2026, 6, 21, 19, 19, 0 };
    const Face face = layout( look, time );
    // The scene's gauges: 91 % of the day, the battery at 92 %, the Moon 55 % through its pass
    // and 45 % lit, waxing.
    const Corners::Gauge gauges[Corners::CornerCount] = {
        { 0, 910, Corners::Day }, { 0, 920, Corners::BatteryHigh }, { 0, 546, Corners::MoonUp },
        { 0, 450, Corners::MoonLit } };
    const int faceX = ( ( Panel::WIDTH - face.width ) / 2 ) & ~1;
    const int faceY = ( ( Panel::HEIGHT - face.height ) / 2 ) & ~1;
    Panel panel;
    panel.record();
    printf( "SHOT BEGIN -1 %d %d always-on-%d\n", Panel::WIDTH, Panel::HEIGHT, look.style );
    panel.clear();
    const AodGlyph *moonBits = moon( look );
    render( panel, face, faceX, faceY, faceX, faceY, ( face.width + 1 ) & ~1, ( face.height + 1 ) & ~1, moonBits,
            gauges );
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        const int boxX = ( i == Corners::TopLeft || i == Corners::BottomLeft ) ? 0 : Panel::WIDTH - s_cornerBox;
        const int boxY = ( i == Corners::TopLeft || i == Corners::TopRight ) ? 0 : Panel::HEIGHT - s_cornerBox;
        render( panel, face, faceX, faceY, boxX, boxY, s_cornerBox, s_cornerBox, moonBits, gauges );
    }
    printf( "SHOT END\n" );
    fflush( stdout );
}
#endif

void AlwaysOn::begin( const Look &look )
{
    s_active = true;
    s_screenCleared = false;
    s_failedMinutes = 0;
    s_minutes = 0;
    s_look = look;
    s_look.style = validStyle( look.style );
    s_drawnWidth = 0;
    s_drawnHeight = 0;
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        s_drawnGauges[i][2] = -1;
    }
}

void AlwaysOn::end()
{
    s_active = false;
}

bool AlwaysOn::isActive() const
{
    return s_active;
}

int AlwaysOn::minutes() const
{
    return s_minutes;
}

uint64_t AlwaysOn::update( const Rtc::DateTime &time, int64_t utcNow, bool timeRead, int batteryPercent )
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
    Look look = s_look;
    Corners::Gauge gauges[Corners::CornerCount] = {};
    Corners::compute( look.corners, utcNow, batteryPercent, gauges );
    // The day level from sunrise to sunset (the day gauge), else the night level.
    const int brightness = gauges[Corners::TopLeft].kind == Corners::Night ? look.nightBrightness : look.dayBrightness;
    if ( !s_screenCleared ) {
        // Dim first: the panel still runs at the active-mode brightness.
        panel.setBrightness( brightness );
        if ( s_idleMode ) {
            panel.setIdleMode();
        }
        panel.clear();
        s_screenCleared = true;
    }

#if CONFIG_WATCH_AOD_CYCLE_MIN > 0
    // System time runs from power-on through deep sleep, as in the battery samples.
    timeval now = {};
    gettimeofday( &now, nullptr );
    look.style = cycleStyle( static_cast<uint32_t>( now.tv_sec ) );
    if ( look.style < 0 ) {
        look.items = 0;
        look.style = 0;
    }
    const bool blank = cycleStyle( static_cast<uint32_t>( now.tv_sec ) ) < 0;
#else
    const bool blank = false;
#endif
    Rtc::DateTime shown = time;
    shown.hour = static_cast<uint8_t>( minuteOfDay / 60 );
    shown.minute = static_cast<uint8_t>( minuteOfDay % 60 );
    Face face = layout( look, shown );
    if ( blank ) {
        // Cleared like any other face, with nothing drawn.
        face.count = 0;
    }
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
    const AodGlyph *moonBits = blank ? nullptr : moon( look );
    render( panel, face, faceX, faceY, left, top, right - left, bottom - top, moonBits, gauges );
    s_drawnX = faceX;
    s_drawnY = faceY;
    s_drawnWidth = width;
    s_drawnHeight = height;
    // The corners stay in place (no burn-in shift: they lie against the edge); one is drawn again
    // when its gauge changed by a pixel.
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        const int drawn[3] = { gauges[i].start * s_gaugePixels / 1000, gauges[i].end * s_gaugePixels / 1000,
                               gauges[i].kind };
        if ( drawn[0] == s_drawnGauges[i][0] && drawn[1] == s_drawnGauges[i][1] && drawn[2] == s_drawnGauges[i][2] ) {
            continue;
        }
        const int boxX = ( i == Corners::TopLeft || i == Corners::BottomLeft ) ? 0 : Panel::WIDTH - s_cornerBox;
        const int boxY = ( i == Corners::TopLeft || i == Corners::TopRight ) ? 0 : Panel::HEIGHT - s_cornerBox;
        render( panel, face, faceX, faceY, boxX, boxY, s_cornerBox, s_cornerBox, moonBits, gauges );
        for ( int k = 0; k < 3; ++k ) {
            s_drawnGauges[i][k] = drawn[k];
        }
    }
    ++s_minutes;
    // The brightness command is sent after the drawing has finished.
    panel.setBrightness( brightness );
    Panel::holdPins();

    // Land just after the minute changes; the RTC read on the next wake corrects any drift.
    return static_cast<uint64_t>( secondsToNext ) * 1000 * 1000 + 100 * 1000;
}
