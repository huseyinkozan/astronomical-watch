#!/bin/sh
# Builds and runs the host tests of the astronomy code (no ESP-IDF needed, only g++).
#     sh tests/run_host_tests.sh
set -e
cd "$(dirname "$0")/.."
mkdir -p build-host
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/AstronomyTest.cpp main/Astronomy.cpp main/Hijri.cpp -o build-host/astronomy_test
g++ -std=c++17 -O2 -Wall -Wextra -I main tests/HijriTest.cpp main/Hijri.cpp main/Astronomy.cpp -o build-host/hijri_test
build-host/astronomy_test tests/astronomy_reference.csv
build-host/hijri_test
