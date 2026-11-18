#include "AlwaysOn.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/time.h>

#include "CornerTable.h"
#include "Dates.h"
#include "FaceText.h"
#include "Language.h"
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
static const uint16_t s_colours[6] = { 0xFFFF, 0xE0FF, 0x00F8, 0xE007, 0x1F00, 0xFFFF };

static const int s_moonRadius = 18;  // the corner moon, as large as the active face's
static const int s_moonSize = 2 * s_moonRadius + 1;
// The weekday icon's notches: chords of the active face's ring (radius 20, 4 px wide), with
// wider gaps than its 8 degrees, which the lines' square ends would close.
static const float s_notchRadius = 18;
static const float s_notchGap = 18;  // degrees between two notches
// Burn-in: the face moves within +-s_shift pixels, one step per minute; its box keeps that far
// from the screen's edges.
static const int s_shift = 8;

// The face of the last layout() (too large for the main task's stack).
static AlwaysOn::Face s_face;

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
// The corner gauges drawn last: start and end in pixels along the gauge, kind (-1 before the
// first drawing) and charging.
RTC_DATA_ATTR static int s_drawnGauges[Corners::CornerCount][4];

// Corner gauges (Corners::AlwaysOnWidth): a corner is redrawn as an even-sized box when its gauge
// changed by a pixel (the quarter circle is about 184 px long).
static const int s_gaugePixels = 184;
static const int s_cornerBox = 122;

static const uint64_t s_retryUs = 60ULL * 1000 * 1000;
// A lasting RTC fault ends the face, so that it cannot keep the watch out of off mode.
static const int s_maxFailedMinutes = 30;

static uint8_t s_moonBits[( s_moonSize * s_moonSize + 7 ) / 8];
static AodGlyph s_moon;

static void addSprite( AlwaysOn::Face &face, int x, int y, const AodGlyph *glyph, AlwaysOn::Colour colour )
{
    if ( face.count < AlwaysOn::MaxSprites && glyph->width > 0 ) {
        face.sprites[face.count++] = { static_cast<int16_t>( x ), static_cast<int16_t>( y ), glyph, colour };
    }
}

static void addShape( AlwaysOn::Face &face, AlwaysOn::ShapeKind kind, AlwaysOn::Colour colour, int x0, int y0,
                      int radius = 0, int width = 0, int x1 = 0, int y1 = 0 )
{
    if ( face.shapeCount < AlwaysOn::MaxShapes ) {
        face.shapes[face.shapeCount++] = { static_cast<uint8_t>( kind ), static_cast<uint8_t>( colour ),
                                           static_cast<uint8_t>( radius ), static_cast<uint8_t>( width ),
                                           static_cast<int16_t>( x0 ), static_cast<int16_t>( y0 ),
                                           static_cast<int16_t>( x1 ), static_cast<int16_t>( y1 ) };
    }
}

// The lit part of the Moon and its rim, from the illuminated fraction: the terminator is half an
// ellipse, on the right while the Moon waxes (as seen from the northern hemisphere).
static const AodGlyph *moonGlyph( int percent, bool waxing )
{
    memset( s_moonBits, 0, sizeof( s_moonBits ) );
    const int radius = s_moonRadius;
    const double lit = percent / 100.0;
    for ( int y = 0; y < s_moonSize; ++y ) {
        const double dy = y - radius;
        const double half = std::sqrt( std::fmax( 0.0, radius * radius - dy * dy ) );
        const double terminator = half * ( 1 - 2 * lit );
        for ( int x = 0; x < s_moonSize; ++x ) {
            const double dx = x - radius;
            const double distance = std::sqrt( dx * dx + dy * dy );
            if ( distance > radius + 0.5 ) {
                continue;
            }
            const bool rim = distance > radius - 1.5;
            const bool litSide = waxing ? dx >= terminator : -dx >= terminator;
            if ( rim || litSide ) {
                const int bit = y * s_moonSize + x;
                s_moonBits[bit / 8] |= static_cast<uint8_t>( 0x80 >> ( bit % 8 ) );
            }
        }
    }
    s_moon = { static_cast<int16_t>( s_moonSize ), static_cast<int16_t>( s_moonSize ), s_moonBits };
    return &s_moon;
}

