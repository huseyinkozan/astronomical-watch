#!/usr/bin/env python3
"""Reference values for tests/AstrolabeTest.cpp, computed with Skyfield and the JPL DE421
ephemeris: the bright stars of main/Astrolabe.cpp's table seen from a few places (precession,
nutation and aberration included), the Sun's altitude in the morning and the afternoon, and
Polaris' altitude, so that the test can check the alidade's answers (the time from the Sun, the
latitude from Polaris) against an independent computation.

    sh tools/setup_reference.sh                      # once
    tools/.venv/bin/python tools/make_astrolabe_reference.py > tests/astrolabe_reference.csv

Rows: "star", star index, latitude, longitude, Unix time, altitude, azimuth;
"sun" or "polaris", latitude, longitude, Unix time, altitude. Geometric altitudes (no
refraction), degrees.
"""

import datetime
import os
import re

from skyfield.api import Loader, Star, wgs84

HERE = os.path.dirname(os.path.abspath(__file__))
load = Loader(os.path.join(HERE, '.venv', 'skyfield-data'))
eph = load('de421.bsp')
ts = load.timescale()
sun, earth = eph['sun'], eph['earth']

# The table as the watch has it: { "name", raHours, decDegrees, magnitude }.
source = open(os.path.join(HERE, '..', 'main', 'Astrolabe.cpp')).read()
STARS = [(m.group(1), float(m.group(2)), float(m.group(3)))
         for m in re.finditer(r'\{ "([^"]+)", ([-0-9.]+), ([-0-9.]+), [-0-9.]+ \}', source)]
assert len(STARS) == 20, len(STARS)

PLACES = [
    ('istanbul', 41.01, 28.98),
    ('tromso', 69.65, 18.96),
    ('sydney', -33.87, 151.21),
    ('quito', -0.18, -78.47),
    ('honolulu', 21.31, -157.86),
]
DATES = ['2026-03-20', '2026-06-21', '2026-09-23', '2026-12-21', '2030-01-15']


def unix(t):
    return round(t.utc_datetime().timestamp())


def at(date, hours):
    day = datetime.datetime.strptime(date, '%Y-%m-%d').replace(tzinfo=datetime.timezone.utc)
    return ts.from_datetime(day + datetime.timedelta(hours=hours))


def altaz(place, body, t):
    alt, az, _ = (earth + place).at(t).observe(body).apparent().altaz()
    return alt.degrees, az.degrees


print('# Made by tools/make_astrolabe_reference.py (Skyfield, DE421); geometric altitudes.')
for name, lat, lon in PLACES:
    place = wgs84.latlon(lat, lon)
    for date in DATES:
        # Stars at local mean midnight.
        t = at(date, (24 - lon / 15) % 24)
        for index, (star, ra, dec) in enumerate(STARS):
            alt, az = altaz(place, Star(ra_hours=ra, dec_degrees=dec), t)
            print(f'star,{index},{lat},{lon},{unix(t)},{alt:.5f},{az:.5f}')
        # The Sun in the morning and the afternoon (local mean time), when it stands high enough
        # for a sighting and far enough from noon for the time to be well defined.
        for local in (7.5, 8.5, 10.0, 14.0, 15.5, 16.5):
            t = at(date, (local - lon / 15) % 24)
            alt, _ = altaz(place, sun, t)
            if alt > 5:
                print(f'sun,{lat},{lon},{unix(t)},{alt:.5f}')
        # Polaris through the night, in the northern hemisphere.
        if lat > 5:
            for local in (20, 23, 2, 5):
                t = at(date, (local - lon / 15) % 24)
                alt, _ = altaz(place, Star(ra_hours=STARS[-1][1], dec_degrees=STARS[-1][2]), t)
                print(f'polaris,{lat},{lon},{unix(t)},{alt:.5f}')
