// Host test of main/Calculator.cpp (the Tools page's calculator) and main/Units.cpp (its unit
// converter). Build and run: sh tests/run_host_tests.sh

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Calculator.h"
#include "Units.h"

static int failures = 0;

static void checkValue( const char *text, bool radians, double expected, double tolerance = 1e-12 )
{
    double value = 0;
    if ( !Calculator::evaluate( text, radians, value ) ) {
        printf( "FAIL %s: no value, expected %.15g\n", text, expected );
        ++failures;
    } else if ( fabs( value - expected ) > tolerance * ( fabs( expected ) > 1 ? fabs( expected ) : 1 ) ) {
        printf( "FAIL %s: %.15g, expected %.15g\n", text, value, expected );
        ++failures;
    }
}

static void checkError( const char *text )
{
    double value = 0;
    if ( Calculator::evaluate( text, false, value ) ) {
        printf( "FAIL %s: %.15g, expected an error\n", text, value );
        ++failures;
    }
}

static void checkText( const char *name, const char *text, const char *expected )
{
    if ( strcmp( text, expected ) != 0 ) {
        printf( "FAIL %s: \"%s\", expected \"%s\"\n", name, text, expected );
        ++failures;
    }
}

static void check( const char *name, double value, double expected, double tolerance )
{
    if ( fabs( value - expected ) > tolerance ) {
        printf( "FAIL %s: %.15g, expected %.15g\n", name, value, expected );
        ++failures;
    }
}

// Presses the keys of `keys`, one character each (Calculator::keyForChar).
static void type( Calculator &calculator, const char *keys )
{
    for ( const char *k = keys; *k != '\0'; ++k ) {
        calculator.press( Calculator::keyForChar( *k ) );
    }
}

static void testEvaluate()
{
    // Precedence, parentheses, unary minus, powers.
    checkValue( "1+2×3", false, 7 );
    checkValue( "(1+2)×3", false, 9 );
    checkValue( "12×102.875", false, 1234.5 );
    checkValue( "7÷2", false, 3.5 );
    checkValue( "10-4-3", false, 3 );
    checkValue( "100÷10÷5", false, 2 );
    checkValue( "-3+5", false, 2 );
    checkValue( "-3²", false, -9 );       // as written on paper: -(3²)
    checkValue( "(-3)²", false, 9 );
    checkValue( "2^10", false, 1024 );
    checkValue( "2^3^2", false, 512 );    // right associative
    checkValue( "2^-1", false, 0.5 );
    checkValue( "4^-1", false, 0.25 );    // the 1/x key
    checkValue( "-2^2", false, -4 );
    checkValue( "2--3", false, 5 );
    checkValue( "5²+1", false, 26 );
    checkValue( ".5×4", false, 2 );
    // Scientific notation.
    checkValue( "1.5E3", false, 1500 );
    checkValue( "1.5E-3", false, 0.0015 );
    checkValue( "2E3+1", false, 2001 );
    // π, the implied ×, parentheses closed at the end.
    checkValue( "π", false, 3.14159265358979 );
    checkValue( "2π", false, 2 * 3.14159265358979323846 );
    checkValue( "2(3+4)", false, 14 );
    checkValue( "(1+2)(3+4)", false, 21 );
    checkValue( "(1+2", false, 3 );
    checkValue( "2×(3+(4", false, 14 );
    checkValue( "√(16", false, 4 );
    checkValue( "3√(16)", false, 12 );
    // Degrees by default; exact at multiples of 90°.
    checkValue( "sin(30)", false, 0.5 );
    checkValue( "cos(60)", false, 0.5 );
    checkValue( "tan(45)", false, 1 );
    checkValue( "sin(180)", false, 0, 0 );
    checkValue( "cos(90)", false, 0, 0 );
    checkValue( "cos(-90)", false, 0, 0 );
    checkValue( "sin(-270)", false, 1 );
    checkValue( "sin(390)", false, 0.5 );
    checkValue( "asin(0.5)", false, 30 );
    checkValue( "acos(0)", false, 90 );
    checkValue( "atan(1)", false, 45 );
    checkValue( "2sin(30)", false, 1 );
    // Radians.
    checkValue( "sin(π÷6)", true, 0.5 );
    checkValue( "sin(π)", true, 0, 0 );
    checkValue( "cos(π÷2)", true, 0, 0 );
    checkValue( "atan(1)×4", true, 3.14159265358979 );
    // Logarithms.
    checkValue( "log(1000)", false, 3 );
    checkValue( "ln(1)", false, 0 );
    checkValue( "ln(2.718281828459045)", false, 1 );
    // Errors: incomplete, wrong signs, domains, division by zero, overflow.
    checkError( "" );
    checkError( "12×" );
    checkError( "×3" );
    checkError( "1.5E" );
    checkError( "1.2.3" );
    checkError( "." );
    checkError( "()" );
    checkError( "2)" );
    checkError( "5÷0" );
    checkError( "5÷(2-2)" );
    checkError( "√(-1)" );
    checkError( "log(0)" );
    checkError( "ln(-1)" );
    checkError( "asin(2)" );
    checkError( "tan(90)" );
    checkError( "tan(-270)" );
    checkError( "0^-1" );
    checkError( "10^400" );
    checkError( "(-8)^0.5" );
    checkError( "12:30" );  // h:m:s is the converter's, not an expression
    // Too deep a nesting is an error, not a stack overflow.
    checkError( "((((((((((((((((((((((((((((((1" );
    checkError( "-------------------------------1" );
}

