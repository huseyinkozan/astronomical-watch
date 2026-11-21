// Host test of main/Observing.cpp and main/Messier.cpp (the observing page's polar alignment, Milky
// Way window and deep-sky finder) against Skyfield (tests/observing_reference.csv,
// tools/make_observing_reference.py), and of main/Optics.cpp (the eyepiece and photo
// calculators) against hand-worked values.
// Build and run: sh tests/run_host_tests.sh

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Messier.h"
#include "Observing.h"
#include "Optics.h"

static int failures = 0;
static int checks = 0;

static void near( const char *name, double value, double expected, double tolerance )
{
    ++checks;
    if ( !( std::fabs( value - expected ) <= tolerance ) ) {
        printf( "FAIL %s: %.6f, expected %.6f\n", name, value, expected );
        ++failures;
    }
}

static double wrapped( double degrees )
{
    double value = std::fmod( degrees, 360.0 );
    if ( value > 180.0 ) {
        value -= 360.0;
    } else if ( value < -180.0 ) {
        value += 360.0;
    }
    return value;
}

static void reticle()
{
    // North: the star above the pole at the hour angle 0, then counterclockwise (west is on the
    // left looking north); the polar scope turns it by half a turn.
    near( "north upright 0h", Observing::reticleMinutes( 0, false, false ), 0, 0 );
    near( "north upright 6h: 9 o'clock", Observing::reticleMinutes( 90, false, false ), 540, 0 );
    near( "north inverted 0h: 6 o'clock", Observing::reticleMinutes( 0, false, true ), 360, 0 );
    near( "north inverted 6h: 3 o'clock", Observing::reticleMinutes( 90, false, true ), 180, 0 );
    near( "north 8h40m inverted", Observing::reticleMinutes( 130, false, true ), 100, 0 );
    // South: clockwise (west is on the right looking south).
    near( "south upright 6h: 3 o'clock", Observing::reticleMinutes( 90, true, false ), 180, 0 );
    near( "south inverted 0h", Observing::reticleMinutes( 0, true, true ), 360, 0 );
    near( "a full turn", Observing::reticleMinutes( 359.9, false, false ), 0, 0 );
}

static void optics()
{
    // 1000 mm f/8 (125 mm) with a 25 mm, 52 degree eyepiece.
    const double m = Optics::magnification( 1000, 25 );
    near( "magnification", m, 40, 1e-12 );
    near( "exit pupil", Optics::exitPupil( 125, m ), 3.125, 1e-12 );
    near( "true field", Optics::trueField( 52, m ), 1.3, 1e-12 );
    // 2.7 + 5 log10(125) = 13.18; Dawes 116 / 125.
    near( "limiting magnitude", Optics::limitingMagnitude( 125 ), 13.1845, 1e-4 );
    near( "Dawes", Optics::dawesLimit( 125 ), 0.928, 1e-12 );
    near( "focal ratio", Optics::focalRatio( 1000, 125 ), 8, 1e-12 );
    // 24 mm on APS-C (1.5): 500 / 36 = 13.9 s; NPF (35 * 2.8 + 30 * 3.9) / 24 = 8.96 s.
    near( "500 rule", Optics::rule500( 24, 1.5 ), 13.8889, 1e-4 );
    near( "NPF rule", Optics::npfRule( 24, 2.8, 3.9 ), 8.9583, 1e-4 );
    near( "ND 10 from 1/60", Optics::ndExposure( 1.0 / 60, 10 ), 17.0667, 1e-4 );
    near( "ND 0", Optics::ndExposure( 0.5, 0 ), 0.5, 1e-12 );
    near( "trail frames", Optics::trailFrames( 60, 30 ), 120, 0 );
    near( "trail frames, a part frame counts", Optics::trailFrames( 10, 7 ), 86, 0 );
    near( "trail arc an hour", Optics::trailArc( 60 ), 15.041, 1e-3 );
    near( "shutter 1/60", Optics::shutterSeconds( Optics::DefaultShutter ), 1.0 / 60, 1e-12 );
    near( "shutter 30 s", Optics::shutterSeconds( Optics::ShutterCount - 1 ), 30, 1e-12 );
    for ( int i = 0; i < Optics::SensorCount; ++i ) {
        near( "sensor crop", Optics::Sensors[i].crop >= 1.0 && Optics::Sensors[i].crop < 3.0 ? 1 : 0, 1, 0 );
    }
}

