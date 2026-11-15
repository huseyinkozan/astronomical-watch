// Makes main/CornerTable.c: the pixels of the top-left corner gauge's ring (main/CornerTable.h).
// The host tests make it again and compare, so a change of the geometry cannot leave it stale:
// tests/run_host_tests.sh builds it as build-host/make_corner_table; after a change:
//     build-host/make_corner_table > main/CornerTable.c

#include <cmath>
#include <cstdio>
#include <vector>

#include "CornerTable.h"
#include "Corners.h"

// Any screen: the top-left corner does not depend on its size.
static const int s_width = 410;
static const int s_height = 502;

static double toRadians( double degrees )
{
    return degrees * M_PI / 180.0;
}

static double clamp01( double value )
{
    return value < 0 ? 0 : ( value > 1 ? 1 : value );
}

// How much of pixel (x, y) a stroke covers that runs along the circle (centre cx, cy) from
// fraction `from` to `to` of the gauge, `halfWidth` either side of `radius`, with round ends: a
// distance field, so that its edges come out smooth.
static double strokeCover( double x, double y, double cx, double cy, int lower, int upper, double along, double from,
                           double to, double radius, double halfWidth )
{
    const double r = std::hypot( x - cx, y - cy );
    double distance = 0;
    if ( along >= from && along <= to ) {
        distance = std::fabs( r - radius );
    } else {
        const double end = along < from ? from : to;
        const double angle = toRadians( lower + ( upper - lower ) * end );
        distance = std::hypot( x - ( cx + radius * std::cos( angle ) ), y - ( cy + radius * std::sin( angle ) ) );
    }
    return clamp01( halfWidth + 0.5 - distance );
}

int main()
{
    int cx = 0;
    int cy = 0;
    int lower = 0;
    int upper = 0;
    Corners::geometry( Corners::TopLeft, s_width, s_height, cx, cy, lower, upper );
    const double radius = Corners::OuterRadius - Corners::ActiveWidth / 2.0;
    // The active gauge, its round ends and a pixel of edge smoothing.
    const double innerSpan = Corners::OuterRadius - Corners::ActiveWidth - 1.0;
    const double outerSpan = Corners::OuterRadius + 1.0;
    const double mid = ( lower + upper ) / 2.0;
    const double middle = toRadians( mid );
    // The always-on gauge: whole pixels whose squared distance lies in (inner, outer].
    const int aodOuter = Corners::OuterRadius * Corners::OuterRadius;
    const int aodInner = ( Corners::OuterRadius - Corners::AlwaysOnWidth ) * ( Corners::OuterRadius - Corners::AlwaysOnWidth );
    // Its notches: the 2 x 2 blocks whose centres lie nearest to both ends and the middle.
    int notchX[3] = {};
    int notchY[3] = {};
    const double aodRadius = Corners::OuterRadius - Corners::AlwaysOnWidth / 2.0;
    for ( int k = 0; k < 3; ++k ) {
        const double angle = toRadians( lower + ( upper - lower ) * k / 2.0 );
        notchX[k] = static_cast<int>( std::floor( cx + aodRadius * std::cos( angle ) - 0.5 ) );
        notchY[k] = static_cast<int>( std::floor( cy + aodRadius * std::sin( angle ) - 0.5 ) );
    }

    std::vector<CornerPixel> pixels;
    int first[CORNER_TABLE_BOX] = {};
    int count[CORNER_TABLE_BOX] = {};
    int offset[CORNER_TABLE_BOX] = {};
    for ( int y = 0; y < CORNER_TABLE_BOX; ++y ) {
        // The row's run from its first to its last pixel of the ring (one run in this box).
        int start = -1;
        int end = -1;
        for ( int x = 0; x < CORNER_TABLE_BOX; ++x ) {
            const double r = std::hypot( static_cast<double>( x - cx ), static_cast<double>( y - cy ) );
            if ( r >= innerSpan && r <= outerSpan ) {
                start = start < 0 ? x : start;
                end = x;
            }
        }
        offset[y] = static_cast<int>( pixels.size() );
        first[y] = start < 0 ? 0 : start;
        count[y] = start < 0 ? 0 : end - start + 1;
        for ( int x = first[y]; x < first[y] + count[y]; ++x ) {
            const int dx = x - cx;
            const int dy = y - cy;
            const double r = std::hypot( static_cast<double>( dx ), static_cast<double>( dy ) );
            double angle = std::atan2( static_cast<double>( dy ), static_cast<double>( dx ) ) * 180.0 / M_PI;
            while ( angle - mid > 180 ) {
                angle -= 360;
            }
            while ( angle - mid < -180 ) {
                angle += 360;
            }
            const double along = ( angle - lower ) / ( upper - lower );
            const double channel = strokeCover( x, y, cx, cy, lower, upper, along, 0, 1, radius, Corners::ActiveWidth / 2.0 );
            // The notch: 2 px across the gauge at its middle.
            const double across = std::fabs( -dx * std::sin( middle ) + dy * std::cos( middle ) );
            const bool onRadial = dx * std::cos( middle ) + dy * std::sin( middle ) > 0;
            const double notch = onRadial && r >= Corners::OuterRadius - Corners::ActiveWidth - 0.5 &&
                                         r <= Corners::OuterRadius + 0.5
                                     ? clamp01( 1.5 - across )
                                     : 0;
            CornerPixel pixel = {};
            pixel.along = static_cast<int16_t>( std::lround( along * 1000 * CORNER_ALONG_SCALE ) );
            pixel.radial = static_cast<int8_t>( std::lround( ( r - radius ) * 16 ) );
            pixel.channel = static_cast<uint8_t>( std::lround( channel * 255 ) );
            pixel.notch = static_cast<uint8_t>( std::lround( notch * 255 ) );
            const int distance = dx * dx + dy * dy;
            if ( dx <= 0 && dy <= 0 && distance <= aodOuter && distance > aodInner ) {
                pixel.flags |= CORNER_AOD_RING;
            }
            for ( int k = 0; k < 3; ++k ) {
                if ( x - notchX[k] >= 0 && x - notchX[k] < 2 && y - notchY[k] >= 0 && y - notchY[k] < 2 ) {
                    pixel.flags |= CORNER_AOD_NOTCH;
                }
            }
            pixels.push_back( pixel );
        }
    }

    printf( "// Made by tools/make_corner_table.cpp; do not edit.\n\n#include \"CornerTable.h\"\n\n" );
    const char *names[3] = { "cornerRowFirst", "cornerRowCount", "cornerRowOffset" };
    const int *values[3] = { first, count, offset };
    for ( int k = 0; k < 3; ++k ) {
        printf( "const %s %s[CORNER_TABLE_BOX] = {", k < 2 ? "uint8_t" : "uint16_t", names[k] );
        for ( int y = 0; y < CORNER_TABLE_BOX; ++y ) {
            printf( "%s%d,", y % 16 == 0 ? "\n    " : " ", values[k][y] );
        }
        printf( "\n};\n\n" );
    }
    printf( "// along, radial, channel, notch, flags\nconst CornerPixel cornerPixels[%d] = {\n",
            static_cast<int>( pixels.size() ) );
    for ( size_t i = 0; i < pixels.size(); ++i ) {
        const CornerPixel &p = pixels[i];
        printf( "    { %d, %d, %d, %d, %d },\n", p.along, p.radial, p.channel, p.notch, p.flags );
    }
    printf( "};\n" );
    return 0;
}
