#include "Corners.h"

#include <cstdint>

#include <cmath>

#include "Dates.h"
#include "FaceConfig.h"

static const int64_t s_dayS = 86400;

void Corners::geometry( Corner corner, int width, int height, int &centreX, int &centreY, int &lowerAngle,
                        int &upperAngle )
{
    const bool left = corner == TopLeft || corner == BottomLeft;
    const bool top = corner == TopLeft || corner == TopRight;
    centreX = left ? CornerRadius : width - 1 - CornerRadius;
    centreY = top ? CornerRadius : height - 1 - CornerRadius;
    // The lower end lies on the side edge for the top corners and on the bottom edge for the
    // bottom ones.
    static const int lower[CornerCount] = { 180, 360, 90, 90 };
    static const int upper[CornerCount] = { 270, 270, 180, 0 };
    lowerAngle = lower[corner];
    upperAngle = upper[corner];
}

static void addEvent( Corners::Events &events, const Astronomy::Event &event, bool rising, int64_t shift = 0 )
{
    if ( !event.occurs || events.count >= Corners::MaxEvents ) {
        return;
    }
    // Insertion in time order.
    const int64_t time = event.time + shift;
    int i = events.count;
    while ( i > 0 && events.times[i - 1] > time ) {
        events.times[i] = events.times[i - 1];
        events.rising[i] = events.rising[i - 1];
        --i;
    }
    events.times[i] = time;
    events.rising[i] = rising;
    ++events.count;
}

static void addCrossings( Corners::Events &events, const Astronomy::Crossings &crossings )
{
    addEvent( events, crossings.rising, true );
    addEvent( events, crossings.setting, false );
}

Corners::Input Corners::fromSky( const Sky::Day &day )
{
    Input input = {};
    input.valid = true;
    const Astronomy::Crossings &sun = day.sun.riseSet;
    addEvent( input.sun, day.sunsetBefore, false );
    addCrossings( input.sun, sun );
    // Tomorrow's sunrise as today's a day later: within a few minutes outside the polar regions,
    // under 2 pixels on the gauge.
    addEvent( input.sun, sun.rising, true, s_dayS );
    input.sun.alwaysUp = sun.alwaysAbove;
    input.sun.alwaysDown = sun.alwaysBelow;
    addCrossings( input.moon, day.moonBefore );
    addCrossings( input.moon, day.moon );
    addCrossings( input.moon, day.moonAfter );
    input.moonPercent = static_cast<int>( day.phase.illumination * 100 + 0.5 );
    input.waxing = day.phase.elongation < 180;
    for ( int i = 0; i < Sky::PrayerCount && i < 6; ++i ) {
        input.prayers[i] = day.prayers.times[i];
        input.prayerValid[i] = day.prayers.valid[i];
    }
    return input;
}

// Whether the body is up at `now` and how much of the time between the events around it has
// passed, in thousandths; false when `now` is not between two events.
static bool progress( const Corners::Events &events, int64_t now, bool &up, int &passed )
{
    for ( int i = 0; i + 1 < events.count; ++i ) {
        if ( events.times[i] <= now && now < events.times[i + 1] ) {
            up = events.rising[i];
            passed = static_cast<int>( ( now - events.times[i] ) * 1000 / ( events.times[i + 1] - events.times[i] ) );
            return true;
        }
    }
    return false;
}

// The share of the time between the latest of `times` (each also a day earlier and later, for a
// daily event) not after `now` and the earliest after it, in thousandths; false without both.
static bool between( const int64_t *times, const bool *use, int count, bool daily, int64_t now, int &passed )
{
    int64_t previous = INT64_MIN;
    int64_t next = INT64_MAX;
    for ( int i = 0; i < count; ++i ) {
        if ( !use[i] ) {
            continue;
        }
        for ( int shift = daily ? -1 : 0; shift <= ( daily ? 1 : 0 ); ++shift ) {
            const int64_t t = times[i] + shift * s_dayS;
            if ( t <= now && t > previous ) {
                previous = t;
            }
            if ( t > now && t < next ) {
                next = t;
            }
        }
    }
    if ( previous == INT64_MIN || next == INT64_MAX ) {
        return false;
    }
    passed = static_cast<int>( ( now - previous ) * 1000 / ( next - previous ) );
    return true;
}

// The next rise (`rising`) or set of `events`.
static bool toEvent( const Corners::Events &events, bool rising, bool daily, int64_t now, int &passed )
{
    bool use[Corners::MaxEvents] = {};
    for ( int i = 0; i < events.count; ++i ) {
        use[i] = events.rising[i] == rising;
    }
    return between( events.times, use, events.count, daily, now, passed );
}

static int share( double part, double whole )
{
    const double value = part * 1000 / whole;
    return static_cast<int>( value < 0 ? 0 : ( value > 1000 ? 1000 : value ) );
}

