#include "CycleLog.h"

#include <cstring>

static int indexOf( int day )
{
    const int index = day % CycleLog::Days;
    return index < 0 ? index + CycleLog::Days : index;
}

void CycleLog::clear()
{
    memset( this, 0, sizeof( *this ) );
}

void CycleLog::advance( int today )
{
    if ( newestDay == 0 ) {
        newestDay = today;
        return;
    }
    if ( today <= newestDay ) {
        return;
    }
    const int passed = today - newestDay;
    for ( int i = 1; i <= passed && i <= Days; ++i ) {
        days[indexOf( newestDay + i )] = 0;
    }
    newestDay = today;
}

bool CycleLog::covers( int day ) const
{
    return newestDay != 0 && day <= newestDay && day > newestDay - Days;
}

uint8_t CycleLog::get( int day ) const
{
    return covers( day ) ? days[indexOf( day )] : 0;
}

int CycleLog::flow( int day ) const
{
    return get( day ) & FlowMask;
}

void CycleLog::set( int day, uint8_t value )
{
    if ( !covers( day ) ) {
        return;
    }
    uint8_t &stored = days[indexOf( day )];
    const bool was = ( stored & MadeUpFast ) != 0;
    const bool now = ( value & MadeUpFast ) != 0;
    madeUp += ( now ? 1 : 0 ) - ( was ? 1 : 0 );
    stored = value;
}
