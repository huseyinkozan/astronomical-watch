#include "Calculator.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const double s_pi = 3.14159265358979323846;

// What each key writes into the text (nullptr: a key that does something else).
static const char *s_tokens[Calculator::KeyCount] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "+", "-", "×", "÷", nullptr, nullptr, nullptr, nullptr,
    "sin(", "cos(", "tan(", "asin(", "acos(", "atan(", "√(", "²", "^", "π", "log(", "ln(", "(", ")", "^-1",
    nullptr, "E", nullptr };
static const char *s_labels[Calculator::KeyCount] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "+", "-", "×", "÷", "=", "C", "⌫", "fn",
    "sin", "cos", "tan", "asin", "acos", "atan", "√", "x²", "x^y", "π", "log", "ln", "(", ")", "1/x",
    "DEG", "EXP", "±" };

// The keypads (mock-ups research/mockups/tools-sheet.png and tools-sheet-3.png); fn keeps its
// place on all of them, so that it switches back where it switched.
using K = Calculator::Key;
static const K s_pads[Calculator::PadCount][Calculator::Cells] = {
    { K::Clear, K::Back, K::Divide, K::Fn,
      K::Key7, K::Key8, K::Key9, K::Times,
      K::Key4, K::Key5, K::Key6, K::Minus,
      K::Key1, K::Key2, K::Key3, K::Plus,
      K::Key0, K::Point, K::Equals, K::Equals },
    { K::Sin, K::Cos, K::Tan, K::Fn,
      K::Sqrt, K::Square, K::Power, K::Pi,
      K::Log, K::Ln, K::Open, K::Close,
      K::Reciprocal, K::Angle, K::Exp, K::Sign,
      K::Clear, K::Clear, K::Equals, K::Equals },
    { K::Asin, K::Acos, K::Atan, K::Fn,
      K::Sqrt, K::Square, K::Power, K::Pi,
      K::Log, K::Ln, K::Open, K::Close,
      K::Reciprocal, K::Angle, K::Exp, K::Sign,
      K::Clear, K::Clear, K::Equals, K::Equals } };

// Function names with their "(", removed as one sign by the back key.
static const char *s_functions[] = { "asin(", "acos(", "atan(", "sin(", "cos(", "tan(", "log(", "ln(", "√(" };
static const int s_functionCount = sizeof( s_functions ) / sizeof( s_functions[0] );

static bool endsWith( const char *text, int length, const char *token )
{
    const int n = static_cast<int>( strlen( token ) );
    return length >= n && strncmp( text + length - n, token, n ) == 0;
}

static bool isDigit( char c )
{
    return c >= '0' && c <= '9';
}

// sin in degrees, exact at the multiples of 90° (sin(180°) is 0, not 1.2E-16).
static double sinDegrees( double degrees )
{
    double r = fmod( degrees, 360.0 );
    if ( r < 0 ) {
        r += 360.0;
    }
    if ( r == 0 || r == 180 ) {
        return 0;
    }
    if ( r == 90 ) {
        return 1;
    }
    if ( r == 270 ) {
        return -1;
    }
    return sin( r * s_pi / 180.0 );
}

// π is not exact in binary: sin(π) is 1.2E-16. A result that small against its angle is 0.
static double snapRadians( double angle, double result )
{
    return fabs( angle ) > 1 && fabs( result ) < 1e-15 * fabs( angle ) ? 0 : result;
}

namespace {

// Recursive descent over the text: expr, term (× ÷ and the implied ×), unary minus, power
// (right associative), x², then numbers, π, parentheses and functions.
struct Parser
{
    const char *p;
    bool radians;
    bool ok;
    int depth;

    bool take( const char *token )
    {
        const size_t n = strlen( token );
        if ( strncmp( p, token, n ) == 0 ) {
            p += n;
            return true;
        }
        return false;
    }

    double fail()
    {
        ok = false;
        return 0;
    }

    // A "2π" or "2sin(30)": a parenthesis, π, √ or a function name follows directly.
    bool impliedTimes() const
    {
        return *p == '(' || strncmp( p, "π", 2 ) == 0 || strncmp( p, "√", 3 ) == 0 || ( *p >= 'a' && *p <= 'z' );
    }

    double expr()
    {
        double value = term();
        while ( ok ) {
            if ( take( "+" ) ) {
                value += term();
            } else if ( take( "-" ) ) {
                value -= term();
            } else {
                break;
            }
        }
        return value;
    }

    double term()
    {
        double value = unary();
        while ( ok ) {
            if ( take( "×" ) ) {
                value *= unary();
            } else if ( take( "÷" ) ) {
                const double divisor = unary();
                if ( divisor == 0 ) {
                    return fail();
                }
                value /= divisor;
            } else if ( impliedTimes() ) {
                value *= unary();
            } else {
                break;
            }
        }
        return value;
    }

    double unary()
    {
        // A bound on the nesting keeps the recursion within the LVGL task's stack.
        if ( ++depth > 24 ) {
            return fail();
        }
        double value = 0;
        if ( take( "-" ) ) {
            value = -unary();
        } else if ( take( "+" ) ) {
            value = unary();
        } else {
            value = power();
        }
        --depth;
        return value;
    }