// The small glyph of a code point; null when there is none.
static const AodGlyph *smallGlyph( uint32_t code )
{
    for ( int i = 0; i < AOD_SMALL_COUNT; ++i ) {
        if ( aodSmallCodes[i] == code ) {
            return &aodSmall[i];
        }
    }
    return nullptr;
}

static bool dateValid( const Rtc::DateTime &time )
{
    return time.month >= 1 && time.month <= 12 && time.day >= 1 && time.day <= 31;
}

// The code point at `text[*index]` (UTF-8, up to three bytes: the Turkish letters), moving the
// index past it.
static uint32_t nextCodePoint( const char *text, int *index )
{
    const uint8_t first = static_cast<uint8_t>( text[( *index )++] );
    if ( first < 0x80 ) {
        return first;
    }
    const int extra = first >= 0xE0 ? 2 : 1;
    uint32_t code = first & ( extra == 2 ? 0x0F : 0x1F );
    for ( int i = 0; i < extra && text[*index] != '\0'; ++i ) {
        code = ( code << 6 ) | ( static_cast<uint8_t>( text[( *index )++] ) & 0x3F );
    }
    return code;
}

// `text` in the small glyphs, centred on (centreX, centreY).
static void addSmallText( AlwaysOn::Face &face, const char *text, int centreX, int centreY, AlwaysOn::Colour colour )
{
    int letters = 0;
    for ( int i = 0; text[i] != '\0'; ) {
        nextCodePoint( text, &i );
        ++letters;
    }
    int x = centreX - letters * aodSmallAdvance / 2;
    const int y = centreY - aodSmallHeight / 2;
    for ( int i = 0; text[i] != '\0'; ) {
        const AodGlyph *glyph = smallGlyph( nextCodePoint( text, &i ) );
        if ( glyph != nullptr ) {
            addSprite( face, x, y, glyph, colour );
        }
        x += aodSmallAdvance;
    }
}

// HH:MM of the page's digit style, centred on the Large template's clock line.
static void addClock( AlwaysOn::Face &face, const AodStyle &style, const Rtc::DateTime &time )
{
    // Even, so that the dots of the seven segments fall on the same grid in every minute.
    const int left = ( FaceConfig::CentreX - style.width / 2 ) & ~1;
    const int top = ( FaceConfig::ClockY - style.height / 2 ) & ~1;
    const int digits[4] = { time.hour / 10, time.hour % 10, time.minute / 10, time.minute % 10 };
    const int places[4] = { 0, 1, 3, 4 };
    for ( int i = 0; i < 4; ++i ) {
        addSprite( face, left + style.x[places[i]], top + style.y[places[i]], &style.digits[digits[i]],
                   AlwaysOn::White );
    }
    addSprite( face, left + style.x[2], top + style.y[2], &style.colon, AlwaysOn::White );
}

// The weekday icon: the lit notches (the first n on the n-th day of the week, Monday one) as
// chords of the ring, and the day's first letter.
static void addWeekday( AlwaysOn::Face &face, int cx, int cy, int weekday )
{
    const float step = 360.0f / 7;
    for ( int i = 0; i <= weekday; ++i ) {
        // Clockwise from 12 o'clock.
        const float a0 = ( 270 + i * step + s_notchGap / 2 ) * static_cast<float>( M_PI ) / 180.0f;
        const float a1 = ( 270 + ( i + 1 ) * step - s_notchGap / 2 ) * static_cast<float>( M_PI ) / 180.0f;
        addShape( face, AlwaysOn::LineShape, AlwaysOn::White, static_cast<int>( lroundf( cx + s_notchRadius * cosf( a0 ) ) ),
                  static_cast<int>( lroundf( cy + s_notchRadius * sinf( a0 ) ) ), 0, 3,
                  static_cast<int>( lroundf( cx + s_notchRadius * cosf( a1 ) ) ),
                  static_cast<int>( lroundf( cy + s_notchRadius * sinf( a1 ) ) ) );
    }
    const char *name = Language::shortDay( weekday );
    char letter[8];
    snprintf( letter, sizeof( letter ), "%.*s", FaceText::fit( name, 1, 1 ), name );
    addSmallText( face, letter, cx, cy, AlwaysOn::White );
}

