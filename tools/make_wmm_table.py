#!/usr/bin/env python3
"""The World Magnetic Model's coefficients in main/Magnetic.cpp, from NOAA NCEI's WMM2025 file
(WMM.COF, US government work, public domain; www.ncei.noaa.gov/products/world-magnetic-model).

    curl -LO https://www.ncei.noaa.gov/sites/default/files/2025-01/WMM2025COF.zip
    unzip WMM2025COF.zip
    python3 tools/make_wmm_table.py WMM2025COF/WMM.COF

Prints the rows { g, h, g per year, h per year } (nT) for n = 1 to 12, m = 0 to n in that order.
The same archive's WMM2025_TEST_VALUES.txt is tests/wmm2025_test_values.txt.
"""

import sys

lines = open(sys.argv[1]).read().split('\n')
assert lines[0].split()[0] == '2025.0', lines[0]
count = 0
for line in lines[1:]:
    fields = line.split()
    if len(fields) != 6:
        break
    n, m = int(fields[0]), int(fields[1])
    g, h, gDot, hDot = fields[2:]
    print(f'    {{ {g}f, {h}f, {gDot}f, {hDot}f }},  // {n} {m}')
    count += 1
assert count == 90, count
