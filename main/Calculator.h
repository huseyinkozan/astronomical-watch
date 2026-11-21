#pragma once

// The Tools page's calculator: an expression typed with keys, evaluated with the usual
// precedence (parentheses, then ^ and x², unary minus, × and ÷, + and -), shown as a live result
// while it is typed and as the result after "=". Plain C++ for the host tests; the keypad
// (Keypad) only draws the keys and calls press().
//
// The text is the expression as shown, in UTF-8 with the watch font's signs (× ÷ √ π ²), so that
// what is shown is what is evaluated. Functions open their parenthesis ("sin("); parentheses
// left open at the end are closed by the evaluation. A number before "(", π, √ or a function
// multiplies ("2π"). Trigonometry in degrees by default, in radians after the DEG key.
class Calculator
{
public:
    enum Key
    {
        Key0,
        Key1,
        Key2,
        Key3,
        Key4,
        Key5,
        Key6,
        Key7,
        Key8,
        Key9,
        Point,       // "." (":" in colon mode)
        Plus,
        Minus,
        Times,
        Divide,
        Equals,
        Clear,
        Back,        // removes the last sign, a whole function name with its "("
        Fn,          // digits -> functions -> inverse functions -> digits
        Sin,
        Cos,
        Tan,
        Asin,
        Acos,
        Atan,
        Sqrt,
        Square,      // x²
        Power,       // x^y
        Pi,
        Log,         // base 10
        Ln,
        Open,
        Close,
        Reciprocal,  // 1/x: "^-1" after the value
        Angle,       // degrees <-> radians
        Exp,         // "E": a power of ten in the number
        Sign,        // ±: the sign of the last number (of its exponent after "E")
        NoKey,
        KeyCount = NoKey
    };

    // The keypads: 5 rows of 4 cells; a key over two cells is listed in both.
    enum Pad
    {
        DigitPad,
        FunctionPad,
        InversePad,  // the function pad with asin, acos and atan in the first row
        PadCount
    };
    static const int Columns = 4;
    static const int Rows = 5;
    static const int Cells = Columns * Rows;
    static Key keyAt( int pad, int cell );
    // The key's sign; the Angle key shows the mode ("DEG" or "RAD"), Point ":" in colon mode.
    const char *label( Key key ) const;
    // The keys that do the main things (C and =), drawn in amber; fn too while the functions show.
    bool accented( Key key ) const;

    // A key for one ASCII character (host tests, the development command "B keys"): digits
    // . + - * / = C < (back) f (fn) s c t (sin cos tan) S O T (their inverses) r (√) q (x²) ^
    // p (π) l (log) n (ln) ( ) i (1/x) d (DEG) e (EXP) m (±); NoKey for any other.
    static Key keyForChar( char c );

    static const int MaxText = 64;

    enum State
    {
        Typing,  // live result when the text evaluates
        Result,  // after "=": the result
        Failed   // after "=": the text does not evaluate
    };

    // Applies a key. Equals evaluates; any other key after a result starts a new text, or goes
    // on from the result for an operator (+ - × ÷ ^ x² 1/x).
    void press( Key key );
    void clear();

    const char *text() const;
    State state() const;
    int pad() const;
    bool radians() const;
    // The result after "=" (state Result).
    double value() const;
    // The live result while typing: false when the text is empty or does not evaluate yet.
    bool preview( double &result ) const;

    // The converter's entry of h:m:s: the point key writes ":" (not evaluated here).
    void setColon( bool colon );

    // Evaluates `text`; false on a syntax error, a division by zero, a value outside a
    // function's domain or an infinite result.
    static bool evaluate( const char *text, bool radians, double &result );
    // `value` in at most `maxChars` characters: up to 10 significant digits, a power of ten as
    // "E" ("1.5E-7", which evaluate() reads back), "-0" as "0".
    static void format( double value, int maxChars, char *out, int outSize );
    // The last `maxChars` characters of UTF-8 `text` (where to start showing it).
    static const char *tail( const char *text, int maxChars );

private:
    bool append( const char *token );
    void toggleSign();

    // Constant initial values only: the page keeps a Calculator in RTC memory, which a
    // constructor running at each wake would reset.
    char m_text[MaxText] = {};
    int m_state = Typing;
    int m_pad = DigitPad;
    bool m_radians = false;
    bool m_colon = false;
    double m_value = 0;
};