static void testFormat()
{
    char text[32];
    Calculator::format( 1234.5, 11, text, sizeof( text ) );
    checkText( "1234.5", text, "1234.5" );
    Calculator::format( 1.0 / 3.0, 11, text, sizeof( text ) );
    checkText( "1/3 in 11", text, "0.333333333" );
    Calculator::format( 1.0 / 3.0, 16, text, sizeof( text ) );
    checkText( "1/3 in 16", text, "0.3333333333" );
    Calculator::format( -0.0, 11, text, sizeof( text ) );
    checkText( "-0", text, "0" );
    Calculator::format( 1.5e-7, 11, text, sizeof( text ) );
    checkText( "1.5E-7", text, "1.5E-7" );
    Calculator::format( 1e20, 11, text, sizeof( text ) );
    checkText( "1E20", text, "1E20" );
    Calculator::format( -1.2345678901e-100, 11, text, sizeof( text ) );
    checkText( "small", text, "-1.235E-100" );
    Calculator::format( 149597870.7, 11, text, sizeof( text ) );
    checkText( "AU in km", text, "149597870.7" );
    Calculator::format( 9460730472580.8, 11, text, sizeof( text ) );
    checkText( "light year", text, "9.46073E12" );
    Calculator::format( 9460730472580.8, 17, text, sizeof( text ) );
    checkText( "light year in 17", text, "9.460730473E12" );
    // A result written back as text evaluates to it (the operator after "=").
    double value = 0;
    Calculator::format( 1.5e-7, 16, text, sizeof( text ) );
    check( "read back", Calculator::evaluate( text, false, value ) ? value : -1, 1.5e-7, 1e-20 );
    checkText( "tail", Calculator::tail( "12×102.875", 5 ), "2.875" );
    checkText( "tail over ×", Calculator::tail( "12×102.875", 8 ), "×102.875" );
    checkText( "tail of short", Calculator::tail( "12", 5 ), "12" );
}