static bool usesItem( const FaceConfig::Page &page, int textItem, int cornerItem )
{
    for ( int slot = 0; slot < FaceConfig::SlotCount; ++slot ) {
        const int wanted = slot < FaceConfig::TextSlotCount ? textItem : cornerItem;
        if ( FaceConfig::alwaysOnSlot( page, slot ) && page.items[slot] == wanted ) {
            return true;
        }
    }
    return false;
}

const AlwaysOn::Face &AlwaysOn::layout( const Look &look, const Rtc::DateTime &time, int64_t utcNow, int batteryPercent )
{
    Language::set( look.language );
    Face &face = s_face;
    face.count = 0;
    face.shapeCount = 0;
    face.left = s_shift;
    face.top = s_shift;
    face.width = Panel::WIDTH - 2 * s_shift;
    face.height = Panel::HEIGHT - 2 * s_shift;
    const FaceConfig::Page &page = look.page;
    if ( page.clock == FaceConfig::DigitalClock && page.digital < AOD_STYLE_COUNT ) {
        addClock( face, aodStyles[page.digital], time );
    }

    FaceText::Inputs in = {};
    in.timeValid = dateValid( time );
    in.year = time.year;
    in.month = time.month;
    in.day = time.day;
    in.hour = time.hour;
    in.minute = time.minute;
    in.second = time.second;
    in.weekday = in.timeValid ? Dates::weekday( Dates::dayNumber( time.year, time.month, time.day ) ) : 0;
    in.skyValid = look.skyValid;
    in.now = utcNow;
    in.table = &look.table;
    in.batteryPercent = batteryPercent;
    in.altitudesValid = look.skyValid;
    // The altitudes cost a few milliseconds each: only for a slot that shows them.
    if ( look.skyValid && usesItem( page, FaceConfig::SunAltitude, FaceConfig::SunAltitudeCorner ) ) {
        in.sunAltitude = Astronomy::sunPosition( utcNow, look.observer ).altitude;
    }
    if ( look.skyValid && usesItem( page, FaceConfig::MoonAltitude, FaceConfig::MoonAltitudeCorner ) ) {
        in.moonAltitude = Astronomy::moonPosition( utcNow, look.observer ).altitude;
    }

    char text[64];
    for ( int slot = 0; slot < FaceConfig::TextSlotCount; ++slot ) {
        if ( !FaceConfig::alwaysOnSlot( page, slot ) ) {
            continue;
        }
        const int width = FaceConfig::textWidth( page, slot );
        FaceText::text( static_cast<FaceConfig::TextItem>( page.items[slot] ), page.parameters[slot], in, text,
                        sizeof( text ), width / aodSmallAdvance );
        // Cut as on the active face; a space left at the end would move the text aside.
        int bytes = FaceText::fit( text, aodSmallAdvance, width );
        while ( bytes > 0 && text[bytes - 1] == ' ' ) {
            --bytes;
        }
        text[bytes] = '\0';
        addSmallText( face, text, FaceConfig::CentreX, FaceConfig::TextY[slot], White );
    }
    for ( int i = 0; i < 4; ++i ) {
        const int slot = FaceConfig::TopLeft + i;
        if ( !FaceConfig::alwaysOnSlot( page, slot ) ) {
            continue;
        }
        const int x = FaceConfig::CornerX[i];
        const int y = FaceConfig::CornerY[i];
        const int item = page.items[slot];
        if ( item == FaceConfig::MoonIcon ) {
            if ( look.skyValid ) {
                // White: yellow read as the Sun's state.
                addSprite( face, x - s_moonRadius, y - s_moonRadius, moonGlyph( look.moonPercent, look.waxing ), White );
            }
        } else if ( item == FaceConfig::WeekdayIcon ) {
            if ( in.timeValid ) {
                addWeekday( face, x, y, in.weekday );
            }
        } else {
            FaceText::corner( static_cast<FaceConfig::CornerItem>( item ), in, text, sizeof( text ) );
            addSmallText( face, text, x, y, item == FaceConfig::SunAltitudeCorner ? Yellow : White );
        }
    }
    return face;
}

