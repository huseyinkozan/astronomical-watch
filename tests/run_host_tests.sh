#!/bin/sh
# Builds and runs the host tests of the astronomy code (no ESP-IDF needed, only g++).
#     sh tests/run_host_tests.sh
set -e
cd "$(dirname "$0")/.."
mkdir -p build-host
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/AstronomyTest.cpp main/Astronomy.cpp main/Hijri.cpp -o build-host/astronomy_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/HijriTest.cpp main/Hijri.cpp main/Astronomy.cpp -o build-host/hijri_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/CornersTest.cpp main/Corners.cpp main/Sky.cpp main/Dates.cpp main/Hijri.cpp main/Astronomy.cpp -o build-host/corners_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/ZonesTest.cpp main/Zones.cpp -o build-host/zones_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/SkyTest.cpp main/Sky.cpp main/Dates.cpp main/Hijri.cpp main/Astronomy.cpp -o build-host/sky_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/PrayerTest.cpp main/Sky.cpp main/Dates.cpp main/Hijri.cpp main/Astronomy.cpp -o build-host/prayer_test
build-host/prayer_test
build-host/astronomy_test tests/astronomy_reference.csv
build-host/hijri_test
build-host/corners_test
build-host/zones_test
build-host/sky_test
build-host/prayer_test
