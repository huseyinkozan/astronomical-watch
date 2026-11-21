#pragma once

// The watch's tilt from the accelerometer (the spirit level and the alidade): the sensor's axes
// turned into the picture's, a low-pass filter and the angles. Plain C++ for the host tests
// (tests/TiltTest.cpp).
//
// The QMI8658's axes against the screen (docs/hardware.md, QMI8658A section): +X towards the top
// edge, +Y towards the right edge, +Z into the screen (lying face up it reads z = -1 g). The
// picture can be mirrored (Settings::Mirror, bit 0 left-right, bit 1 top-bottom): the vector is
// given in the picture's directions, so that "right" and "up" are those the user sees.
class Tilt
{
public:
    // The direction away from the ground (what a still accelerometer measures), in g: towards
    // the picture's right edge, its top edge and out of the screen.
    struct Vector
    {
        double right;
        double up;
        double out;
    };

    static Vector fromSensor( int xMg, int yMg, int zMg, int mirror );

    // Exponential smoothing of the samples, about 25 a second: each new one counts a quarter
    // (a time constant of about 0.15 s), smooth enough for the bubble and quick to follow.
    void add( const Vector &sample );
    void reset();
    bool valid() const;
    Vector value() const;

    // Angles in degrees. A still watch only; while it moves they mean little.
    // The altitude of the direction from the bottom edge to the top edge (the bubble level's
    // pitch: positive with the top edge raised) and of the direction from the left edge to the
    // right edge (roll: positive with the right edge raised).
    static double pitch( const Vector &v );
    static double roll( const Vector &v );
    // The angle between the screen and the horizontal plane: the slope of a surface the watch
    // lies on, 0 to 180 (90 upright).
    static double slope( const Vector &v );
    // Sighting along the top edge (the alidade, the level's sighting mode): the watch held
    // upright, the edge pointed at the target. The edge's altitude above or below the horizon,
    // 0 to 90, the same from either end (and along the bottom edge, which is parallel).
    static double edgeAltitude( const Vector &v );

    // Sighting: the height of a target seen `altitude` degrees above the eye from `distance`
    // away (the same unit), and the distance of a target of `height` above the eye. False when
    // the angle gives no answer (0 or 90 degrees).
    static bool heightFromDistance( double distance, double altitude, double &height );
    static bool distanceFromHeight( double height, double altitude, double &distance );

    // The bubble's offset from the centre for the level's rings, as a fraction of the outer ring
    // (`right`, `up`, each -1 to 1): the square root of the slope over 45 degrees, so that a
    // degree moves it visibly and the 30-45 degree slope band still fits; beyond 45 it stays on
    // the outer ring, in the direction of the higher edge.
    static void bubble( const Vector &v, double &right, double &up );
    // The ring's radius fraction of a slope (degrees), as bubble() places it.
    static double ringFraction( double slopeDegrees );

private:
    Vector m_value = { 0, 0, 0 };
    bool m_valid = false;
};