int AlwaysOn::gaugeColour( Corners::Kind kind )
{
    // By Corners::Kind; orange and the Moon's dark grey have no idle-mode colour.
    static const int colours[] = { -1, Yellow, Blue, Green, Yellow, Red, White, White, White };
    return colours[kind];
}

// The colour of pixel (x, y) on a corner gauge, white on a notch (2 x 2 pixels at both ends and
// in the middle), 0 where nothing is lit; the ring's pixels come from main/CornerTable.c. The Moon
// below the horizon is dotted.
static uint16_t ringPixel( const Corners::Gauge *gauges, int x, int y )
{
    int corner = 0;
    int along = 0;
    const CornerPixel *pixel = cornerTablePixel( Panel::WIDTH, Panel::HEIGHT, x, y, &corner, &along );
    if ( pixel == nullptr ) {
        return 0;
    }
    if ( pixel->flags & CORNER_AOD_NOTCH ) {
        return s_colours[AlwaysOn::White];
    }
    const Corners::Gauge &gauge = gauges[corner];
    const int colour = AlwaysOn::gaugeColour( gauge.kind );
    if ( !( pixel->flags & CORNER_AOD_RING ) || colour < 0 ) {
        return 0;
    }
    if ( along >= gauge.start * CORNER_ALONG_SCALE && along <= gauge.end * CORNER_ALONG_SCALE ) {
        return ( gauge.kind == Corners::MoonDown && ( ( x + y ) & 3 ) != 0 ) ? 0 : s_colours[colour];
    }
    // While the battery charges, the part above its value is white (CornerView animates it).
    return ( gauge.charging && along > gauge.end * CORNER_ALONG_SCALE ) ? s_colours[AlwaysOn::White] : 0;
}

uint16_t AlwaysOn::cornerPixel( const Corners::Gauge gauges[Corners::CornerCount], int x, int y )
{
    return ringPixel( gauges, x, y );
}

// Draws the part of `glyph` (top-left corner at x, y on the screen) that falls into the band;
// `halftone` lights only every second pixel (AlwaysOn::Grey).
static void drawGlyph( uint16_t *pixels, int bandTop, int rows, int left, int width, const AodGlyph &glyph, int x,
                       int y, uint16_t colour, bool halftone = false )
{
    const int firstRow = y > bandTop ? y : bandTop;
    const int endRow = ( y + glyph.height < bandTop + rows ) ? y + glyph.height : bandTop + rows;
    for ( int row = firstRow; row < endRow; ++row ) {
        const int firstBit = ( row - y ) * glyph.width;
        uint16_t *line = pixels + ( row - bandTop ) * width;
        for ( int column = 0; column < glyph.width; ++column ) {
            const int bit = firstBit + column;
            const int screenX = x + column;
            if ( halftone && ( screenX + row ) % 2 != 0 ) {
                continue;
            }
            if ( ( glyph.bits[bit / 8] & ( 0x80 >> ( bit % 8 ) ) ) && screenX >= left && screenX < left + width ) {
                line[screenX - left] = colour;
            }
        }
    }
}