// A day counter (FaceConfig::GaugeItem DayCounterGauge) at local day `t` (with its fraction).
static bool counterShare( const Corners::Context &context, int parameter, double t, int &passed )
{
    const int index = parameter & 3;
    const int fill = ( parameter >> 2 ) & 3;
    if ( !context.counterValid[index] ) {
        return false;
    }
    const int date = context.counterDays[index];
    if ( fill == FaceConfig::SinceSaved ) {
        const int saved = context.counterSavedDays[index];
        if ( saved <= 0 || date <= saved ) {
            return false;
        }
        passed = share( t - saved, date - saved );
        return true;
    }
    if ( fill == FaceConfig::Yearly ) {
        // The date's month and day in the years around now (29 February gives 1 March).
        int dateYear = 0;
        int month = 0;
        int dayOfMonth = 0;
        Dates::civil( date, dateYear, month, dayOfMonth );
        int thisYear = 0;
        int thisMonth = 0;
        int thisDay = 0;
        Dates::civil( static_cast<int>( std::floor( t ) ), thisYear, thisMonth, thisDay );
        double previous = -1e9;
        double next = 1e9;
        for ( int y = thisYear - 1; y <= thisYear + 1; ++y ) {
            const double anniversary = Dates::dayNumber( y, month, dayOfMonth );
            if ( anniversary <= t && anniversary > previous ) {
                previous = anniversary;
            }
            if ( anniversary > t && anniversary < next ) {
                next = anniversary;
            }
        }
        passed = share( t - previous, next - previous );
        return true;
    }
    const int span = fill == FaceConfig::Last30Days ? 30 : 100;
    passed = share( t - ( date - span ), span );
    return true;
}

static Corners::Gauge gaugeOf( int item, int parameter, const Corners::Input &input, const Corners::Context &context )
{
    Corners::Gauge gauge = { 0, 0, Corners::Empty, false };
    const int64_t now = context.now;
    const int64_t local = now + static_cast<int64_t>( context.utcOffsetMinutes ) * 60;
    const int day = static_cast<int>( ( local >= 0 ? local : local - s_dayS + 1 ) / s_dayS );
    const int seconds = static_cast<int>( local - static_cast<int64_t>( day ) * s_dayS );
    const double t = day + seconds / static_cast<double>( s_dayS );
    int year = 0;
    int month = 0;
    int date = 0;
    Dates::civil( day, year, month, date );
    bool up = false;
    int passed = 0;
    switch ( item ) {
    case FaceConfig::BatteryGauge:
        if ( context.batteryPercent >= 0 ) {
            const int percent = context.batteryPercent > 100 ? 100 : context.batteryPercent;
            const Corners::Kind kind = percent > 50 ? Corners::BatteryHigh
                                                    : ( percent > 10 ? Corners::BatteryMiddle : Corners::BatteryLow );
            gauge = { 0, percent * 10, kind, context.charging };
        }
        return gauge;
    case FaceConfig::WeekGauge:
        return { 0, share( Dates::weekday( day ) * static_cast<double>( s_dayS ) + seconds, 7.0 * s_dayS ), Corners::Calendar, false };
    case FaceConfig::MonthGauge: {
        const int length = Dates::dayNumber( month == 12 ? year + 1 : year, month == 12 ? 1 : month + 1, 1 ) -
                           Dates::dayNumber( year, month, 1 );
        return { 0, share( ( date - 1 ) * static_cast<double>( s_dayS ) + seconds, length * static_cast<double>( s_dayS ) ),
                 Corners::Calendar, false };
    }
    case FaceConfig::YearGauge: {
        const int first = Dates::dayNumber( year, 1, 1 );
        const int length = Dates::dayNumber( year + 1, 1, 1 ) - first;
        return { 0, share( t - first, length ), Corners::Calendar, false };
    }
    case FaceConfig::HourGauge:
        return { 0, parameter == 1 ? share( seconds % 43200, 43200 ) : share( seconds, s_dayS ), Corners::Clock, false };
    case FaceConfig::MinuteGauge:
        return { 0, share( seconds % 3600, 3600 ), Corners::Clock, false };
    case FaceConfig::SecondGauge:
        if ( !context.alwaysOn ) {
            gauge = { 0, share( seconds % 60, 60 ), Corners::Clock, false };
        }
        return gauge;
    case FaceConfig::DayCounterGauge:
        if ( counterShare( context, parameter, t, passed ) ) {
            gauge = { 0, passed, Corners::Counter, false };
        }
        return gauge;
    default:
        break;
    }
    if ( !input.valid ) {
        return gauge;
    }
    switch ( item ) {
    case FaceConfig::DayNightGauge:
        if ( progress( input.sun, now, up, passed ) ) {
            gauge = { 0, passed, up ? Corners::Day : Corners::Night, false };
        } else if ( input.sun.alwaysUp ) {
            gauge = { 0, 1000, Corners::Day, false };
        } else if ( input.sun.alwaysDown ) {
            gauge = { 0, 1000, Corners::Night, false };
        }
        break;
    case FaceConfig::MoonPassGauge:
        if ( progress( input.moon, now, up, passed ) ) {
            gauge = { 0, passed, up ? Corners::MoonUp : Corners::MoonDown, false };
        }
        break;
    case FaceConfig::MoonPhaseGauge: {
        const int lit = input.moonPercent * 10;
        gauge = input.waxing ? Corners::Gauge { 0, lit, Corners::MoonLit, false }
                             : Corners::Gauge { 1000 - lit, 1000, Corners::MoonLit, false };
        break;
    }
    case FaceConfig::NextPrayerGauge:
        // Today's times a day earlier and later stand for yesterday's and tomorrow's (within a
        // couple of minutes, under a pixel of the gauge).
        if ( between( input.prayers, input.prayerValid, 6, true, now, passed ) ) {
            gauge = { 0, passed, Corners::PrayerEvent, false };
        }
        break;
    case FaceConfig::NextSunriseGauge:
    case FaceConfig::NextSunsetGauge:
        if ( toEvent( input.sun, item == FaceConfig::NextSunriseGauge, true, now, passed ) ) {
            gauge = { 0, passed, Corners::SunEvent, false };
        }
        break;
    case FaceConfig::NextMoonriseGauge:
    case FaceConfig::NextMoonsetGauge:
        // The Moon's own events over three days: its day is longer than ours.
        if ( toEvent( input.moon, item == FaceConfig::NextMoonriseGauge, false, now, passed ) ) {
            gauge = { 0, passed, Corners::MoonEvent, false };
        }
        break;
    case FaceConfig::HijriMonthGauge:
        // The month's length is not known here: 30 days (a 29-day month ends a day short).
        if ( context.hijriDay > 0 ) {
            gauge = { 0, share( ( context.hijriDay - 1 ) * static_cast<double>( s_dayS ) + seconds, 30.0 * s_dayS ),
                      Corners::HijriCalendar, false };
        }
        break;
    case FaceConfig::SunAltitudeGauge:
        if ( context.sunAltitudeValid && context.sunAltitude > 0 ) {
            gauge = { 0, share( context.sunAltitude, 90 ), Corners::Altitude, false };
        }
        break;
    default:
        break;
    }
    return gauge;
}