static void testKeys()
{
    Calculator calculator;
    double value = 0;
    type( calculator, "12*102.875" );
    checkText( "typed", calculator.text(), "12×102.875" );
    check( "live result", calculator.preview( value ) ? value : -1, 1234.5, 1e-9 );
    check( "typing", calculator.state(), Calculator::Typing, 0 );
    type( calculator, "=" );
    check( "result", calculator.state(), Calculator::Result, 0 );
    check( "result value", calculator.value(), 1234.5, 1e-9 );
    // An operator goes on from the result, a digit starts anew.
    type( calculator, "+1=" );
    checkText( "goes on", calculator.text(), "1234.5+1" );
    check( "goes on: value", calculator.value(), 1235.5, 1e-9 );
    type( calculator, "7" );
    checkText( "starts anew", calculator.text(), "7" );
    type( calculator, "C" );
    checkText( "cleared", calculator.text(), "" );
    check( "nothing to show", calculator.preview( value ), 0, 0 );
    // Incomplete: no live result, and "=" fails until it is finished.
    type( calculator, "12*" );
    check( "incomplete", calculator.preview( value ), 0, 0 );
    type( calculator, "=" );
    check( "failed", calculator.state(), Calculator::Failed, 0 );
    type( calculator, "3=" );
    check( "finished after failing", calculator.value(), 36, 1e-9 );
    // Back removes a function name as one sign.
    type( calculator, "C" );
    type( calculator, "fs30" );
    checkText( "sin from the functions", calculator.text(), "sin(30" );
    check( "digits back after sin", calculator.pad(), Calculator::DigitPad, 0 );
    type( calculator, "<<" );
    checkText( "back to sin(", calculator.text(), "sin(" );
    type( calculator, "<" );
    checkText( "back over sin(", calculator.text(), "" );
    // fn: digits -> functions -> inverse -> digits; DEG stays on the functions.
    type( calculator, "f" );
    check( "functions", calculator.pad(), Calculator::FunctionPad, 0 );
    type( calculator, "d" );
    check( "DEG keeps the pad", calculator.pad(), Calculator::FunctionPad, 0 );
    check( "radians", calculator.radians(), 1, 0 );
    checkText( "RAD label", calculator.label( Calculator::Angle ), "RAD" );
    type( calculator, "d" );
    type( calculator, "f" );
    check( "inverse", calculator.pad(), Calculator::InversePad, 0 );
    check( "inverse row", Calculator::keyAt( Calculator::InversePad, 0 ), Calculator::Asin, 0 );
    type( calculator, "S0.5=" );
    check( "asin", calculator.value(), 30, 1e-9 );
    type( calculator, "f" );
    type( calculator, "f" );
    type( calculator, "f" );
    check( "fn three times: digits", calculator.pad(), Calculator::DigitPad, 0 );
    // DEG after a result computes it again.
    type( calculator, "C" );
    type( calculator, "fs30=" );
    check( "sin(30°)", calculator.value(), 0.5, 1e-12 );
    type( calculator, "fd" );
    check( "sin(30 rad)", calculator.value(), sin( 30.0 ), 1e-12 );
    type( calculator, "dC" );
    // ± on the last number, on the exponent after EXP.
    type( calculator, "5m" );
    checkText( "minus", calculator.text(), "-5" );
    type( calculator, "m" );
    checkText( "plus again", calculator.text(), "5" );
    type( calculator, "*3m" );
    checkText( "minus after ×", calculator.text(), "5×-3" );
    type( calculator, "=" );
    check( "5×-3", calculator.value(), -15, 1e-12 );
    type( calculator, "C2-3m" );
    checkText( "minus after a subtraction", calculator.text(), "2--3" );
    type( calculator, "C1.5fem" );
    checkText( "EXP minus", calculator.text(), "1.5E-" );
    type( calculator, "7" );
    type( calculator, "fm" );
    checkText( "exponent's sign again", calculator.text(), "1.5E7" );
    type( calculator, "C(2)fm" );
    checkText( "no minus after a value", calculator.text(), "(2)" );
    type( calculator, "C*" );
    checkText( "a minus starts a number", calculator.text(), "×" );
    type( calculator, "Cfm" );
    checkText( "minus alone", calculator.text(), "-" );
    type( calculator, "fm" );
    checkText( "minus alone again", calculator.text(), "" );
    // x², 1/x and x^y go on from a result.
    type( calculator, "C3=fq=" );
    check( "3²", calculator.value(), 9, 1e-12 );
    type( calculator, "fi=" );
    check( "1/9", calculator.value(), 1.0 / 9.0, 1e-9 );
    // The text does not grow past its buffer.
    type( calculator, "C" );
    for ( int i = 0; i < 100; ++i ) {
        type( calculator, "1" );
    }
    check( "bounded", static_cast<double>( strlen( calculator.text() ) ), Calculator::MaxText - 1, 0 );
    // Colon mode (the converter's h:m:s).
    Calculator hms;
    hms.setColon( true );
    checkText( "colon label", hms.label( Calculator::Point ), ":" );
    type( hms, "12.30.15m" );
    checkText( "colon", hms.text(), "-12:30:15" );
}

