// Host test of main/SevenSegment.cpp. Build and run: sh tests/run_host_tests.sh

#include <cmath>
#include <cstdio>

#include "SevenSegment.h"

static int s_checks = 0;
static int s_failures = 0;

static void check( const char *name, bool condition )
{
    ++s_checks;
    if ( !condition ) {
        printf( "FAIL %s\n", name );
        ++s_failures;
    }
}

int main()
{
    const float height = 122;
    const float width = SevenSegment::digitWidth( height );
    check( "width is 0.62 of the height", std::fabs( width - 75.64f ) < 0.01f );

    SevenSegment::Hexagon hexagons[7] = {};
    check( "eight lights seven segments", SevenSegment::segments( 8, height, hexagons ) == 7 );
    bool inside = true;
    for ( int i = 0; i < 7; ++i ) {
        for ( int p = 0; p < 6; ++p ) {
            const SevenSegment::Point &point = hexagons[i].p[p];
            inside = inside && point.x >= 0 && point.x <= width && point.y >= 0 && point.y <= height;
        }
    }
    check( "every corner inside the digit", inside );
    check( "one lights two segments", SevenSegment::segments( 1, height, hexagons ) == 2 );

    // The top segment (the first one of an 8): its height is the thickness, height / 11.
    SevenSegment::segments( 8, height, hexagons );
    float top = hexagons[0].p[0].y;
    float bottom = top;
    for ( int p = 0; p < 6; ++p ) {
        top = std::fmin( top, hexagons[0].p[p].y );
        bottom = std::fmax( bottom, hexagons[0].p[p].y );
    }
    check( "thickness is a height / 11", std::fabs( ( bottom - top ) - height / 11 ) < 0.01f );
    check( "colon square is a height / 11", std::fabs( SevenSegment::colonSize( height ) - height / 11 ) < 0.01f );

    // Fits between the corner gauges at the clock's height, as the owner's mock-up (x 22 to 388).
    check( "time block fits", SevenSegment::timeWidth( height ) <= 368 );
    float x[4] = {};
    float colonX = 0;
    SevenSegment::layout( height, x, colonX );
    check( "digits in order", x[0] == 0 && x[1] > x[0] + width && x[2] > colonX && x[3] > x[2] + width );
    check( "last digit ends at the block's width", std::fabs( x[3] + width - SevenSegment::timeWidth( height ) ) < 0.01f );

    printf( "Seven segment: %d checks, %d failures\n", s_checks, s_failures );
    return s_failures == 0 ? 0 : 1;
}