static void reference( const char *path )
{
    FILE *file = fopen( path, "r" );
    if ( file == nullptr ) {
        printf( "FAIL cannot open %s\n", path );
        ++failures;
        return;
    }
    double worstPole = 0;
    double worstNight = 0;
    double worstWindow = 0;
    double worstHighest = 0;
    double worstMessier = 0;
    double worstBest = 0;
    int windows = 0;
    char line[256];
    static Messier::Entry entries[Messier::Count];
    while ( fgets( line, sizeof( line ), file ) != nullptr ) {
        if ( line[0] == '#' ) {
            continue;
        }
        char kind[16] = {};
        double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0, h = 0, k = 0;
        sscanf( line, "%15[^,],%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf", kind, &a, &b, &c, &d, &e, &f, &g, &h, &k );
        if ( strcmp( kind, "pole" ) == 0 ) {
            // pole, south, longitude, time, hour angle. Precession only: nutation and aberration
            // move a star 1 degree from the pole by up to about 30" in arc, which is up to about
            // 1 degree of its hour angle (and a quarter of that on the 12-hour reticle).
            const double diff = std::fabs( wrapped( Observing::poleStarHourAngle( a != 0, static_cast<int64_t>( c ), b ) - d ) );
            worstPole = diff > worstPole ? diff : worstPole;
            near( a != 0 ? "sigma Octantis hour angle" : "Polaris hour angle", diff, 0, 1.2 );
        } else if ( strcmp( kind, "centre" ) == 0 ) {
            double ra = 0;
            double dec = 0;
            Astronomy::precess( 17 + 45 / 60.0 + 40.04 / 3600.0, -( 29 + 28.1 / 3600.0 ), static_cast<int64_t>( c ), ra, dec );
            near( "centre altitude", Observing::altitude( ra, dec, static_cast<int64_t>( c ), { a, b } ), d, 0.05 );
        } else if ( strcmp( kind, "night" ) == 0 ) {
            // night, latitude, longitude, noon, dark, dusk, dawn: the declination taken at each
            // event and the hour angle's formula give the events within seconds (9 s at worst).
            const Observing::Night night = Observing::night( static_cast<int64_t>( c ), { a, b } );
            near( "night dark", night.dark ? 1 : 0, d, 0 );
            if ( night.dark && d != 0 ) {
                const double diff = std::fmax( std::fabs( night.dusk - e ), std::fabs( night.dawn - f ) );
                worstNight = diff > worstNight ? diff : worstNight;
                near( "dusk", night.dusk, e, 60 );
                near( "dawn", night.dawn, f, 60 );
            }
        } else if ( strcmp( kind, "window" ) == 0 ) {
            // window, latitude, longitude, noon, window, start, end, highest, altitude. The Moon is
            // looked at every 5 min (the window's ends set by it move inwards by up to that);
            // the reference goes by the minute.
            const Observing::MilkyWayNight night = Observing::milkyWay( static_cast<int64_t>( c ), { a, b } );
            near( "window", night.window ? 1 : 0, d, 0 );
            if ( night.window && d != 0 ) {
                ++windows;
                const double diff = std::fmax( std::fabs( night.start - e ), std::fabs( night.end - f ) );
                worstWindow = diff > worstWindow ? diff : worstWindow;
                near( "window start", night.start, e, Observing::MoonStep + 120 );
                near( "window end", night.end, f, Observing::MoonStep + 120 );
                near( "highest time", night.highest, g, Observing::MoonStep + 120 );
                // At a window's end the altitude changes by up to about 0.25 degree a minute (the
                // worst seen: 0.15 degree).
                const double altitudeDiff = std::fabs( night.highestAltitude - h );
                worstHighest = altitudeDiff > worstHighest ? altitudeDiff : worstHighest;
                near( "highest altitude", night.highestAltitude, h, 0.5 );
            }
        } else if ( strcmp( kind, "messier" ) == 0 ) {
            // messier, index, latitude, longitude, time, altitude.
            const int index = static_cast<int>( a );
            const int64_t time = static_cast<int64_t>( d );
            double ra = 0;
            double dec = 0;
            Astronomy::precess( Messier::raHours( index ), Messier::decDegrees( index ), time, ra, dec );
            const double diff = std::fabs( Observing::altitude( ra, dec, time, { b, c } ) - e );
            worstMessier = diff > worstMessier ? diff : worstMessier;
            near( "Messier altitude", diff, 0, 0.05 );
            // The list has it exactly when it is above 10 degrees (away from the edge).
            if ( std::fabs( e - 10 ) > 0.1 ) {
                const Observing::Night none = {};
                const int count = Messier::visible( time, { b, c }, none, 10, entries );
                bool listed = false;
                for ( int i = 0; i < count; ++i ) {
                    listed = listed || entries[i].index == index;
                }
                near( "Messier listed above 10 degrees", listed ? 1 : 0, e > 10 ? 1 : 0, 0 );
            }
        } else if ( strcmp( kind, "best" ) == 0 ) {
            // best, index, latitude, longitude, noon, best time.
            const int index = static_cast<int>( a );
            const Observing::Night night = Observing::night( static_cast<int64_t>( d ), { b, c } );
            if ( e == 0 ) {
                near( "best: no darkness", night.dark ? 1 : 0, 0, 0 );
                continue;
            }
            double ra = 0;
            double dec = 0;
            Astronomy::precess( Messier::raHours( index ), Messier::decDegrees( index ), night.dusk, ra, dec );
            const int64_t best = Observing::highestTime( ra, night.dusk, night.dawn, c );
            const double diff = std::fabs( best - e );
            worstBest = diff > worstBest ? diff : worstBest;
            near( "best time", best, e, 180 );
        }
    }
    fclose( file );
    printf( "worst: pole %.2f deg, night %.0f s, window %.0f s, highest %.2f deg, Messier %.3f deg, best %.0f s; "
            "%d windows\n",
            worstPole, worstNight, worstWindow, worstHighest, worstMessier, worstBest, windows );
}

