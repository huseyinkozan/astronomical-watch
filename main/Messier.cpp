#include "Messier.h"

// Made by tools/make_messier_table.py from NASA HEASARC's MESSIER and NGC2000 tables (public domain).
const Messier::Object Messier::Objects[Count] = {
    { 3345, 1321, 84 },  // M1 NGC1952
    { 12935, -49, 65 },  // M2 NGC7089
    { 8222, 1703, 64 },  // M3 NGC5272
    { 9836, -1592, 59 },  // M4 NGC6121
    { 9186, 125, 58 },  // M5 NGC5904
    { 10601, -1933, 42 },  // M6 NGC6405
    { 10739, -2089, 33 },  // M7 NGC6475
    { 10838, -1463, 58 },  // M8 NGC6523
    { 10392, -1111, 79 },  // M9 NGC6333
    { 10171, -246, 66 },  // M10 NGC6254
    { 11311, -376, 58 },  // M11 NGC6705
    { 10072, -117, 66 },  // M12 NGC6218
    { 10017, 2188, 59 },  // M13 NGC6205
    { 10576, -195, 76 },  // M14 NGC6402
    { 12900, 730, 64 },  // M15 NGC7078
    { 10988, -827, 60 },  // M16 NGC6611
    { 11008, -971, 70 },  // M17 NGC6618
    { 10999, -1028, 69 },  // M18 NGC6613
    { 10226, -1576, 72 },  // M19 NGC6273
    { 10826, -1382, 85 },  // M20 NGC6514
    { 10846, -1350, 59 },  // M21 NGC6531
    { 11164, -1434, 51 },  // M22 NGC6656
    { 10768, -1141, 55 },  // M23 NGC6494
    { 10969, -1109, 45 },  // M24 (no NGC number)
    { 11116, -1155, 46 },  // M25 IC4725
    { 11252, -564, 80 },  // M26 NGC6694
    { 11996, 1363, 81 },  // M27 NGC6853
    { 11045, -1492, 69 },  // M28 NGC6626
    { 12239, 2312, 66 },  // M29 NGC6913
    { 13004, -1391, 75 },  // M30 NGC7099
    { 427, 2476, 34 },  // M31 NGC224
    { 427, 2452, 82 },  // M32 NGC221
    { 939, 1839, 57 },  // M33 NGC598
    { 1620, 2567, 52 },  // M34 NGC1039
    { 3689, 1460, 51 },  // M35 NGC2168
    { 3361, 2048, 60 },  // M36 NGC1960
    { 3524, 1953, 56 },  // M37 NGC2099
    { 3287, 2150, 64 },  // M38 NGC1912
    { 12922, 2906, 46 },  // M39 NGC7092
    { 7424, 3485, 80 },  // M40 (no NGC number)
    { 4070, -1244, 45 },  // M41 NGC2287
    { 3354, -327, 40 },  // M42 NGC1976
    { 3356, -316, 90 },  // M43 NGC1982
    { 5201, 1199, 31 },  // M44 NGC2632
    { 2270, 1447, 12 },  // M45 (no NGC number)
    { 4618, -889, 61 },  // M46 NGC2437
    { 4566, -870, 44 },  // M47 NGC2422
    { 4938, -348, 58 },  // M48 NGC2548
    { 7498, 480, 84 },  // M49 NGC4472
    { 4232, -500, 59 },  // M50 NGC2323
    { 8099, 2832, 81 },  // M51 NGC5194-5
    { 14042, 3695, 69 },  // M52 NGC7654
    { 7929, 1090, 77 },  // M53 NGC5024
    { 11351, -1829, 77 },  // M54 NGC6715
    { 11800, -1858, 70 },  // M55 NGC6809
    { 11566, 1811, 82 },  // M56 NGC6779
    { 11336, 1982, 90 },  // M57 NGC6720
    { 7577, 709, 98 },  // M58 NGC4579
    { 7620, 699, 98 },  // M59 NGC4621
    { 7637, 693, 88 },  // M60 NGC4649
    { 7419, 268, 97 },  // M61 NGC4303
    { 10212, -1807, 66 },  // M62 NGC6266
    { 7958, 2522, 86 },  // M63 NGC5055
    { 7767, 1301, 85 },  // M64 NGC4826
    { 6789, 785, 93 },  // M65 NGC3623
    { 6802, 779, 90 },  // M66 NGC3627
    { 5304, 709, 69 },  // M67 NGC2682
    { 7595, -1605, 82 },  // M68 NGC4590
    { 11114, -1941, 77 },  // M69 NGC6637
    { 11232, -1938, 81 },  // M70 NGC6681
    { 11938, 1127, 83 },  // M71 NGC6838
    { 12535, -752, 94 },  // M72 NGC6981
    { 12589, -758, 90 },  // M73 NGC6994
    { 967, 947, 92 },  // M74 NGC628
    { 12061, -1315, 86 },  // M75 NGC6864
    { 1024, 3094, 115 },  // M76 NGC650-1
    { 1627, -1, 88 },  // M77 NGC1068
    { 3467, 3, 80 },  // M78 NGC2068
    { 3245, -1473, 80 },  // M79 NGC1904
    { 9770, -1379, 72 },  // M80 NGC6093
    { 5956, 4144, 68 },  // M81 NGC3031
    { 5958, 4181, 84 },  // M82 NGC3034
    { 8170, -1792, 76 },  // M83 NGC5236
    { 7451, 773, 93 },  // M84 NGC4374
    { 7454, 1091, 92 },  // M85 NGC4382
    { 7462, 777, 92 },  // M86 NGC4406
    { 7508, 744, 86 },  // M87 NGC4486
    { 7520, 865, 95 },  // M88 NGC4501
    { 7557, 753, 98 },  // M89 NGC4552
    { 7568, 790, 95 },  // M90 NGC4569
    { 7554, 870, 102 },  // M91 NGC4548
    { 10371, 2588, 65 },  // M92 NGC6341
    { 4646, -1432, 62 },  // M93 NGC2447
    { 7709, 2467, 81 },  // M94 NGC4736
    { 6440, 702, 97 },  // M95 NGC3351
    { 6468, 709, 92 },  // M96 NGC3368
    { 6748, 3301, 112 },  // M97 NGC3587
    { 7338, 894, 101 },  // M98 NGC4192
    { 7388, 865, 98 },  // M99 NGC4254
    { 7429, 949, 94 },  // M100 NGC4321
    { 8432, 3261, 77 },  // M101 NGC5457
    { 9065, 3346, 100 },  // M102 NGC5866
    { 932, 3642, 74 },  // M103 NGC581
    { 7600, -697, 83 },  // M104 NGC4594
    { 6478, 755, 93 },  // M105 NGC3379
    { 7390, 2838, 83 },  // M106 NGC4258
    { 9925, -783, 81 },  // M107 NGC6171
    { 6715, 3340, 100 },  // M108 NGC3556
    { 7176, 3203, 98 },  // M109 NGC3992
    { 404, 2501, 80 },  // M110 NGC205
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
