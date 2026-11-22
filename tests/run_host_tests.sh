#!/bin/sh
# Builds and runs the host tests of the astronomy code (no ESP-IDF needed, only g++).
#     sh tests/run_host_tests.sh
set -e
cd "$(dirname "$0")/.."
mkdir -p build-host
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/AstronomyTest.cpp main/Astronomy.cpp main/Hijri.cpp -o build-host/astronomy_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/HijriTest.cpp main/Hijri.cpp main/Astronomy.cpp -o build-host/hijri_test
build-host/holy_days_test
build-host/regatta_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/HolyDaysTest.cpp main/HolyDays.cpp main/Dates.cpp main/Hijri.cpp main/Astronomy.cpp -o build-host/holy_days_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/RegattaTest.cpp main/Regatta.cpp -o build-host/regatta_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/CornersTest.cpp main/Corners.cpp main/Sky.cpp main/MilkyWay.cpp main/Dates.cpp main/Hijri.cpp main/Astronomy.cpp -o build-host/corners_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/ZonesTest.cpp main/Zones.cpp -o build-host/zones_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/SkyTest.cpp main/Sky.cpp main/MilkyWay.cpp main/Dates.cpp main/Hijri.cpp main/Astronomy.cpp -o build-host/sky_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/PrayerTest.cpp main/Sky.cpp main/MilkyWay.cpp main/Dates.cpp main/Hijri.cpp main/Astronomy.cpp -o build-host/prayer_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/PartOrderTest.cpp main/PartOrder.cpp -o build-host/part_order_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/TallyTest.cpp main/Tally.cpp main/Morse.cpp -o build-host/tally_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/CalculatorTest.cpp main/Calculator.cpp main/Units.cpp -o build-host/calculator_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/TiltTest.cpp main/Tilt.cpp -o build-host/tilt_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/BatteryLimitTest.cpp main/BatteryLimit.cpp -o build-host/battery_limit_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/NightRedTest.cpp main/NightRed.cpp -o build-host/night_red_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/FaceConfigTest.cpp main/FaceConfig.cpp -o build-host/face_config_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/FaceTapTest.cpp main/FaceTap.cpp -o build-host/face_tap_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/FaceTextTest.cpp main/FaceText.cpp main/FaceTable.cpp main/FaceConfig.cpp main/Language.cpp main/Zones.cpp main/Sky.cpp main/MilkyWay.cpp main/Dates.cpp main/Hijri.cpp main/Astronomy.cpp -o build-host/face_text_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/AodSmallTest.cpp main/FaceText.cpp main/FaceTable.cpp main/FaceConfig.cpp main/Language.cpp main/Zones.cpp main/Sky.cpp main/MilkyWay.cpp main/Dates.cpp main/Hijri.cpp main/Astronomy.cpp -o build-host/aod_small_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/AnalogClockTest.cpp main/AnalogClock.cpp -o build-host/analog_clock_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/MilkyWayTest.cpp main/MilkyWay.cpp main/Astronomy.cpp -o build-host/milky_way_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/SevenSegmentTest.cpp main/SevenSegment.cpp -o build-host/seven_segment_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/TableTextTest.cpp main/TableText.cpp -o build-host/table_text_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/AlarmsTest.cpp main/Alarms.cpp main/AlarmIcons.cpp main/Zones.cpp main/Sky.cpp main/MilkyWay.cpp main/Dates.cpp main/Hijri.cpp main/Astronomy.cpp -o build-host/alarms_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/AstrolabeTest.cpp main/Astrolabe.cpp main/Astronomy.cpp -o build-host/astrolabe_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/ObservingTest.cpp main/Observing.cpp main/Messier.cpp main/Optics.cpp main/Astronomy.cpp -o build-host/observing_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/NavigationTest.cpp main/Almanac.cpp main/Aiming.cpp main/Magnetic.cpp main/Planets.cpp main/Astronomy.cpp -o build-host/navigation_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/CycleLogTest.cpp main/CycleLog.cpp -o build-host/cycle_log_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/CycleTest.cpp main/Cycle.cpp main/CycleLog.cpp -o build-host/cycle_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/HayizTest.cpp main/Hayiz.cpp main/CycleLog.cpp -o build-host/hayiz_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/PlanetsTest.cpp main/Planets.cpp main/Astronomy.cpp -o build-host/planets_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tools/make_corner_table.cpp main/Corners.cpp main/Sky.cpp main/MilkyWay.cpp main/Dates.cpp main/Hijri.cpp main/Astronomy.cpp -o build-host/make_corner_table
build-host/cycle_log_test
build-host/cycle_test
build-host/hayiz_test
build-host/astronomy_test tests/astronomy_reference.csv
build-host/hijri_test
build-host/corners_test
build-host/zones_test
build-host/sky_test
build-host/prayer_test
build-host/part_order_test
build-host/tally_test
build-host/calculator_test
build-host/tilt_test
build-host/battery_limit_test
build-host/night_red_test
build-host/face_config_test tools/portal_preview.py
build-host/face_tap_test
build-host/face_text_test
build-host/seven_segment_test
build-host/aod_small_test main/AodGlyphs.c
build-host/analog_clock_test
build-host/milky_way_test
build-host/table_text_test
build-host/alarms_test
build-host/astrolabe_test tests/astrolabe_reference.csv
build-host/observing_test tests/observing_reference.csv
build-host/navigation_test tests/navigation_reference.csv tests/wmm2025_test_values.txt
build-host/planets_test tests/planet_reference.csv tests/planet_risings.csv tests/planet_arcs.csv
# main/CornerTable.c must be what the corner geometry gives today.
build-host/make_corner_table > build-host/CornerTable.c
if cmp -s build-host/CornerTable.c main/CornerTable.c; then
    echo "Corner table: up to date"
else
    echo "Corner table: stale, run build-host/make_corner_table > main/CornerTable.c"
    exit 1
fi
