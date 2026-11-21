// Host test of main/Tilt.cpp (the spirit level's and the alidade's angles).
// Build and run: sh tests/run_host_tests.sh

#include <cmath>
#include <cstdio>

#include "Tilt.h"

static int failures = 0;
static const double s_pi = 3.14159265358979323846;

static void near( const char *name, double value, double expected, double tolerance )
{
    if ( !( std::fabs( value - expected ) <= tolerance ) ) {
        printf( "FAIL %s: %.6f, expected %.6f\n", name, value, expected );
        ++failures;
    }
}

// The sensor's reading of a watch whose right edge is raised by `roll` degrees, then its top edge
// by `pitch` degrees (an upright watch: pitch 90), at a scale of 1047 mg per g as measured on the
// desk (docs/hardware.md): the angles must not depend on the scale.
static Tilt::Vector tilted( double rollDegrees, double pitchDegrees, int mirror )
{
    const double r = rollDegrees * s_pi / 180.0;
    const double p = pitchDegrees * s_pi / 180.0;
    const double right = std::sin( r ) * std::cos( p );
    const double up = std::sin( p );
    const double out = std::cos( r ) * std::cos( p );
    const double scale = 1047.0;
    // Sensor: x = up, y = right, z = -out.
    return Tilt::fromSensor( static_cast<int>( std::lround( up * scale ) ), static_cast<int>( std::lround( right * scale ) ),
                             static_cast<int>( std::lround( -out * scale ) ), mirror );
}

int main()
{
    // Face up on the desk: the reading of the log, x=-4 y=12 z=-1048 mg.
    const Tilt::Vector desk = Tilt::fromSensor( -4, 12, -1048, 0 );
    near( "desk: out of the screen is up", desk.out, 1.048, 1e-9 );
    near( "desk: slope", Tilt::slope( desk ), 0.69, 0.01 );
    near( "desk: pitch", Tilt::pitch( desk ), -0.22, 0.01 );
    near( "desk: roll", Tilt::roll( desk ), 0.66, 0.01 );

    // Flat level: pitch and roll as the edges are raised; the bubble moves to the higher side.
    const Tilt::Vector rightUp = tilted( 5, 0, 0 );
    near( "right edge up: roll", Tilt::roll( rightUp ), 5, 0.05 );
    near( "right edge up: pitch", Tilt::pitch( rightUp ), 0, 0.05 );
    near( "right edge up: slope", Tilt::slope( rightUp ), 5, 0.05 );
    double bubbleRight = 0;
    double bubbleUp = 0;
    Tilt::bubble( rightUp, bubbleRight, bubbleUp );
    near( "bubble right", bubbleRight, std::sqrt( 5.0 / 45.0 ), 0.005 );
    near( "bubble not up", bubbleUp, 0, 0.005 );
    const Tilt::Vector topUp = tilted( 0, 10, 0 );
    near( "top edge up: pitch", Tilt::pitch( topUp ), 10, 0.05 );
    Tilt::bubble( topUp, bubbleRight, bubbleUp );
    near( "bubble up", bubbleUp, std::sqrt( 10.0 / 45.0 ), 0.005 );
    // Beyond 45 degrees the bubble stays on the outer ring.
    Tilt::bubble( tilted( -60, 0, 0 ), bubbleRight, bubbleUp );
    near( "bubble on the ring", bubbleRight, -1, 1e-6 );
    near( "ring of 30 degrees", Tilt::ringFraction( 30 ), std::sqrt( 30.0 / 45.0 ), 1e-9 );
    near( "ring of 0", Tilt::ringFraction( 0 ), 0, 1e-9 );

    // Mirrored pictures: what the user sees as right or up turns with the picture.
    near( "mirrored left-right: roll", Tilt::roll( tilted( 5, 0, 1 ) ), -5, 0.05 );
    near( "mirrored left-right: pitch", Tilt::pitch( tilted( 0, 7, 1 ) ), 7, 0.05 );
    near( "mirrored top-bottom: pitch", Tilt::pitch( tilted( 0, 7, 2 ) ), -7, 0.05 );
    near( "turned around: roll", Tilt::roll( tilted( 5, 0, 3 ) ), -5, 0.05 );
    near( "turned around: pitch", Tilt::pitch( tilted( 0, 7, 3 ) ), -7, 0.05 );

    // Upright (pitch 90), turned in its plane: the top edge's altitude, from either end and in
    // any mirroring.
    for ( int mirror = 0; mirror < 4; ++mirror ) {
        for ( int angle = -85; angle <= 85; angle += 17 ) {
            // Upright with the right edge raised by `angle`: the screen's plane vertical.
            const double a = angle * s_pi / 180.0;
            const int x = static_cast<int>( std::lround( std::cos( a ) * 1000 ) );
            const int y = static_cast<int>( std::lround( std::sin( a ) * 1000 ) );
            char name[48];
            snprintf( name, sizeof( name ), "edge altitude %d, mirror %d", angle, mirror );
            near( name, Tilt::edgeAltitude( Tilt::fromSensor( x, y, 0, mirror ) ), std::abs( angle ), 0.1 );
        }
    }
    near( "upright: slope 90", Tilt::slope( tilted( 0, 90, 0 ) ), 90, 0.05 );
    // The screen leaning back by 30 degrees does not change the edge's altitude.
    {
        const double a = 40 * s_pi / 180.0;
        const double lean = 30 * s_pi / 180.0;
        // The edge (right axis) at 40 degrees; the screen turned about it by 30 degrees.
        const double right = std::sin( a );
        const double up = std::cos( a ) * std::cos( lean );
        const double out = std::cos( a ) * std::sin( lean );
        near( "leaning: edge altitude", Tilt::edgeAltitude( { right, up, out } ), 40, 1e-9 );
    }

    // The filter: the first sample at once, then a quarter of each step.
    Tilt filter;
    near( "filter: empty", filter.valid() ? 1 : 0, 0, 0 );
    filter.add( { 0, 0, 1 } );
    near( "filter: first", filter.value().out, 1, 1e-12 );
    filter.add( { 1, 0, 1 } );
    near( "filter: second", filter.value().right, 0.25, 1e-12 );
    for ( int i = 0; i < 40; ++i ) {
        filter.add( { 1, 0, 1 } );
    }
    near( "filter: settled", filter.value().right, 1, 1e-4 );
    filter.reset();
    near( "filter: reset", filter.valid() ? 1 : 0, 0, 0 );

    // Sighting.
    double value = 0;
    near( "height: valid", Tilt::heightFromDistance( 20, 39.1, value ) ? 1 : 0, 1, 0 );
    near( "height", value, 20 * std::tan( 39.1 * s_pi / 180.0 ), 1e-9 );
    near( "height of 45 degrees", ( Tilt::heightFromDistance( 12, 45, value ), value ), 12, 1e-9 );
    near( "distance of 45 degrees", ( Tilt::distanceFromHeight( 12, 45, value ), value ), 12, 1e-9 );
    near( "distance of 30 degrees", ( Tilt::distanceFromHeight( 10, 30, value ), value ), 10 * std::sqrt( 3.0 ), 1e-9 );
    near( "no height at 90", Tilt::heightFromDistance( 10, 90, value ) ? 1 : 0, 0, 0 );
    near( "no distance at 0", Tilt::distanceFromHeight( 10, 0, value ) ? 1 : 0, 0, 0 );

    if ( failures == 0 ) {
        printf( "Tilt: all tests passed\n" );
    }
    return failures == 0 ? 0 : 1;
}
