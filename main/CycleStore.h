#pragma once

#include "CycleLog.h"
#include "esp_err.h"

// The cycle page's log in NVS (namespace "cycle", one blob), written when a day's entry or a
// setting of the page is saved. The factory reset and the portal's "delete cycle data" erase
// the namespace.
class CycleStore
{
public:
    // An empty log when nothing (or a blob of another size) is saved.
    static void load( CycleLog &log );
    static esp_err_t save( const CycleLog &log );
    static esp_err_t erase();
};