    double power()
    {
        const double base = postfix();
        if ( ok && take( "^" ) ) {
            return pow( base, unary() );
        }
        return base;
    }

    double postfix()
    {
        double value = primary();
        while ( ok && take( "²" ) ) {
            value *= value;
        }
        return value;
    }

    double number()
    {
        char buffer[40];
        int n = 0;
        bool point = false;
        while ( isDigit( *p ) || ( *p == '.' && !point ) ) {
            point = point || *p == '.';
            if ( n < 30 ) {
                buffer[n++] = *p;
            }
            ++p;
        }
        if ( n == 1 && point ) {
            return fail();  // a point alone
        }
        if ( *p == 'E' ) {
            buffer[n++] = *p++;
            if ( *p == '-' || *p == '+' ) {
                buffer[n++] = *p++;
            }
            if ( !isDigit( *p ) ) {
                return fail();  // "1.5E" not finished
            }
            while ( isDigit( *p ) ) {
                if ( n < 38 ) {
                    buffer[n++] = *p;
                }
                ++p;
            }
        }
        buffer[n] = '\0';
        // A second point right after the number ("1.2.3").
        if ( *p == '.' ) {
            return fail();
        }
        return strtod( buffer, nullptr );
    }

    // The value inside a parenthesis just opened; one left open at the end of the text closes there.
    double inner()
    {
        const double value = expr();
        if ( ok && !take( ")" ) && *p != '\0' ) {
            return fail();
        }
        return value;
    }

    double function( int index, double x )
    {
        const double toAngle = radians ? 1.0 : 180.0 / s_pi;
        double result = 0;
        switch ( index ) {
        case 0:
        case 1:
            if ( x < -1 || x > 1 ) {
                return fail();
            }
            result = ( index == 0 ? asin( x ) : acos( x ) ) * toAngle;
            break;
        case 2:
            result = atan( x ) * toAngle;
            break;
        case 3:
        case 4:
        case 5: {
            double s = 0;
            double c = 0;
            if ( radians ) {
                s = snapRadians( x, sin( x ) );
                c = snapRadians( x, cos( x ) );
            } else {
                s = sinDegrees( x );
                c = sinDegrees( x + 90 );
            }
            if ( index == 5 && c == 0 ) {
                return fail();  // tan(90°)
            }
            result = index == 3 ? s : ( index == 4 ? c : s / c );
            break;
        }
        case 6:
        case 7:
            if ( x <= 0 ) {
                return fail();
            }
            result = index == 6 ? log10( x ) : log( x );
            break;
        default:
            if ( x < 0 ) {
                return fail();
            }
            result = sqrt( x );
            break;
        }
        return result;
    }

    double primary()
    {
        if ( isDigit( *p ) || *p == '.' ) {
            return number();
        }
        if ( take( "π" ) ) {
            return s_pi;
        }
        if ( take( "(" ) ) {
            return inner();
        }
        for ( int i = 0; i < s_functionCount; ++i ) {
            if ( take( s_functions[i] ) ) {
                const double x = inner();
                return ok ? function( i, x ) : 0;
            }
        }
        return fail();  // an operand is missing ("12×") or a sign does not belong here
    }
};

}  // namespace

Calculator::Key Calculator::keyAt( int pad, int cell )
{
    if ( pad < 0 || pad >= PadCount || cell < 0 || cell >= Cells ) {
        return NoKey;
    }
    return s_pads[pad][cell];
}

const char *Calculator::label( Key key ) const
{
    if ( key == Angle ) {
        return m_radians ? "RAD" : "DEG";
    }
    if ( key == Point && m_colon ) {
        return ":";
    }
    return key >= 0 && key < KeyCount ? s_labels[key] : "";
}

bool Calculator::accented( Key key ) const
{
    return key == Clear || key == Equals || ( key == Fn && m_pad != DigitPad );
}

void Calculator::clear()
{
    m_text[0] = '\0';
    m_state = Typing;
}