static void sorting()
{
    // Istanbul, 2026-11-21 20:00 local: sorted by altitude, all above 10 degrees, best times
    // inside tonight's darkness.
    const Astronomy::Observer istanbul = { 41.01, 28.98 };
    const int64_t noon = 1795251600;  // 2026-11-21 12:00 UTC+3
    const Observing::Night night = Observing::night( noon, istanbul );
    near( "Istanbul in November is dark", night.dark ? 1 : 0, 1, 0 );
    static Messier::Entry entries[Messier::Count];
    const int count = Messier::visible( noon + 8 * 3600, istanbul, night, 10, entries );
    near( "some objects up", count > 20 ? 1 : 0, 1, 0 );
    for ( int i = 0; i < count; ++i ) {
        near( "above 10", entries[i].altitude >= 10 ? 1 : 0, 1, 0 );
        near( "sorted", i == 0 || entries[i - 1].altitude >= entries[i].altitude ? 1 : 0, 1, 0 );
        near( "best in the dark",
              entries[i].bestMinute * 60 >= night.dusk - 60 && entries[i].bestMinute * 60 <= night.dawn ? 1 : 0, 1, 0 );
    }

    // The next window from late November at Istanbul: the centre is up only by day until the
    // mornings of February; the search passes the nights between quickly.
    const auto started = std::chrono::steady_clock::now();
    int64_t found = 0;
    const bool any = Observing::nextMilkyWayNight( noon, 120, istanbul, found );
    const double ms = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - started ).count();
    near( "a window by spring", any ? 1 : 0, 1, 0 );
    near( "not before February", found > noon + 60 * 86400 ? 1 : 0, 1, 0 );
    printf( "next Milky Way night from 2026-11-21 at Istanbul: %+.0f days (%.1f ms on the host)\n",
            ( found - noon ) / 86400.0, ms );
    // Never 10 degrees up at Tromso.
    near( "Tromso: the centre stays low", Observing::centreRises( 69.65 ) ? 1 : 0, 0, 0 );
    near( "Istanbul: the centre rises", Observing::centreRises( 41.01 ) ? 1 : 0, 1, 0 );
}

int main( int argc, char **argv )
{
    reticle();
    optics();
    sorting();
    reference( argc > 1 ? argv[1] : "tests/observing_reference.csv" );
    printf( "Observing: %d checks, %d failures\n", checks, failures );
    return failures == 0 ? 0 : 1;
}
