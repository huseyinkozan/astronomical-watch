#pragma once

#include "FaceConfig.h"

// The face pages in NVS: one blob, read at the start of active mode and written when the setup
// portal saves them.
class FaceStore
{
public:
    // The saved pages, or the defaults when there are none, the blob has another size (an
    // earlier layout) or its values are out of range.
    static void load( FaceConfig::Pages &pages );
    static void save( const FaceConfig::Pages &pages );
};