// Sets pixel (x, y) of the band when it lies in the drawn rectangle.
static inline void putPixel( uint16_t *pixels, int bandTop, int rows, int left, int width, int x, int y, uint16_t colour )
{
    if ( x >= left && x < left + width && y >= bandTop && y < bandTop + rows ) {
        pixels[( y - bandTop ) * width + ( x - left )] = colour;
    }
}

// Draws the part of a shape (moved by shiftX, shiftY) that falls into the band.
static void drawShape( uint16_t *pixels, int bandTop, int rows, int left, int width, const AlwaysOn::Shape &shape,
                       int shiftX, int shiftY )
{
    const uint16_t colour = s_colours[shape.colour];
    const int x0 = shiftX + shape.x0;
    const int y0 = shiftY + shape.y0;
    switch ( shape.kind ) {
    case AlwaysOn::DotShape:
        for ( int dy = 0; dy < 2; ++dy ) {
            for ( int dx = 0; dx < 2; ++dx ) {
                putPixel( pixels, bandTop, rows, left, width, x0 + dx, y0 + dy, colour );
            }
        }
        break;
    case AlwaysOn::DiscShape:
    case AlwaysOn::RingShape: {
        const int r = shape.radius;
        if ( y0 + r < bandTop || y0 - r >= bandTop + rows ) {
            break;
        }
        const int inner = shape.kind == AlwaysOn::RingShape ? r - shape.width : -1;
        for ( int y = y0 - r; y <= y0 + r; ++y ) {
            if ( y < bandTop || y >= bandTop + rows ) {
                continue;
            }
            for ( int x = x0 - r; x <= x0 + r; ++x ) {
                const int d = ( x - x0 ) * ( x - x0 ) + ( y - y0 ) * ( y - y0 );
                if ( d <= r * r && ( inner < 0 || d > inner * inner ) ) {
                    putPixel( pixels, bandTop, rows, left, width, x, y, colour );
                }
            }
        }
        break;
    }
    case AlwaysOn::LineShape: {
        const int x1 = shiftX + shape.x1;
        const int y1 = shiftY + shape.y1;
        const int half = shape.width / 2;
        const int top = ( y0 < y1 ? y0 : y1 ) - half;
        const int bottom = ( y0 > y1 ? y0 : y1 ) + half + 1;
        if ( bottom < bandTop || top >= bandTop + rows ) {
            break;
        }
        // Squares of the line's width stamped every half pixel along it.
        const int steps = 2 * ( std::abs( x1 - x0 ) > std::abs( y1 - y0 ) ? std::abs( x1 - x0 ) : std::abs( y1 - y0 ) ) + 1;
        for ( int k = 0; k <= steps; ++k ) {
            const int x = x0 + ( x1 - x0 ) * k / steps - half;
            const int y = y0 + ( y1 - y0 ) * k / steps - half;
            for ( int dy = 0; dy < shape.width; ++dy ) {
                for ( int dx = 0; dx < shape.width; ++dx ) {
                    putPixel( pixels, bandTop, rows, left, width, x + dx, y + dy, colour );
                }
            }
        }
        break;
    }
    default:
        break;
    }
}

