#include "AlwaysOn.h"

#include <cstdio>

#include "Panel.h"
#include "esp_attr.h"
#include "sdkconfig.h"

// Seven-segment geometry in pixels; every value is even (panel rounding).
static const int DIGIT_WIDTH = 56;
static const int DIGIT_HEIGHT = 104;
static const int STROKE = 12;
static const int DIGIT_STEP = 68;
static const int COLON_GAP = 24;
static const int FACE_WIDTH = 4 * DIGIT_STEP - ( DIGIT_STEP - DIGIT_WIDTH ) + COLON_GAP;
static const int FACE_X = ( ( Panel::WIDTH - FACE_WIDTH ) / 2 ) & ~1;
static const int FACE_Y = ( ( Panel::HEIGHT - DIGIT_HEIGHT ) / 2 ) & ~1;

#if CONFIG_WATCH_AOD_IDLE_MODE
static const bool s_idleMode = true;
#else
static const bool s_idleMode = false;
#endif

// Segments a to g in bits 0 to 6.
static const uint8_t s_digitSegments[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };

// Kept in RTC memory: cleared on power-on and reset.
RTC_DATA_ATTR static bool s_active;
RTC_DATA_ATTR static bool s_screenCleared;
RTC_DATA_ATTR static int s_failedMinutes;
RTC_DATA_ATTR static int s_faceX;
RTC_DATA_ATTR static int s_faceY;

static const uint64_t s_retryUs = 60ULL * 1000 * 1000;
// A lasting RTC fault ends the face, so that it cannot keep the watch out of off mode.
static const int s_maxFailedMinutes = 30;

static void drawDigit( Panel &panel, int x, int y, int digit )
{
    const int half = DIGIT_HEIGHT / 2;
    const uint8_t segments = s_digitSegments[digit];
    // a, b, c, d, e, f, g as rectangles: x, y, width, height.
    const int rects[7][4] = {
        { x, y, DIGIT_WIDTH, STROKE },
        { x + DIGIT_WIDTH - STROKE, y, STROKE, half },
        { x + DIGIT_WIDTH - STROKE, y + half, STROKE, half },
        { x, y + DIGIT_HEIGHT - STROKE, DIGIT_WIDTH, STROKE },
        { x, y + half, STROKE, half },
        { x, y, STROKE, half },
        { x, y + half - STROKE / 2, DIGIT_WIDTH, STROKE },
    };
    for ( int i = 0; i < 7; ++i ) {
        if ( segments & ( 1 << i ) ) {
            panel.fill( rects[i][0], rects[i][1], rects[i][2], rects[i][3], true );
        }
    }
}

// HH:MM at (x, y): the digits and the colon, in white on what is already there.
static void drawFace( Panel &panel, int x, int y, int hour, int minute )
{
    drawDigit( panel, x, y, hour / 10 );
    drawDigit( panel, x + DIGIT_STEP, y, hour % 10 );
    const int colonX = x + 2 * DIGIT_STEP - ( DIGIT_STEP - DIGIT_WIDTH ) + ( COLON_GAP - STROKE ) / 2;
    panel.fill( colonX & ~1, y + 30, STROKE, STROKE, true );
    panel.fill( colonX & ~1, y + DIGIT_HEIGHT - 30 - STROKE, STROKE, STROKE, true );
    const int minutesX = x + 2 * DIGIT_STEP + COLON_GAP;
    drawDigit( panel, minutesX, y, minute / 10 );
    drawDigit( panel, minutesX + DIGIT_STEP, y, minute % 10 );
}

#if CONFIG_WATCH_DEV_LOGGING
void AlwaysOn::capture()
{
    Panel panel;
    panel.record();
    printf( "SHOT BEGIN -1 %d %d always-on\n", Panel::WIDTH, Panel::HEIGHT );
    panel.fill( 0, 0, Panel::WIDTH, Panel::HEIGHT, false );
    drawFace( panel, FACE_X, FACE_Y, 19, 19 );
    printf( "SHOT END\n" );
    fflush( stdout );
}
#endif

void AlwaysOn::begin()
{
    s_active = true;
    s_screenCleared = false;
    s_failedMinutes = 0;
    s_faceX = FACE_X;
    s_faceY = FACE_Y;
}

void AlwaysOn::end()
{
    s_active = false;
}

bool AlwaysOn::isActive() const
{
    return s_active;
}

uint64_t AlwaysOn::update( const Rtc::DateTime &time, bool timeRead )
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
        panel.fill( 0, 0, Panel::WIDTH, Panel::HEIGHT, false );
        s_screenCleared = true;
    } else {
        panel.fill( s_faceX, s_faceY, FACE_WIDTH, DIGIT_HEIGHT, false );
    }

    // Burn-in: the face moves within +-8 pixels, one step per minute.
    s_faceX = FACE_X + ( minuteOfDay % 5 - 2 ) * 4;
    s_faceY = FACE_Y + ( ( minuteOfDay / 5 ) % 5 - 2 ) * 4;
    drawFace( panel, s_faceX, s_faceY, minuteOfDay / 60, minuteOfDay % 60 );
    // The brightness command is sent after the drawing has finished.
    panel.setBrightness( CONFIG_WATCH_AOD_BRIGHTNESS );
    Panel::holdPins();

    // Land just after the minute changes; the RTC read on the next wake corrects any drift.
    return static_cast<uint64_t>( secondsToNext ) * 1000 * 1000 + 100 * 1000;
}
