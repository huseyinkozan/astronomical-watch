#pragma once

// The observing page's calculators (docs/superpowers/plans/2026-11-21-tools.md, stage 4): a
// telescope with an eyepiece, and a camera on a tripod under the stars. Plain C++ for the host
// tests (tests/ObservingTest.cpp). Lengths in millimetres, angles in degrees, times in seconds.
class Optics
{
public:
    // ---- Eyepiece ----

    static double magnification( double scopeFocal, double eyepieceFocal );
    // The beam leaving the eyepiece: aperture / magnification.
    static double exitPupil( double aperture, double magnification );
    // The sky the eyepiece shows: its apparent field / magnification.
    static double trueField( double apparentField, double magnification );
    // The faintest star under a dark sky: 2.7 + 5 log10(aperture) (Sky & Telescope's rule of
    // thumb, at a high enough magnification).
    static double limitingMagnitude( double aperture );
    // Dawes' limit for two equal stars: 116 / aperture, arcseconds.
    static double dawesLimit( double aperture );
    static double focalRatio( double scopeFocal, double aperture );

    // ---- Camera ----

    // Sensor presets: the crop factor and a typical pixel pitch (micrometres) of today's cameras.
    struct Sensor
    {
        const char *name;
        double crop;
        double pixelPitch;
    };
    static const int SensorCount = 5;
    static const Sensor Sensors[SensorCount];

    // The longest exposure without star trails by the 500 rule: 500 / (focal * crop).
    static double rule500( double focal, double crop );
    // By the NPF rule in Frederic Michaud's simplified form: (35 N + 30 p) / f, with the f-number
    // N, the pixel pitch p in micrometres and the real focal length f.
    static double npfRule( double focal, double fNumber, double pixelPitch );
    // The exposure behind an ND filter of `stops`: base * 2^stops.
    static double ndExposure( double base, int stops );
    // The frames of a star-trail series of `minutes`, one every `interval` seconds (a part frame
    // at the end counts).
    static int trailFrames( int minutes, int interval );
    // How far a star on the celestial equator moves in `minutes` (the sidereal rate).
    static double trailArc( int minutes );

    // The base shutter speeds offered for the ND calculator: whole stops from 1/8000 s to 30 s.
    static const int ShutterCount = 19;
    static const int DefaultShutter = 7;  // 1/60 s
    static double shutterSeconds( int index );
};