// Draws the rectangle (even position and size) band by band: black, with the face's bitmaps and
// shapes moved by (shiftX, shiftY) and the corner gauges where they fall into it.
static void render( Panel &panel, const AlwaysOn::Face &face, int shiftX, int shiftY, int left, int top, int width,
                    int height, const Corners::Gauge *gauges )
{
    uint16_t *pixels = Panel::buffer();
    const int bandRows = ( Panel::BufferPixels / width ) & ~1;
    for ( int bandTop = top; bandTop < top + height; bandTop += bandRows ) {
        const int rows = ( top + height - bandTop < bandRows ) ? top + height - bandTop : bandRows;
        memset( pixels, 0, sizeof( uint16_t ) * rows * width );
        for ( int i = 0; i < face.count; ++i ) {
            const AlwaysOn::Sprite &sprite = face.sprites[i];
            drawGlyph( pixels, bandTop, rows, left, width, *sprite.glyph, shiftX + sprite.x, shiftY + sprite.y,
                       s_colours[sprite.colour], sprite.colour == AlwaysOn::Grey );
        }
        for ( int i = 0; i < face.shapeCount; ++i ) {
            drawShape( pixels, bandTop, rows, left, width, face.shapes[i], shiftX, shiftY );
        }
        for ( int row = bandTop; row < bandTop + rows; ++row ) {
            if ( row >= s_cornerBox && row < Panel::HEIGHT - s_cornerBox ) {
                continue;
            }
            uint16_t *line = pixels + ( row - bandTop ) * width;
            for ( int x = left; x < left + width; ++x ) {
                if ( x >= s_cornerBox && x < Panel::WIDTH - s_cornerBox ) {
                    continue;
                }
                const uint16_t colour = ringPixel( gauges, x, row );
                if ( colour != 0 ) {
                    line[x - left] = colour;
                }
            }
        }
        panel.draw( left, bandTop, width, rows, pixels );
    }
}

// The corner box of gauge `corner` on the screen.
static void cornerBox( int corner, int &x, int &y )
{
    x = ( corner == Corners::TopLeft || corner == Corners::BottomLeft ) ? 0 : Panel::WIDTH - s_cornerBox;
    y = ( corner == Corners::TopLeft || corner == Corners::TopRight ) ? 0 : Panel::HEIGHT - s_cornerBox;
}

#if CONFIG_WATCH_AOD_CYCLE_MIN > 0
int AlwaysOn::cycleStyle( uint32_t seconds )
{
    // Blank for an hour, then the face and the blank face in turn. Run V's result (no visible cost
    // of lit pixels) does not depend on what the face shows.
    const uint32_t settleS = 3600;
    if ( seconds < settleS ) {
        return -1;
    }
    return ( ( seconds - settleS ) / ( CONFIG_WATCH_AOD_CYCLE_MIN * 60 ) ) % 2 == 0 ? 0 : -1;
}
#endif