static void testUnits()
{
    // Angle and time: 24 h = 360°.
    check( "12.5 h in degrees", Units::convert( Units::AngleTime, Units::Hours, Units::Degrees, 12.5 ), 187.5, 1e-12 );
    check( "187.5625° in hours", Units::convert( Units::AngleTime, Units::Degrees, Units::Hours, 187.5625 ),
           12.50416666666667, 1e-12 );
    check( "180° in radians", Units::convert( Units::AngleTime, Units::Degrees, Units::Radians, 180 ),
           3.14159265358979, 1e-12 );
    check( "π rad in hours", Units::convert( Units::AngleTime, Units::Radians, Units::Hours, 3.14159265358979323846 ),
           12, 1e-12 );
    // Distance: the IAU's astronomical unit, the Julian light year, the parsec (648000/π AU).
    check( "AU in km", Units::convert( Units::Distance, Units::AstronomicalUnits, Units::Kilometres, 1 ), 149597870.7,
           1e-6 );
    check( "light year in km", Units::convert( Units::Distance, Units::LightYears, Units::Kilometres, 1 ),
           9460730472580.8, 1e-2 );
    check( "parsec in light years", Units::convert( Units::Distance, Units::Parsecs, Units::LightYears, 1 ),
           3.261563777, 1e-9 );
    check( "parsec in AU", Units::convert( Units::Distance, Units::Parsecs, Units::AstronomicalUnits, 1 ),
           206264.80624709636, 1e-6 );
    check( "light year in AU", Units::convert( Units::Distance, Units::LightYears, Units::AstronomicalUnits, 1 ),
           63241.07708426628, 1e-6 );
    // Temperature.
    check( "100 °C in °F", Units::convert( Units::Temperature, Units::Celsius, Units::Fahrenheit, 100 ), 212, 1e-12 );
    check( "-40 °F in °C", Units::convert( Units::Temperature, Units::Fahrenheit, Units::Celsius, -40 ), -40, 1e-12 );
    check( "0 K in °C", Units::convert( Units::Temperature, Units::Kelvin, Units::Celsius, 0 ), -273.15, 1e-12 );
    check( "98.6 °F in K", Units::convert( Units::Temperature, Units::Fahrenheit, Units::Kelvin, 98.6 ), 310.15, 1e-9 );
    check( "same unit", Units::convert( Units::Temperature, Units::Kelvin, Units::Kelvin, 5 ), 5, 0 );
    check( "below absolute zero", Units::valid( Units::Temperature, Units::Celsius, -273.16 ), 0, 0 );
    check( "absolute zero", Units::valid( Units::Temperature, Units::Kelvin, 0 ), 1, 0 );
    check( "negative angle", Units::valid( Units::AngleTime, Units::Degrees, -10 ), 1, 0 );
    check( "unit counts", Units::unitCount( Units::Distance ), 4, 0 );
    check( "unit counts", Units::unitCount( Units::Temperature ), 3, 0 );

    // h:m:s read and split.
    double hours = 0;
    check( "12:30:15", Units::parseHms( "12:30:15", hours ) ? hours : -1, 12.50416666666667, 1e-12 );
    check( "12:30", Units::parseHms( "12:30", hours ) ? hours : -1, 12.5, 1e-12 );
    check( "12", Units::parseHms( "12", hours ) ? hours : -1, 12, 1e-12 );
    check( "-1:30", Units::parseHms( "-1:30", hours ) ? hours : 99, -1.5, 1e-12 );
    check( "minutes over 59", Units::parseHms( "1:60", hours ), 0, 0 );
    check( "empty", Units::parseHms( "", hours ), 0, 0 );
    check( "three colons", Units::parseHms( "1:2:3:4", hours ), 0, 0 );
    check( "empty field", Units::parseHms( "1::3", hours ), 0, 0 );
    bool negative = false;
    int h = 0;
    int m = 0;
    int s = 0;
    check( "split", Units::splitHms( 12.50416666666667, negative, h, m, s ), 1, 0 );
    check( "split h", h, 12, 0 );
    check( "split m", m, 30, 0 );
    check( "split s", s, 15, 0 );
    // 59.6 s rounds up into the next minute and hour.
    Units::splitHms( 1 + 59.0 / 60 + 59.6 / 3600, negative, h, m, s );
    check( "carry h", h, 2, 0 );
    check( "carry m", m, 0, 0 );
    check( "carry s", s, 0, 0 );
    Units::splitHms( -1.5, negative, h, m, s );
    check( "negative", negative, 1, 0 );
    check( "negative h", h, 1, 0 );
    check( "negative m", m, 30, 0 );
    check( "too large", Units::splitHms( 1e7, negative, h, m, s ), 0, 0 );
}

int main()
{
    testEvaluate();
    testFormat();
    testKeys();
    testUnits();
    if ( failures == 0 ) {
        printf( "Calculator: all tests passed\n" );
    }
    return failures == 0 ? 0 : 1;
}
