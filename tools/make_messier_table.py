#!/usr/bin/env python3
"""The Messier table of main/Messier.cpp, from OpenNGC (github.com/mattiaverga/OpenNGC,
CC-BY-SA 4.0: database_files/NGC.csv and addendum.csv): J2000 positions and visual magnitudes.

    curl -LO https://raw.githubusercontent.com/mattiaverga/OpenNGC/master/database_files/NGC.csv
    curl -LO https://raw.githubusercontent.com/mattiaverga/OpenNGC/master/database_files/addendum.csv
    python3 tools/make_messier_table.py <folder with both files>

Prints the table's rows: { right ascension in tenths of a minute of time, declination in
arcminutes, visual magnitude in tenths }. OpenNGC leaves M102 out (its identity is disputed; the
usual choice is NGC 5866, taken here) and has no V magnitude for a few objects; those come from
SEDS's Messier pages (MAGNITUDES below).
"""

import csv
import os
import sys

# Visual magnitudes missing from OpenNGC (SEDS, messier.seds.org).
MAGNITUDES = {22: 5.1, 35: 5.3, 62: 6.6, 69: 7.6, 70: 7.9, 71: 8.2, 107: 7.9}


def degrees(text):
    sign = -1 if text.startswith('-') else 1
    parts = [float(p) for p in text.lstrip('+-').split(':')]
    return sign * (parts[0] + parts[1] / 60 + parts[2] / 3600)


folder = sys.argv[1]
rows = {}
for name in ('NGC.csv', 'addendum.csv'):
    for row in csv.DictReader(open(os.path.join(folder, name)), delimiter=';'):
        if row['Type'] == 'Dup':
            continue
        if row['M']:
            rows[int(row['M'])] = row
        elif row['Name'] == 'NGC5866':
            rows[102] = row
assert sorted(rows) == list(range(1, 111)), sorted(set(range(1, 111)) - set(rows))

for number in range(1, 111):
    row = rows[number]
    ra = round(degrees(row['RA']) * 600)  # hours to tenths of a minute
    dec = round(degrees(row['Dec']) * 60)
    magnitude = MAGNITUDES.get(number) or float(row['V-Mag'] or row['B-Mag'])
    print(f'    {{ {ra}, {dec}, {round(magnitude * 10)} }},  // M{number} {row["Name"]}')