void Calculator::press( Key key )
{
    if ( key < 0 || key >= KeyCount ) {
        return;
    }
    if ( key == Fn ) {
        m_pad = ( m_pad + 1 ) % PadCount;
        return;
    }
    if ( key == Angle ) {
        m_radians = !m_radians;
        // A result shown is computed again in the new unit.
        if ( m_state == Result && !evaluate( m_text, m_radians, m_value ) ) {
            m_state = Failed;
        }
        return;
    }
    if ( key == Clear ) {
        clear();
        return;
    }
    // A key of the functions writes one thing; then the digits come back.
    const bool fromFunctions = m_pad != DigitPad;
    if ( key == Equals ) {
        m_pad = DigitPad;
        if ( m_text[0] != '\0' ) {
            m_state = evaluate( m_text, m_radians, m_value ) ? Result : Failed;
        }
        return;
    }
    if ( key == Back ) {
        m_state = Typing;
        const int length = static_cast<int>( strlen( m_text ) );
        int cut = length;
        for ( int i = 0; i < s_functionCount && cut == length; ++i ) {
            if ( endsWith( m_text, length, s_functions[i] ) ) {
                cut = length - static_cast<int>( strlen( s_functions[i] ) );
            }
        }
        if ( cut == length && length > 0 ) {
            // One UTF-8 character: its continuation bytes too.
            cut = length - 1;
            while ( cut > 0 && ( static_cast<unsigned char>( m_text[cut] ) & 0xC0 ) == 0x80 ) {
                --cut;
            }
        }
        m_text[cut] = '\0';
        return;
    }
    if ( m_state == Result ) {
        const bool goesOn = key == Plus || key == Minus || key == Times || key == Divide || key == Power ||
                            key == Square || key == Reciprocal;
        if ( goesOn ) {
            format( m_value, 16, m_text, MaxText );
        } else {
            m_text[0] = '\0';
        }
    }
    m_state = Typing;
    if ( key == Sign ) {
        toggleSign();
    } else if ( key == Point && m_colon ) {
        append( ":" );
    } else {
        append( s_tokens[key] );
    }
    if ( fromFunctions ) {
        m_pad = DigitPad;
    }
}

bool Calculator::append( const char *token )
{
    if ( token == nullptr ) {
        return false;
    }
    const size_t length = strlen( m_text );
    if ( length + strlen( token ) >= sizeof( m_text ) ) {
        return false;
    }
    memcpy( m_text + length, token, strlen( token ) + 1 );
    return true;
}

// Whether the text before `index` ends with a value, so that a "-" there subtracts.
static bool afterOperand( const char *text, int index )
{
    if ( index == 0 ) {
        return false;
    }
    const char c = text[index - 1];
    return isDigit( c ) || c == '.' || c == ')' || endsWith( text, index, "π" ) || endsWith( text, index, "²" );
}

void Calculator::toggleSign()
{
    const int length = static_cast<int>( strlen( m_text ) );
    int start = length;
    while ( start > 0 && ( isDigit( m_text[start - 1] ) || m_text[start - 1] == '.' || m_text[start - 1] == ':' ) ) {
        --start;
    }
    // Where the minus goes: at the number's start, or after its "E" (the exponent's sign).
    int at = start;
    if ( start == length ) {
        // No number at the end: a minus to start one, unless a value ends the text.
        if ( afterOperand( m_text, length ) ) {
            return;
        }
    }
    if ( at > 0 && m_text[at - 1] == '-' && ( at == 1 || m_text[at - 2] == 'E' || !afterOperand( m_text, at - 1 ) ) ) {
        // A minus that is a sign (not a subtraction) goes again.
        memmove( m_text + at - 1, m_text + at, length - at + 1 );
        return;
    }
    if ( length + 1 >= MaxText ) {
        return;
    }
    memmove( m_text + at + 1, m_text + at, length - at + 1 );
    m_text[at] = '-';
}

const char *Calculator::text() const
{
    return m_text;
}

Calculator::State Calculator::state() const
{
    return static_cast<State>( m_state );
}

int Calculator::pad() const
{
    return m_pad;
}

bool Calculator::radians() const
{
    return m_radians;
}

double Calculator::value() const
{
    return m_value;
}

bool Calculator::preview( double &result ) const
{
    return m_text[0] != '\0' && evaluate( m_text, m_radians, result );
}

void Calculator::setColon( bool colon )
{
    m_colon = colon;
}

bool Calculator::evaluate( const char *text, bool radians, double &result )
{
    Parser parser = { text, radians, true, 0 };
    const double value = parser.expr();
    if ( !parser.ok || *parser.p != '\0' || !std::isfinite( value ) ) {
        return false;
    }
    result = value;
    return true;
}

void Calculator::format( double value, int maxChars, char *out, int outSize )
{
    if ( value == 0 ) {
        value = 0;  // no "-0"
    }
    char text[40] = {};
    for ( int precision = 10; precision >= 1; --precision ) {
        char raw[32];
        snprintf( raw, sizeof( raw ), "%.*g", precision, value );
        // "1.5e-07" -> "1.5E-7", "1e+20" -> "1E20".
        char *e = strchr( raw, 'e' );
        if ( e == nullptr ) {
            snprintf( text, sizeof( text ), "%s", raw );
        } else {
            *e = '\0';
            const char *exponent = e + 2;
            while ( *exponent == '0' && exponent[1] != '\0' ) {
                ++exponent;
            }
            snprintf( text, sizeof( text ), "%sE%s%s", raw, e[1] == '-' ? "-" : "", exponent );
        }
        if ( static_cast<int>( strlen( text ) ) <= maxChars ) {
            break;
        }
    }
    snprintf( out, outSize, "%s", text );
}

const char *Calculator::tail( const char *text, int maxChars )
{
    const char *start = text + strlen( text );
    int count = 0;
    while ( start > text && count < maxChars ) {
        --start;
        while ( start > text && ( static_cast<unsigned char>( *start ) & 0xC0 ) == 0x80 ) {
            --start;
        }
        ++count;
    }
    return start;
}
