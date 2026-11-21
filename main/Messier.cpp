#include "Messier.h"

// Made by tools/make_messier_table.py from OpenNGC (CC-BY-SA 4.0).
const Messier::Object Messier::Objects[Count] = {
    { 3345, 1321, 84 },  // M1 NGC1952
    { 12935, -49, 62 },  // M2 NGC7089
    { 8222, 1703, 64 },  // M3 NGC5272
    { 9836, -1592, 54 },  // M4 NGC6121
    { 9186, 125, 60 },  // M5 NGC5904
    { 10603, -1935, 42 },  // M6 NGC6405
    { 10739, -2088, 33 },  // M7 NGC6475
    { 10837, -1463, 58 },  // M8 NGC6523
    { 10392, -1111, 84 },  // M9 NGC6333
    { 10171, -246, 50 },  // M10 NGC6254
    { 11311, -376, 58 },  // M11 NGC6705
    { 10072, -117, 61 },  // M12 NGC6218
    { 10017, 2188, 58 },  // M13 NGC6205
    { 10576, -195, 57 },  // M14 NGC6402
    { 12900, 730, 63 },  // M15 NGC7078
    { 10988, -828, 60 },  // M16 NGC6611
    { 11008, -970, 70 },  // M17 NGC6618
    { 11000, -1026, 69 },  // M18 NGC6613
    { 10226, -1576, 56 },  // M19 NGC6273
    { 10827, -1378, 85 },  // M20 NGC6514
    { 10842, -1349, 59 },  // M21 NGC6531
    { 11164, -1434, 51 },  // M22 NGC6656
    { 10771, -1139, 55 },  // M23 NGC6494
    { 10969, -1111, 45 },  // M24 IC4715
    { 11118, -1147, 46 },  // M25 IC4725
    { 11253, -563, 89 },  // M26 NGC6694
    { 11996, 1363, 74 },  // M27 NGC6853
    { 11045, -1492, 69 },  // M28 NGC6626
    { 12240, 2310, 66 },  // M29 NGC6913
    { 13004, -1391, 71 },  // M30 NGC7099
    { 427, 2476, 34 },  // M31 NGC0224
    { 427, 2452, 81 },  // M32 NGC0221
    { 938, 1840, 58 },  // M33 NGC0598
    { 1621, 2565, 52 },  // M34 NGC1039
    { 3691, 1460, 53 },  // M35 NGC2168
    { 3363, 2048, 60 },  // M36 NGC1960
    { 3523, 1953, 56 },  // M37 NGC2099
    { 3287, 2151, 64 },  // M38 NGC1912
    { 12918, 2906, 46 },  // M39 NGC7092
    { 7423, 3485, 80 },  // M40 M040
    { 4060, -1245, 45 },  // M41 NGC2287
    { 3353, -323, 40 },  // M42 NGC1976
    { 3355, -316, 90 },  // M43 NGC1982
    { 5204, 1180, 31 },  // M44 NGC2632
    { 2275, 1446, 12 },  // M45 Mel022
    { 4618, -889, 61 },  // M46 NGC2437
    { 4566, -869, 44 },  // M47 NGC2422
    { 4937, -345, 58 },  // M48 NGC2548
    { 7498, 480, 83 },  // M49 NGC4472
    { 4227, -502, 59 },  // M50 NGC2323
    { 8099, 2832, 84 },  // M51 NGC5194
    { 14048, 3696, 69 },  // M52 NGC7654
    { 7929, 1090, 78 },  // M53 NGC5024
    { 11351, -1829, 77 },  // M54 NGC6715
    { 11800, -1858, 65 },  // M55 NGC6809
    { 11566, 1811, 84 },  // M56 NGC6779
    { 11336, 1982, 88 },  // M57 NGC6720
    { 7577, 709, 103 },  // M58 NGC4579
    { 7620, 699, 96 },  // M59 NGC4621
    { 7637, 693, 88 },  // M60 NGC4649
    { 7419, 268, 102 },  // M61 NGC4303
    { 10212, -1807, 66 },  // M62 NGC6266
    { 7958, 2522, 86 },  // M63 NGC5055
    { 7767, 1301, 85 },  // M64 NGC4826
    { 6789, 786, 93 },  // M65 NGC3623
    { 6802, 779, 89 },  // M66 NGC3627
    { 5313, 709, 69 },  // M67 NGC2682
    { 7595, -1605, 80 },  // M68 NGC4590
    { 11114, -1941, 76 },  // M69 NGC6637
    { 11232, -1938, 79 },  // M70 NGC6681
    { 11938, 1127, 82 },  // M71 NGC6838
    { 12535, -752, 90 },  // M72 NGC6981
    { 12589, -758, 89 },  // M73 NGC6994
    { 967, 947, 93 },  // M74 NGC0628
    { 12061, -1315, 83 },  // M75 NGC6864
    { 1023, 3095, 101 },  // M76 NGC0650
    { 1627, -1, 93 },  // M77 NGC1068
    { 3468, 5, 80 },  // M78 NGC2068
    { 3242, -1471, 82 },  // M79 NGC1904
    { 9770, -1379, 73 },  // M80 NGC6093
    { 5956, 4144, 69 },  // M81 NGC3031
    { 5959, 4181, 83 },  // M82 NGC3034
    { 8170, -1792, 72 },  // M83 NGC5236
    { 7451, 773, 98 },  // M84 NGC4374
    { 7454, 1091, 90 },  // M85 NGC4382
    { 7462, 777, 89 },  // M86 NGC4406
    { 7508, 743, 90 },  // M87 NGC4486
    { 7520, 865, 103 },  // M88 NGC4501
    { 7557, 753, 101 },  // M89 NGC4552
    { 7568, 790, 95 },  // M90 NGC4569
    { 7554, 870, 110 },  // M91 NGC4548
    { 10371, 2588, 65 },  // M92 NGC6341
    { 4645, -1431, 62 },  // M93 NGC2447
    { 7709, 2467, 82 },  // M94 NGC4736
    { 6440, 702, 98 },  // M95 NGC3351
    { 6468, 709, 92 },  // M96 NGC3368
    { 6748, 3301, 99 },  // M97 NGC3587
    { 7338, 894, 108 },  // M98 NGC4192
    { 7388, 865, 98 },  // M99 NGC4254
    { 7429, 949, 95 },  // M100 NGC4321
    { 8432, 3261, 79 },  // M101 NGC5457
    { 9065, 3346, 99 },  // M102 NGC5866
    { 934, 3639, 74 },  // M103 NGC0581
    { 7600, -697, 86 },  // M104 NGC4594
    { 6478, 755, 93 },  // M105 NGC3379
    { 7390, 2838, 93 },  // M106 NGC4258
    { 9925, -783, 79 },  // M107 NGC6171
    { 6715, 3340, 100 },  // M108 NGC3556
    { 7176, 3202, 99 },  // M109 NGC3992
    { 404, 2501, 82 },  // M110 NGC0205
};

