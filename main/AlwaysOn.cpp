#include "AlwaysOn.h"
#include "Language.h"

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/time.h>

#include "CornerTable.h"
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

static const int s_moonRadius = 18;  // the corner moon
static const int s_moonSize = 2 * s_moonRadius + 1;
static const int s_skyMoonRadius = 10;  // the moon on its path (sky styles)
static const int s_itemsGap = 24;  // between the time and the date


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

static void addSprite( AlwaysOn::Face &face, int x, int y, const AodGlyph *glyph, const lv_font_t *font, uint32_t code,
                       AlwaysOn::Colour colour )
{
    if ( face.count < AlwaysOn::MaxSprites && glyph->width > 0 ) {
        face.sprites[face.count++] = { x, y, glyph, font, code, colour };
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
// ellipse, on the right while the Moon waxes (as seen from the northern hemisphere). Dotted:
// every second pixel of every second row, as the dotted glyphs.
static const AodGlyph *moonGlyph( int radius, int percent, bool waxing, bool dotted )
{
    memset( s_moonBits, 0, sizeof( s_moonBits ) );
    const int size = 2 * radius + 1;
    const double lit = percent / 100.0;
    for ( int y = 0; y < size; ++y ) {
        const double dy = y - radius;
        const double half = std::sqrt( std::fmax( 0.0, radius * radius - dy * dy ) );
        const double terminator = half * ( 1 - 2 * lit );
        for ( int x = 0; x < size; ++x ) {
            const double dx = x - radius;
            const double distance = std::sqrt( dx * dx + dy * dy );
            if ( distance > radius + 0.5 ) {
                continue;
            }
            const bool rim = distance > radius - 1.5;
            const bool litSide = waxing ? dx >= terminator : -dx >= terminator;
            const bool onGrid = !dotted || ( ( x | y ) & 1 ) == 0;
            if ( ( rim || litSide ) && onGrid ) {
                const int bit = y * size + x;
                s_moonBits[bit / 8] |= static_cast<uint8_t>( 0x80 >> ( bit % 8 ) );
            }
        }
    }
    s_moon = { static_cast<int16_t>( size ), static_cast<int16_t>( size ), s_moonBits };
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
    if ( !( look.items & MoonItem ) || look.style >= ArcsStyle ) {
        return nullptr;
    }
    return moonGlyph( s_moonRadius, look.moonPercent, look.waxing, aodStyles[look.style].dottedItems );
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

// Letters, not bytes: the width of a text in the small glyphs.
static int smallTextWidth( const char *text )
{
    int count = 0;
    for ( int i = 0; text[i] != '\0'; ) {
        nextCodePoint( text, &i );
        ++count;
    }
    return count * aodSmallAdvance;
}

// `text` in the small glyphs (solid or dotted), from `x` with its top at `y` (face coordinates).
static void addSmallGlyphs( AlwaysOn::Face &face, const char *text, int x, int y, bool dotted, AlwaysOn::Colour colour )
{
    const lv_font_t *font = dotted ? &aodSmallDottedFont : &aodSmallFont;
    for ( int i = 0; text[i] != '\0'; ) {
        const uint32_t code = nextCodePoint( text, &i );
        const AodGlyph *glyph = smallGlyph( code, dotted );
        if ( glyph != nullptr ) {
            addSprite( face, x, y, glyph, font, code, colour );
        }
        x += aodSmallAdvance;
    }
}

// `text` in the small glyphs, centred on `centreX` with its top at `y` (face coordinates).
static void addSmallText( AlwaysOn::Face &face, const char *text, int centreX, int y, AlwaysOn::Colour colour )
{
    addSmallGlyphs( face, text, centreX - smallTextWidth( text ) / 2, y, false, colour );
}

// The date as "21 June", centred on `centreX` with its top at `y` (face coordinates).
static void addLongDate( AlwaysOn::Face &face, const Rtc::DateTime &time, int centreX, int y,
                         AlwaysOn::Colour colour )
{
    if ( !dateValid( time ) ) {
        return;
    }
    char date[32];
    Language::longDate( date, sizeof( date ), -1, time.day, time.month );
    addSmallText( face, date, centreX, y, colour );
}

// Dots every `step` pixels along the circle of `radius` around (cx, cy) from angle a0 to a1
// (degrees, clockwise from 3 o'clock), leaving out those within `clear` of the points in
// `keepOut` (the Sun and the Moon on the path).
static void addDottedArc( AlwaysOn::Face &face, AlwaysOn::Colour colour, float cx, float cy, float radius, float a0,
                          float a1, float step, const float keepOut[][2], int keepOutCount, float clear )
{
    const float length = static_cast<float>( M_PI ) / 180.0f * ( a1 - a0 ) * radius;
    const int count = static_cast<int>( length / step );
    for ( int k = 0; k <= count; ++k ) {
        const float angle = ( a0 + ( a1 - a0 ) * k / ( count > 0 ? count : 1 ) ) * static_cast<float>( M_PI ) / 180.0f;
        const float x = cx + radius * cosf( angle );
        const float y = cy + radius * sinf( angle );
        bool free = true;
        for ( int i = 0; i < keepOutCount; ++i ) {
            const float dx = x - keepOut[i][0];
            const float dy = y - keepOut[i][1];
            free = free && dx * dx + dy * dy > clear * clear;
        }
        if ( free ) {
            addShape( face, AlwaysOn::DotShape, colour, static_cast<int>( lroundf( x ) ), static_cast<int>( lroundf( y ) ) );
        }
    }
}

// The sky arcs: the Sun's and the Moon's paths over the horizon (dotted), each body placed by the
// time between its rise and its set, the time and the date under them. Face coordinates are the
// screen's moved by the face's place (s_arcs*).
static const int s_arcsX = 14;
static const int s_arcsY = 42;
static const int s_arcsWidth = 382;
static const int s_arcsHeight = 404;

static void layoutArcs( AlwaysOn::Face &face, const AlwaysOn::Look &look, const Rtc::DateTime &time, int64_t utcNow )
{
    face.x = s_arcsX;
    face.y = s_arcsY;
    face.width = s_arcsWidth;
    face.height = s_arcsHeight;
    const float cx = 205 - s_arcsX;
    const float cy = 236 - s_arcsY;
    const float sunRadius = 180;
    const float moonRadius = 132;

    float bodies[2][2] = {};
    int bodyCount = 0;
    Corners::Pass sun = {};
    const bool sunUp = Corners::pass( look.corners.sun, utcNow, sun ) && sun.up;
    if ( sunUp ) {
        const float f = static_cast<float>( utcNow - sun.upStart ) / static_cast<float>( sun.upEnd - sun.upStart );
        const float angle = ( 180 + 180 * f ) * static_cast<float>( M_PI ) / 180.0f;
        bodies[bodyCount][0] = cx + sunRadius * cosf( angle );
        bodies[bodyCount][1] = cy + sunRadius * sinf( angle );
        addShape( face, AlwaysOn::DiscShape, AlwaysOn::Yellow, static_cast<int>( bodies[bodyCount][0] ),
                  static_cast<int>( bodies[bodyCount][1] ), 9 );
        ++bodyCount;
    }
    Corners::Pass moon = {};
    if ( ( look.items & AlwaysOn::MoonItem ) && Corners::pass( look.corners.moon, utcNow, moon ) && moon.up ) {
        const float f = static_cast<float>( utcNow - moon.upStart ) / static_cast<float>( moon.upEnd - moon.upStart );
        const float angle = ( 180 + 180 * f ) * static_cast<float>( M_PI ) / 180.0f;
        bodies[bodyCount][0] = cx + moonRadius * cosf( angle );
        bodies[bodyCount][1] = cy + moonRadius * sinf( angle );
        addSprite( face, static_cast<int>( bodies[bodyCount][0] ) - s_skyMoonRadius,
                   static_cast<int>( bodies[bodyCount][1] ) - s_skyMoonRadius,
                   moonGlyph( s_skyMoonRadius, look.moonPercent, look.waxing, false ), nullptr, 0, AlwaysOn::White );
        ++bodyCount;
    }
    addDottedArc( face, AlwaysOn::Yellow, cx, cy, sunRadius, 180, 360, 6, bodies, bodyCount, 14 );
    addDottedArc( face, AlwaysOn::White, cx, cy, moonRadius, 180, 360, 6, bodies, bodyCount, 14 );
    for ( int x = 16 - s_arcsX; x <= 394 - s_arcsX; x += 6 ) {
        addShape( face, AlwaysOn::DotShape, AlwaysOn::White, x, static_cast<int>( cy ) );
    }

    const AodStyle &style = aodStyles[AlwaysOn::ArcsStyle];
    const int timeX = ( 410 - style.width ) / 2 - s_arcsX;
    const int timeY = 262 - s_arcsY;
    const int digits[4] = { time.hour / 10, time.hour % 10, time.minute / 10, time.minute % 10 };
    const int slots[4] = { 0, 1, 3, 4 };
    for ( int i = 0; i < 4; ++i ) {
        addSprite( face, timeX + style.x[slots[i]], timeY + style.y[slots[i]], &style.digits[digits[i]], style.font,
                   static_cast<uint32_t>( '0' + digits[i] ), AlwaysOn::White );
    }
    addSprite( face, timeX + style.x[2], timeY + style.y[2], &style.colon, style.font, ':', AlwaysOn::White );
    if ( look.items & AlwaysOn::DateItem ) {
        addLongDate( face, time, 205 - s_arcsX, 410 - s_arcsY, AlwaysOn::White );
    }
}

// The dial: an analog clock (twelve ticks, hour and minute hands) between the Sun's orbit and
// the Moon's. Each orbit is dotted densely where the body is above the horizon (an arc centred at
// the top, as long as its share of the pass) and sparsely below, with marks at rise and set; the
// body moves clockwise along it, solid above the horizon and hollow below.
static const int s_dialX = 12;
static const int s_dialY = 12;
static const int s_dialWidth = 386;
static const int s_dialHeight = 480;

static void addOrbit( AlwaysOn::Face &face, AlwaysOn::Colour colour, float cx, float cy, float radius,
                      const Corners::Events &events, int64_t utcNow, float bodyX[1], float bodyY[1], bool &up,
                      bool &known )
{
    Corners::Pass pass = {};
    known = Corners::pass( events, utcNow, pass );
    float rise = 180;
    float set = 360;
    up = false;
    if ( known ) {
        const float span = 360.0f * static_cast<float>( pass.upEnd - pass.upStart ) /
                           static_cast<float>( pass.downEnd - pass.upStart );
        rise = 270 - span / 2;
        set = 270 + span / 2;
        up = pass.up;
        float angle = 0;
        if ( up ) {
            angle = rise + ( set - rise ) * static_cast<float>( utcNow - pass.upStart ) /
                               static_cast<float>( pass.upEnd - pass.upStart );
        } else {
            angle = set + ( rise + 360 - set ) * static_cast<float>( utcNow - pass.upEnd ) /
                              static_cast<float>( pass.downEnd - pass.upEnd );
        }
        const float a = angle * static_cast<float>( M_PI ) / 180.0f;
        bodyX[0] = cx + radius * cosf( a );
        bodyY[0] = cy + radius * sinf( a );
    }
    const float keepOut[1][2] = { { bodyX[0], bodyY[0] } };
    const int keepOutCount = known ? 1 : 0;
    addDottedArc( face, colour, cx, cy, radius, rise, set, 5, keepOut, keepOutCount, 14 );
    addDottedArc( face, colour, cx, cy, radius, set, rise + 360, 15, keepOut, keepOutCount, 14 );
    for ( int i = 0; i < 2 && known; ++i ) {
        const float a = ( i == 0 ? rise : set ) * static_cast<float>( M_PI ) / 180.0f;
        addShape( face, AlwaysOn::LineShape, colour, static_cast<int>( lroundf( cx + ( radius - 6 ) * cosf( a ) ) ),
                  static_cast<int>( lroundf( cy + ( radius - 6 ) * sinf( a ) ) ), 0, 2,
                  static_cast<int>( lroundf( cx + ( radius + 6 ) * cosf( a ) ) ),
                  static_cast<int>( lroundf( cy + ( radius + 6 ) * sinf( a ) ) ) );
    }
}

static void layoutDial( AlwaysOn::Face &face, const AlwaysOn::Look &look, const Rtc::DateTime &time, int64_t utcNow )
{
    face.x = s_dialX;
    face.y = s_dialY;
    face.width = s_dialWidth;
    face.height = s_dialHeight;
    const float cx = 205 - s_dialX;
    const float cy = 243 - s_dialY;

    float sunX[1] = {};
    float sunY[1] = {};
    bool sunUp = false;
    bool sunKnown = false;
    addOrbit( face, AlwaysOn::Yellow, cx, cy, 182, look.corners.sun, utcNow, sunX, sunY, sunUp, sunKnown );
    if ( sunKnown ) {
        addShape( face, sunUp ? AlwaysOn::DiscShape : AlwaysOn::RingShape, AlwaysOn::Yellow, static_cast<int>( sunX[0] ),
                  static_cast<int>( sunY[0] ), 9, 2 );
    }
    if ( look.items & AlwaysOn::MoonItem ) {
        float moonX[1] = {};
        float moonY[1] = {};
        bool moonUp = false;
        bool moonKnown = false;
        addOrbit( face, AlwaysOn::White, cx, cy, 140, look.corners.moon, utcNow, moonX, moonY, moonUp, moonKnown );
        if ( moonKnown && moonUp ) {
            addSprite( face, static_cast<int>( moonX[0] ) - s_skyMoonRadius, static_cast<int>( moonY[0] ) - s_skyMoonRadius,
                       moonGlyph( s_skyMoonRadius, look.moonPercent, look.waxing, false ), nullptr, 0, AlwaysOn::White );
        } else if ( moonKnown ) {
            addShape( face, AlwaysOn::RingShape, AlwaysOn::White, static_cast<int>( moonX[0] ), static_cast<int>( moonY[0] ),
                      s_skyMoonRadius, 2 );
        }
    }
    for ( int i = 0; i < 12; ++i ) {
        const float a = ( i * 30 - 90 ) * static_cast<float>( M_PI ) / 180.0f;
        const float inner = i % 3 == 0 ? 100 : 108;
        addShape( face, AlwaysOn::LineShape, AlwaysOn::White, static_cast<int>( lroundf( cx + inner * cosf( a ) ) ),
                  static_cast<int>( lroundf( cy + inner * sinf( a ) ) ), 0, i % 3 == 0 ? 2 : 1,
                  static_cast<int>( lroundf( cx + 122 * cosf( a ) ) ), static_cast<int>( lroundf( cy + 122 * sinf( a ) ) ) );
    }
    const float hour = ( ( time.hour % 12 ) + time.minute / 60.0f ) * 30 - 90;
    const float minute = time.minute * 6.0f - 90;
    const float hands[2][3] = { { hour, 58, 5 }, { minute, 92, 3 } };
    for ( int i = 0; i < 2; ++i ) {
        const float a = hands[i][0] * static_cast<float>( M_PI ) / 180.0f;
        addShape( face, AlwaysOn::LineShape, AlwaysOn::White, static_cast<int>( cx ), static_cast<int>( cy ), 0,
                  static_cast<int>( hands[i][2] ), static_cast<int>( lroundf( cx + hands[i][1] * cosf( a ) ) ),
                  static_cast<int>( lroundf( cy + hands[i][1] * sinf( a ) ) ) );
    }
    addShape( face, AlwaysOn::DiscShape, AlwaysOn::White, static_cast<int>( cx ), static_cast<int>( cy ), 4 );
    if ( look.items & AlwaysOn::DateItem ) {
        addLongDate( face, time, 205 - s_dialX, 16 - s_dialY, AlwaysOn::Grey );
    }
    // The time also in digits, in the space below the dial, as far from the bottom edge as the
    // date is from the top.
    char digits[8];
    snprintf( digits, sizeof( digits ), "%02d:%02d", time.hour % 24, time.minute % 60 );
    addSmallText( face, digits, 205 - s_dialX, 456 - s_dialY, AlwaysOn::Grey );
}

const AlwaysOn::Face &AlwaysOn::layout( const Look &look, const Rtc::DateTime &time, int64_t utcNow )
{
    Language::set( look.language );
    s_face.count = 0;
    s_face.shapeCount = 0;
    s_face.x = -1;
    s_face.y = -1;
    if ( look.style == ArcsStyle ) {
        layoutArcs( s_face, look, time, utcNow );
        return s_face;
    }
    if ( look.style == DialStyle ) {
        layoutDial( s_face, look, time, utcNow );
        return s_face;
    }
    Face &face = s_face;
    const AodStyle &style = aodStyles[look.style];
    const bool dotted = style.dottedItems;
    const bool showDate = look.items & DateItem;

    char date[16] = "";
    // A time not read yet (all zero) has no weekday: the name table must not be left.
    if ( showDate && dateValid( time ) ) {
        // Monday first, as Language counts.
        const int weekday = ( ( daysFromCivil( time.year, time.month, time.day ) + 3 ) % 7 + 7 ) % 7;
        snprintf( date, sizeof( date ), "%s %d", Language::shortDay( weekday ), time.day );
    }
    const int dateWidth = smallTextWidth( date );
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
        addSmallGlyphs( face, date, ( face.width - dateWidth ) / 2, y, dotted, White );
        y += dateHeight;
    }
    face.height = y;
    return s_face;
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

// Draws the part of a shape (face coordinates moved by faceX, faceY) that falls into the band.
static void drawShape( uint16_t *pixels, int bandTop, int rows, int left, int width, const AlwaysOn::Shape &shape,
                       int faceX, int faceY )
{
    const uint16_t colour = s_colours[shape.colour];
    const int x0 = faceX + shape.x0;
    const int y0 = faceY + shape.y0;
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
        const int x1 = faceX + shape.x1;
        const int y1 = faceY + shape.y1;
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
// shapes, the moon (null: none) at its fixed place and the corner gauges (when given) where they
// fall into it.
static void render( Panel &panel, const AlwaysOn::Face &face, int faceX, int faceY, int left, int top, int width,
                    int height, const AodGlyph *moon, const Corners::Gauge *gauges )
{
    uint16_t *pixels = Panel::buffer();
    const int bandRows = ( Panel::BufferPixels / width ) & ~1;
    for ( int bandTop = top; bandTop < top + height; bandTop += bandRows ) {
        const int rows = ( top + height - bandTop < bandRows ) ? top + height - bandTop : bandRows;
        memset( pixels, 0, sizeof( uint16_t ) * rows * width );
        for ( int i = 0; i < face.count; ++i ) {
            const AlwaysOn::Sprite &sprite = face.sprites[i];
            drawGlyph( pixels, bandTop, rows, left, width, *sprite.glyph, faceX + sprite.x, faceY + sprite.y,
                       s_colours[sprite.colour], sprite.colour == AlwaysOn::Grey );
        }
        for ( int i = 0; i < face.shapeCount; ++i ) {
            drawShape( pixels, bandTop, rows, left, width, face.shapes[i], faceX, faceY );
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
                const uint16_t colour = ringPixel( gauges, x, row );
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
    return ( style >= 0 && style < AlwaysOn::StyleCount ) ? style : 0;
}

#if CONFIG_WATCH_DEV_LOGGING
void AlwaysOn::capture( int style )
{
    // The scene's Sun and Moon (Istanbul, UTC+3; times as on the Sun and Moon pages): yesterday's
    // sunset, sunrise 05:32, sunset 20:40, tomorrow's sunrise; moonset 00:29, moonrise 12:40,
    // moonset 00:51 and moonrise 13:45 the next day.
    const int64_t midnight = static_cast<int64_t>( daysFromCivil( 2026, 6, 21 ) ) * 86400 - 3 * 3600;
    Look look = { validStyle( style ), MoonItem | DateItem, 45, true, {}, 25, 2, Language::current() };
    look.corners.valid = true;
    const int sunMinutes[4] = { 1238 - 1440, 332, 1240, 333 + 1440 };
    const int moonMinutes[4] = { 29, 760, 51 + 1440, 825 + 1440 };
    for ( int i = 0; i < 4; ++i ) {
        look.corners.sun.times[i] = midnight + sunMinutes[i] * 60;
        look.corners.sun.rising[i] = i % 2 == 1;
        look.corners.moon.times[i] = midnight + moonMinutes[i] * 60;
        look.corners.moon.rising[i] = i % 2 == 1;
    }
    look.corners.sun.count = 4;
    look.corners.moon.count = 4;
    const Rtc::DateTime time = { 2026, 6, 21, 19, 19, 0 };
    const Face &face = layout( look, time, midnight + ( 19 * 60 + 19 ) * 60 );
    // The scene's gauges: 91 % of the day, the battery at 92 %, the Moon 55 % through its pass
    // and 45 % lit, waxing.
    const Corners::Gauge gauges[Corners::CornerCount] = {
        { 0, 910, Corners::Day, false }, { 0, 920, Corners::BatteryHigh, false }, { 0, 546, Corners::MoonUp, false },
        { 0, 450, Corners::MoonLit, false } };
    const int faceX = face.x >= 0 ? face.x : ( ( Panel::WIDTH - face.width ) / 2 ) & ~1;
    const int faceY = face.y >= 0 ? face.y : ( ( Panel::HEIGHT - face.height ) / 2 ) & ~1;
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

uint64_t AlwaysOn::update( const Rtc::DateTime &time, int64_t utcNow, bool timeRead, int batteryPercent,
                           bool charging )
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
    const Face &face = layout( look, shown, utcNow );
    if ( blank ) {
        // Battery run V: cleared like any other face, with nothing drawn.
        Face &cleared = const_cast<Face &>( face );
        cleared.count = 0;
        cleared.shapeCount = 0;
    }
    // Burn-in: the face moves within +-8 pixels, one step per minute (the sky styles keep 8 px
    // from the screen's edges for it).
    const int baseX = face.x >= 0 ? face.x : ( ( Panel::WIDTH - face.width ) / 2 ) & ~1;
    const int baseY = face.y >= 0 ? face.y : ( ( Panel::HEIGHT - face.height ) / 2 ) & ~1;
    const int faceX = baseX + ( minuteOfDay % 5 - 2 ) * 4;
    const int faceY = baseY + ( ( minuteOfDay / 5 ) % 5 - 2 ) * 4;
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
        const int drawn[4] = { gauges[i].start * s_gaugePixels / 1000, gauges[i].end * s_gaugePixels / 1000,
                               gauges[i].kind, gauges[i].charging ? 1 : 0 };
        if ( drawn[0] == s_drawnGauges[i][0] && drawn[1] == s_drawnGauges[i][1] && drawn[2] == s_drawnGauges[i][2] &&
             drawn[3] == s_drawnGauges[i][3] ) {
            continue;
        }
        const int boxX = ( i == Corners::TopLeft || i == Corners::BottomLeft ) ? 0 : Panel::WIDTH - s_cornerBox;
        const int boxY = ( i == Corners::TopLeft || i == Corners::TopRight ) ? 0 : Panel::HEIGHT - s_cornerBox;
        render( panel, face, faceX, faceY, boxX, boxY, s_cornerBox, s_cornerBox, moonBits, gauges );
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
