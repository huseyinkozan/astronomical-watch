#include "Magnetic.h"

#include <cmath>

static const double s_pi = 3.14159265358979323846;
static const int s_degree = 12;
static const double s_epoch = 2025.0;
// The model's reference radius and the WGS 84 ellipsoid, km.
static const double s_referenceRadius = 6371.2;
static const double s_equatorRadius = 6378.137;
static const double s_flattening = 1 / 298.257223563;

struct Coefficient
{
    float g;
    float h;
    float gPerYear;
    float hPerYear;
};

// WMM2025 (tools/make_wmm_table.py): n = 1 to 12, m = 0 to n, nT and nT a year.
static const Coefficient s_coefficients[] = {
    { -29351.8f, 0.0f, 12.0f, 0.0f },  // 1 0
    { -1410.8f, 4545.4f, 9.7f, -21.5f },  // 1 1
    { -2556.6f, 0.0f, -11.6f, 0.0f },  // 2 0
    { 2951.1f, -3133.6f, -5.2f, -27.7f },  // 2 1
    { 1649.3f, -815.1f, -8.0f, -12.1f },  // 2 2
    { 1361.0f, 0.0f, -1.3f, 0.0f },  // 3 0
    { -2404.1f, -56.6f, -4.2f, 4.0f },  // 3 1
    { 1243.8f, 237.5f, 0.4f, -0.3f },  // 3 2
    { 453.6f, -549.5f, -15.6f, -4.1f },  // 3 3
    { 895.0f, 0.0f, -1.6f, 0.0f },  // 4 0
    { 799.5f, 278.6f, -2.4f, -1.1f },  // 4 1
    { 55.7f, -133.9f, -6.0f, 4.1f },  // 4 2
    { -281.1f, 212.0f, 5.6f, 1.6f },  // 4 3
    { 12.1f, -375.6f, -7.0f, -4.4f },  // 4 4
    { -233.2f, 0.0f, 0.6f, 0.0f },  // 5 0
    { 368.9f, 45.4f, 1.4f, -0.5f },  // 5 1
    { 187.2f, 220.2f, 0.0f, 2.2f },  // 5 2
    { -138.7f, -122.9f, 0.6f, 0.4f },  // 5 3
    { -142.0f, 43.0f, 2.2f, 1.7f },  // 5 4
    { 20.9f, 106.1f, 0.9f, 1.9f },  // 5 5
    { 64.4f, 0.0f, -0.2f, 0.0f },  // 6 0
    { 63.8f, -18.4f, -0.4f, 0.3f },  // 6 1
    { 76.9f, 16.8f, 0.9f, -1.6f },  // 6 2
    { -115.7f, 48.8f, 1.2f, -0.4f },  // 6 3
    { -40.9f, -59.8f, -0.9f, 0.9f },  // 6 4
    { 14.9f, 10.9f, 0.3f, 0.7f },  // 6 5
    { -60.7f, 72.7f, 0.9f, 0.9f },  // 6 6
    { 79.5f, 0.0f, -0.0f, 0.0f },  // 7 0
    { -77.0f, -48.9f, -0.1f, 0.6f },  // 7 1
    { -8.8f, -14.4f, -0.1f, 0.5f },  // 7 2
    { 59.3f, -1.0f, 0.5f, -0.8f },  // 7 3
    { 15.8f, 23.4f, -0.1f, 0.0f },  // 7 4
    { 2.5f, -7.4f, -0.8f, -1.0f },  // 7 5
    { -11.1f, -25.1f, -0.8f, 0.6f },  // 7 6
    { 14.2f, -2.3f, 0.8f, -0.2f },  // 7 7
    { 23.2f, 0.0f, -0.1f, 0.0f },  // 8 0
    { 10.8f, 7.1f, 0.2f, -0.2f },  // 8 1
    { -17.5f, -12.6f, 0.0f, 0.5f },  // 8 2
    { 2.0f, 11.4f, 0.5f, -0.4f },  // 8 3
    { -21.7f, -9.7f, -0.1f, 0.4f },  // 8 4
    { 16.9f, 12.7f, 0.3f, -0.5f },  // 8 5
    { 15.0f, 0.7f, 0.2f, -0.6f },  // 8 6
    { -16.8f, -5.2f, -0.0f, 0.3f },  // 8 7
    { 0.9f, 3.9f, 0.2f, 0.2f },  // 8 8
    { 4.6f, 0.0f, -0.0f, 0.0f },  // 9 0
    { 7.8f, -24.8f, -0.1f, -0.3f },  // 9 1
    { 3.0f, 12.2f, 0.1f, 0.3f },  // 9 2
    { -0.2f, 8.3f, 0.3f, -0.3f },  // 9 3
    { -2.5f, -3.3f, -0.3f, 0.3f },  // 9 4
    { -13.1f, -5.2f, 0.0f, 0.2f },  // 9 5
    { 2.4f, 7.2f, 0.3f, -0.1f },  // 9 6
    { 8.6f, -0.6f, -0.1f, -0.2f },  // 9 7
    { -8.7f, 0.8f, 0.1f, 0.4f },  // 9 8
    { -12.9f, 10.0f, -0.1f, 0.1f },  // 9 9
    { -1.3f, 0.0f, 0.1f, 0.0f },  // 10 0
    { -6.4f, 3.3f, 0.0f, 0.0f },  // 10 1
    { 0.2f, 0.0f, 0.1f, -0.0f },  // 10 2
    { 2.0f, 2.4f, 0.1f, -0.2f },  // 10 3
    { -1.0f, 5.3f, -0.0f, 0.1f },  // 10 4
    { -0.6f, -9.1f, -0.3f, -0.1f },  // 10 5
    { -0.9f, 0.4f, 0.0f, 0.1f },  // 10 6
    { 1.5f, -4.2f, -0.1f, 0.0f },  // 10 7
    { 0.9f, -3.8f, -0.1f, -0.1f },  // 10 8
    { -2.7f, 0.9f, -0.0f, 0.2f },  // 10 9
    { -3.9f, -9.1f, -0.0f, -0.0f },  // 10 10
    { 2.9f, 0.0f, 0.0f, 0.0f },  // 11 0
    { -1.5f, 0.0f, -0.0f, -0.0f },  // 11 1
    { -2.5f, 2.9f, 0.0f, 0.1f },  // 11 2
    { 2.4f, -0.6f, 0.0f, -0.0f },  // 11 3
    { -0.6f, 0.2f, 0.0f, 0.1f },  // 11 4
    { -0.1f, 0.5f, -0.1f, -0.0f },  // 11 5
    { -0.6f, -0.3f, 0.0f, -0.0f },  // 11 6
    { -0.1f, -1.2f, -0.0f, 0.1f },  // 11 7
    { 1.1f, -1.7f, -0.1f, -0.0f },  // 11 8
    { -1.0f, -2.9f, -0.1f, 0.0f },  // 11 9
    { -0.2f, -1.8f, -0.1f, 0.0f },  // 11 10
    { 2.6f, -2.3f, -0.1f, 0.0f },  // 11 11
    { -2.0f, 0.0f, 0.0f, 0.0f },  // 12 0
    { -0.2f, -1.3f, 0.0f, -0.0f },  // 12 1
    { 0.3f, 0.7f, -0.0f, 0.0f },  // 12 2
    { 1.2f, 1.0f, -0.0f, -0.1f },  // 12 3
    { -1.3f, -1.4f, -0.0f, 0.1f },  // 12 4
    { 0.6f, -0.0f, -0.0f, -0.0f },  // 12 5
    { 0.6f, 0.6f, 0.1f, -0.0f },  // 12 6
    { 0.5f, -0.1f, -0.0f, -0.0f },  // 12 7
    { -0.1f, 0.8f, 0.0f, 0.0f },  // 12 8
    { -0.4f, 0.1f, 0.0f, -0.0f },  // 12 9
    { -0.2f, -1.0f, -0.1f, -0.0f },  // 12 10
    { -1.3f, 0.1f, -0.0f, 0.0f },  // 12 11
    { -0.7f, 0.2f, -0.1f, -0.1f },  // 12 12
};
static_assert( sizeof( s_coefficients ) / sizeof( s_coefficients[0] ) == s_degree * ( s_degree + 3 ) / 2,
               "n = 1 to 12, m = 0 to n" );