#if CONFIG_WATCH_DEV_LOGGING
void AlwaysOn::capture( const Look &look, int page )
{
    const Rtc::DateTime time = { 2026, 6, 21, 19, 19, 0 };
    const int64_t utcNow = static_cast<int64_t>( Dates::dayNumber( 2026, 6, 21 ) ) * 86400 + ( 19 * 60 + 19 - 180 ) * 60;
    const Face &face = layout( look, time, utcNow, 92 );
    // The scene's gauges: 91 % of the day, the battery at 92 %, the Moon 55 % through its pass
    // and 45 % lit, waxing.
    const Corners::Gauge gauges[Corners::CornerCount] = {
        { 0, 910, Corners::Day, false }, { 0, 920, Corners::BatteryHigh, false }, { 0, 546, Corners::MoonUp, false },
        { 0, 450, Corners::MoonLit, false } };
    Panel panel;
    panel.record();
    printf( "SHOT BEGIN -1 %d %d always-on-%d\n", Panel::WIDTH, Panel::HEIGHT, page );
    panel.clear();
    render( panel, face, 0, 0, face.left, face.top, face.width, face.height, gauges );
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        int x = 0;
        int y = 0;
        cornerBox( i, x, y );
        render( panel, face, 0, 0, x, y, s_cornerBox, s_cornerBox, gauges );
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

uint64_t AlwaysOn::update( const Rtc::DateTime &time, int64_t utcNow, bool timeRead, int batteryPercent,
                           bool charging )
{
    // A failed or corrupt read must not turn into a long sleep: try again in a minute.
    if ( !timeRead || time.hour > 23 || time.minute > 59 || time.second > 59 || !dateValid( time ) ) {
        if ( ++s_failedMinutes >= s_maxFailedMinutes ) {
            s_active = false;
            return 0;
        }
        return s_retryUs;
    }
    s_failedMinutes = 0;

    // A wake up to 2 s early (RC timer drift) already shows the coming minute, with its date
    // (just before midnight, the next day's).
    Rtc::DateTime shown = time;
    int64_t shownUtc = utcNow;
    int secondsToNext = 60 - time.second;
    if ( time.second >= 58 ) {
        shown = Rtc::addMinutes( time, 1 );
        shown.second = 0;
        shownUtc = utcNow + secondsToNext;
        secondsToNext += 60;
    }
    const int minuteOfDay = shown.hour * 60 + shown.minute;

    Panel panel;
    if ( !panel.open() ) {
        s_active = false;
        return 0;
    }
    // A new local day (always-on past midnight): the slots' events of today and tomorrow, without
    // the sky day.
    if ( s_look.skyValid ) {
        const int day = Dates::dayNumber( shown.year, shown.month, shown.day );
        const int64_t localSeconds = static_cast<int64_t>( day ) * 86400 + shown.hour * 3600 + shown.minute * 60;
        s_look.table.follow( day, static_cast<int>( ( localSeconds - shownUtc ) / 60 ), s_look.observer,
                             s_look.ishaAngle );
    }
    const Look &look = s_look;
    Corners::Gauge gauges[Corners::CornerCount] = {};
    Corners::compute( look.corners, utcNow, batteryPercent, charging, gauges );
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

    const Face &face = layout( look, shown, shownUtc, batteryPercent );
#if CONFIG_WATCH_AOD_CYCLE_MIN > 0
    // System time runs from power-on through deep sleep, as in the battery samples.
    timeval now = {};
    gettimeofday( &now, nullptr );
    if ( cycleStyle( static_cast<uint32_t>( now.tv_sec ) ) < 0 ) {
        // Battery run V: cleared like any other face, with nothing drawn.
        Face &cleared = const_cast<Face &>( face );
        cleared.count = 0;
        cleared.shapeCount = 0;
    }
#endif
    // Burn-in: the face moves within +-s_shift pixels, one step per minute.
    const int shiftX = ( minuteOfDay % 5 - 2 ) * s_shift / 2;
    const int shiftY = ( ( minuteOfDay / 5 ) % 5 - 2 ) * s_shift / 2;
    const int faceX = face.left + shiftX;
    const int faceY = face.top + shiftY;
    // One rectangle covers the old face and the new one, so that the old one is cleared too.
    int left = faceX;
    int top = faceY;
    int right = faceX + face.width;
    int bottom = faceY + face.height;
    if ( s_drawnWidth > 0 ) {
        left = s_drawnX < left ? s_drawnX : left;
        top = s_drawnY < top ? s_drawnY : top;
        right = s_drawnX + s_drawnWidth > right ? s_drawnX + s_drawnWidth : right;
        bottom = s_drawnY + s_drawnHeight > bottom ? s_drawnY + s_drawnHeight : bottom;
    }
    render( panel, face, shiftX, shiftY, left, top, right - left, bottom - top, gauges );
    s_drawnX = faceX;
    s_drawnY = faceY;
    s_drawnWidth = face.width;
    s_drawnHeight = face.height;
    // The corners stay in place (no burn-in shift: they lie against the edge); one is drawn again
    // when its gauge changed by a pixel.
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        const int drawn[4] = { gauges[i].start * s_gaugePixels / 1000, gauges[i].end * s_gaugePixels / 1000,
                               gauges[i].kind, gauges[i].charging ? 1 : 0 };
        if ( drawn[0] == s_drawnGauges[i][0] && drawn[1] == s_drawnGauges[i][1] && drawn[2] == s_drawnGauges[i][2] &&
             drawn[3] == s_drawnGauges[i][3] ) {
            continue;
        }
        int x = 0;
        int y = 0;
        cornerBox( i, x, y );
        render( panel, face, shiftX, shiftY, x, y, s_cornerBox, s_cornerBox, gauges );
        for ( int k = 0; k < 4; ++k ) {
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