double Messier::raHours( int index )
{
    return Objects[index].rightAscension / 600.0;
}

double Messier::decDegrees( int index )
{
    return Objects[index].declination / 60.0;
}

double Messier::magnitude( int index )
{
    return Objects[index].magnitude / 10.0;
}

int Messier::visible( int64_t time, const Astronomy::Observer &observer, const Observing::Night &night,
                      double minimumAltitude, Entry *entries )
{
    int count = 0;
    for ( int i = 0; i < Count; ++i ) {
        double ra = 0;
        double dec = 0;
        Astronomy::precess( raHours( i ), decDegrees( i ), time, ra, dec );
        const double altitude = Observing::altitude( ra, dec, time, observer );
        if ( altitude <= minimumAltitude ) {
            continue;
        }
        Entry entry = {};
        entry.index = static_cast<uint8_t>( i );
        entry.altitude = static_cast<int8_t>( altitude + 0.5 );
        if ( night.dark ) {
            entry.bestMinute = static_cast<int32_t>( Observing::highestTime( ra, night.dusk, night.dawn, observer.longitude ) / 60 );
        }
        // Insertion: the list stays sorted by altitude, highest first.
        int at = count;
        while ( at > 0 && entries[at - 1].altitude < entry.altitude ) {
            entries[at] = entries[at - 1];
            --at;
        }
        entries[at] = entry;
        ++count;
    }
    return count;
}
