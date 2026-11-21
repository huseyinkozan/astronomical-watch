#!/usr/bin/env python3
"""The Messier table of main/Messier.cpp, from NASA HEASARC's MESSIER and NGC2000 tables (US
government work, public domain; heasarc.gsfc.nasa.gov/W3Browse/general-catalog/messier.html):
J2000 positions and visual magnitudes.

    curl -LO https://heasarc.gsfc.nasa.gov/FTP/heasarc/dbase/tdat_files/heasarc_messier.tdat.gz
    curl -LO https://heasarc.gsfc.nasa.gov/FTP/heasarc/dbase/tdat_files/heasarc_ngc2000.tdat.gz
    python3 tools/make_messier_table.py <folder with both files>

Prints the table's rows: { right ascension in tenths of a minute of time, declination in
arcminutes, visual magnitude in tenths }. The MESSIER table leaves M102 out (its identity is
disputed; the usual choice is NGC 5866, taken here from the NGC2000 table) and has no magnitude
for M73, an asterism of four stars (MAGNITUDES below, the commonly quoted value).
"""

import gzip
import os
import sys

MAGNITUDES = {73: 9.0}


def rows(path):
    """The rows of a HEASARC .tdat file as dictionaries by field name."""
    fields = []
    result = []
    inData = False
    for line in gzip.open(path, 'rt', encoding='latin-1'):
        line = line.rstrip('\n')
        if line.startswith('line[1] = '):
            fields = line.split(' = ', 1)[1].split()
        elif line == '<DATA>':
            inData = True
        elif line == '<END>':
            break
        elif inData:
            result.append(dict(zip(fields, line.split('|'))))
    return result


folder = sys.argv[1]
objects = {}
for row in rows(os.path.join(folder, 'heasarc_messier.tdat.gz')):
    objects[int(row['name'].split()[1])] = (row['alt_name'], float(row['ra']), float(row['dec']), row['vmag'])
for row in rows(os.path.join(folder, 'heasarc_ngc2000.tdat.gz')):
    if row['name'] == 'NGC 5866':
        objects[102] = (row['name'], float(row['ra']), float(row['dec']), row['app_mag'])
assert sorted(objects) == list(range(1, 111)), sorted(set(range(1, 111)) - set(objects))

for number in range(1, 111):
    name, ra, dec, magnitude = objects[number]
    magnitude = MAGNITUDES.get(number) or float(magnitude)
    # Degrees of right ascension to tenths of a minute of time (24 per hour: 600 per 15 degrees).
    print(f'    {{ {round(ra * 40)}, {round(dec * 60)}, {round(magnitude * 10)} }},  // M{number} {name.replace(" ", "") or "(no NGC number)"}')
