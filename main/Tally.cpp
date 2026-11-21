#include "Tally.h"

static const int s_goals[Tally::TargetCount] = { 0, 33, 99, 100, 33 };

void Tally::add( int64_t nowSeconds )
{
    if ( count >= MaxCount ) {
        return;
    }
    if ( count == 0 ) {
        startMinute = nowSeconds > 0 ? nowSeconds / 60 : 0;
    }
    ++count;
}

void Tally::undo()
{
    if ( count > 0 ) {
        --count;
    }
    if ( count == 0 ) {
        startMinute = 0;
    }
}

void Tally::reset()
{
    count = 0;
    startMinute = 0;
}

void Tally::nextTarget()
{
    target = ( target + 1 ) % TargetCount;
}

int Tally::goal() const
{
    return target >= 0 && target < TargetCount ? s_goals[target] : 0;
}

int Tally::stepCount() const
{
    return target == Steps33 ? 3 : 1;
}

int Tally::step() const
{
    if ( stepCount() == 1 || count == 0 ) {
        return 0;
    }
    // The step's last count still shows in it ("33 / 33"); the next one starts the next step.
    const int step = ( count - 1 ) / goal();
    return step < stepCount() - 1 ? step : stepCount() - 1;
}

int Tally::inStep() const
{
    return count - step() * goal();
}

bool Tally::reached() const
{
    return goal() > 0 && inStep() >= goal();
}

int Tally::elapsedMinutes( int64_t nowSeconds ) const
{
    if ( count == 0 || startMinute == 0 || nowSeconds <= 0 ) {
        return -1;
    }
    const int64_t minutes = nowSeconds / 60 - startMinute;
    return minutes < 0 ? -1 : static_cast<int>( minutes );
}

int Tally::perHour( int64_t nowSeconds ) const
{
    if ( count == 0 || startMinute == 0 || nowSeconds <= 0 ) {
        return -1;
    }
    const int64_t seconds = nowSeconds - startMinute * 60;
    if ( seconds < 60 ) {
        return -1;
    }
    return static_cast<int>( ( static_cast<int64_t>( count ) * 3600 + seconds / 2 ) / seconds );
}