static double toRadians( double degrees )
{
    return degrees * s_pi / 180.0;
}

Magnetic::Field Magnetic::field( double latitude, double longitude, double heightKm, double decimalYear )
{
    // Geodetic to geocentric spherical coordinates (WMM report, equation 7).
    const double phi = toRadians( latitude );
    const double eccentricity2 = s_flattening * ( 2 - s_flattening );
    const double curvature = s_equatorRadius / std::sqrt( 1 - eccentricity2 * std::sin( phi ) * std::sin( phi ) );
    const double p = ( curvature + heightKm ) * std::cos( phi );
    const double zc = ( curvature * ( 1 - eccentricity2 ) + heightKm ) * std::sin( phi );
    const double radius = std::sqrt( p * p + zc * zc );
    const double phiC = std::asin( zc / radius );
    const double x = std::sin( phiC );
    double s = std::cos( phiC );
    // Away from the poles' singularity in the east component.
    s = s < 1e-9 ? 1e-9 : s;
    const double lambda = toRadians( longitude );
    const double years = decimalYear - s_epoch;

    double ratio[s_degree + 1] = {};
    for ( int n = 1; n <= s_degree; ++n ) {
        ratio[n] = std::pow( s_referenceRadius / radius, n + 2 );
    }
    // Schmidt semi-normalised associated Legendre functions of sin(phiC) and their derivatives
    // by phiC, column by column (m), P(m, m) carried from one column to the next.
    double north = 0;
    double east = 0;
    double down = 0;
    double pmm = 1;
    double dpmm = 0;
    for ( int m = 0; m <= s_degree; ++m ) {
        if ( m == 1 ) {
            pmm = s;
            dpmm = -x;
        } else if ( m >= 2 ) {
            const double k = std::sqrt( ( 2.0 * m - 1 ) / ( 2.0 * m ) );
            const double next = k * s * pmm;
            dpmm = k * ( s * dpmm - x * pmm );
            pmm = next;
        }
        const double cosM = std::cos( m * lambda );
        const double sinM = std::sin( m * lambda );
        double p1 = pmm;  // P(n - 1, m)
        double dp1 = dpmm;
        double p2 = 0;    // P(n - 2, m)
        double dp2 = 0;
        for ( int n = m < 1 ? 1 : m; n <= s_degree; ++n ) {
            double pnm = pmm;
            double dpnm = dpmm;
            if ( n > m ) {
                const double c = std::sqrt( static_cast<double>( ( n - 1 ) * ( n - 1 ) - m * m ) );
                const double d = std::sqrt( static_cast<double>( n * n - m * m ) );
                pnm = ( ( 2 * n - 1 ) * x * p1 - c * p2 ) / d;
                dpnm = ( ( 2 * n - 1 ) * ( s * p1 + x * dp1 ) - c * dp2 ) / d;
                p2 = p1;
                dp2 = dp1;
                p1 = pnm;
                dp1 = dpnm;
            }
            const Coefficient &k = s_coefficients[n * ( n + 1 ) / 2 - 1 + m];
            const double g = k.g + years * k.gPerYear;
            const double h = k.h + years * k.hPerYear;
            const double cosine = g * cosM + h * sinM;
            north -= ratio[n] * cosine * dpnm;
            east += ratio[n] * m * ( g * sinM - h * cosM ) * pnm / s;
            down -= ratio[n] * ( n + 1 ) * cosine * pnm;
        }
    }
    // Back to the geodetic frame (equation 17).
    const double turn = phiC - phi;
    Field result = {};
    result.x = north * std::cos( turn ) - down * std::sin( turn );
    result.y = east;
    result.z = north * std::sin( turn ) + down * std::cos( turn );
    return result;
}

double Magnetic::declination( double latitude, double longitude, double decimalYear )
{
    const Field f = field( latitude, longitude, 0, decimalYear );
    return std::atan2( f.y, f.x ) * 180.0 / s_pi;
}

double Magnetic::decimalYear( int64_t time )
{
    // 2025-01-01 00:00 UTC; a mean year is close enough for a model that changes by 0.1 degree a year.
    return s_epoch + ( time - 1735689600LL ) / ( 365.2425 * 86400.0 );
}