void Corners::compute( const Input &input, const Context &context, const uint8_t items[CornerCount],
                       const uint8_t parameters[CornerCount], Gauge gauges[CornerCount] )
{
    for ( int i = 0; i < CornerCount; ++i ) {
        gauges[i] = gaugeOf( items[i], parameters[i], input, context );
    }
}

void Corners::compute( const Input &input, int64_t now, int batteryPercent, bool charging,
                       Gauge gauges[CornerCount] )
{
    Context context = {};
    context.now = now;
    context.batteryPercent = batteryPercent;
    context.charging = charging;
    const uint8_t items[CornerCount] = { FaceConfig::DayNightGauge, FaceConfig::BatteryGauge, FaceConfig::MoonPassGauge,
                                         FaceConfig::MoonPhaseGauge };
    const uint8_t parameters[CornerCount] = {};
    compute( input, context, items, parameters, gauges );
}

Corners::ArcPlace Corners::arcPlace( const Events &events, int64_t now, bool orbit, bool south )
{
    ArcPlace place = {};
    place.alwaysUp = events.alwaysUp;
    Pass p = {};
    place.known = Corners::pass( events, now, p ) && p.upEnd > p.upStart && p.downEnd > p.upEnd;
    if ( !place.known ) {
        return place;
    }
    place.up = p.up;
    const float upPart = static_cast<float>( now - p.upStart ) / static_cast<float>( p.upEnd - p.upStart );
    if ( orbit ) {
        const float span = 360.0f * static_cast<float>( p.upEnd - p.upStart ) / static_cast<float>( p.downEnd - p.upStart );
        place.rise = 270 - span / 2;
        place.set = 270 + span / 2;
        place.angle = place.up ? place.rise + span * upPart
                               : place.set + ( 360 - span ) * static_cast<float>( now - p.upEnd ) /
                                                 static_cast<float>( p.downEnd - p.upEnd );
        place.angle = std::fmod( place.angle, 360.0f );
    } else {
        place.angle = south ? 360 - 180 * upPart : 180 + 180 * upPart;
    }
    return place;
}

bool Corners::pass( const Events &events, int64_t now, Pass &result )
{
    for ( int i = 0; i + 1 < events.count; ++i ) {
        if ( !( events.times[i] <= now && now < events.times[i + 1] ) ) {
            continue;
        }
        result.up = events.rising[i];
        if ( result.up ) {
            // Rise, set, then the next rise.
            if ( i + 2 >= events.count ) {
                return false;
            }
            result.upStart = events.times[i];
            result.upEnd = events.times[i + 1];
            result.downEnd = events.times[i + 2];
        } else {
            // The rise before, this set, the next rise.
            if ( i < 1 ) {
                return false;
            }
            result.upStart = events.times[i - 1];
            result.upEnd = events.times[i];
            result.downEnd = events.times[i + 1];
        }
        return result.upStart < result.upEnd && result.upEnd < result.downEnd;
    }
    return false;
}
