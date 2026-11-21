#!/usr/bin/env python3
"""Reference values for tests/ObservingTest.cpp, computed with Skyfield and the JPL DE421
ephemeris: the pole stars' hour angles (Polaris, sigma Octantis), the galactic centre's altitude,
tonight's astronomical darkness, the Milky Way window by the watch's rule (main/Observing.h),
and Messier objects' altitudes and best times (main/Messier.h).

    sh tools/setup_reference.sh                      # once
    tools/.venv/bin/python tools/make_observing_reference.py > tests/observing_reference.csv

Rows (angles in degrees, times Unix seconds, geometric altitudes without refraction):
  pole, south (0/1), longitude, time, hour angle (apparent, 0 to 360)
  centre, latitude, longitude, time, altitude
  night, latitude, longitude, local noon, dark (0/1), dusk, dawn
  window, latitude, longitude, local noon, window (0/1), start, end, highest, highest altitude
  messier, index, latitude, longitude, time, altitude
  best, index, latitude, longitude, local noon, best time (0: no darkness)
The window and the best times are found minute by minute.
"""

import datetime
import os
import re

import numpy as np
from skyfield import almanac
from skyfield.api import Loader, Star, wgs84

HERE = os.path.dirname(os.path.abspath(__file__))
load = Loader(os.path.join(HERE, '.venv', 'skyfield-data'))
eph = load('de421.bsp')
ts = load.timescale()
sun, moon, earth = eph['sun'], eph['moon'], eph['earth']

POLARIS = Star(ra_hours=(2, 31, 49.09), dec_degrees=(89, 15, 50.8))
SIGMA_OCT = Star(ra_hours=(21, 8, 46.86), dec_degrees=(-88, -57, -23.4))
CENTRE = Star(ra_hours=(17, 45, 40.04), dec_degrees=(-29, 0, -28.1))

# The table as the watch has it: { tenths of a minute, arcminutes, magnitude }.
source = open(os.path.join(HERE, '..', 'main', 'Messier.cpp')).read()
MESSIER = [Star(ra_hours=int(m.group(1)) / 600, dec_degrees=int(m.group(2)) / 60)
           for m in re.finditer(r'\{ (\d+), (-?\d+), \d+ \},', source)]
assert len(MESSIER) == 110, len(MESSIER)

# Name, latitude, longitude, UTC offset in hours.
PLACES = [
    ('istanbul', 41.01, 28.98, 3),
    ('sydney', -33.87, 151.21, 10),
    ('quito', -0.18, -78.47, -5),
    ('tromso', 69.65, 18.96, 1),
    ('denver', 39.74, -104.99, -7),
    ('capetown', -33.92, 18.42, 2),
]
DATES = ['2026-03-20', '2026-05-16', '2026-06-21', '2026-07-15', '2026-08-10', '2026-09-23', '2026-12-21',
         '2027-04-07']


def unix(t):
    return round(t.utc_datetime().timestamp())


def local_noon(date, offset):
    day = datetime.datetime.strptime(date, '%Y-%m-%d').replace(tzinfo=datetime.timezone.utc)
    return ts.from_datetime(day + datetime.timedelta(hours=12 - offset))


def altitude(place, body, t):
    alt, _, _ = (earth + place).at(t).observe(body).apparent().altaz()
    return alt.degrees


print('# Made by tools/make_observing_reference.py (Skyfield, DE421); geometric altitudes.')

# Pole stars: the apparent hour angle (Greenwich apparent sidereal time).
for date in DATES:
    for hours in (0, 7.5, 15.25):
        t = ts.from_datetime(datetime.datetime.strptime(date, '%Y-%m-%d').replace(tzinfo=datetime.timezone.utc)
                             + datetime.timedelta(hours=hours))
        for south, star in ((0, POLARIS), (1, SIGMA_OCT)):
            ra, _, _ = earth.at(t).observe(star).apparent().radec(epoch='date')
            for lon in (28.98, -104.99, 151.21):
                hour_angle = (t.gast * 15 + lon - ra.hours * 15) % 360
                print(f'pole,{south},{lon},{unix(t)},{hour_angle:.4f}')

for name, lat, lon, offset in PLACES:
    place = wgs84.latlon(lat, lon)
    observer = earth + place
    for date in DATES:
        noon = local_noon(date, offset)
        # A day from local noon, minute by minute.
        minutes = ts.tt_jd(noon.tt + np.arange(0, 1441) / 1440)
        sun_alt = observer.at(minutes).observe(sun).apparent().altaz()[0].degrees
        centre_alt = observer.at(minutes).observe(CENTRE).apparent().altaz()[0].degrees
        dark = sun_alt < -18

        for k in (360, 720, 1080):
            print(f'centre,{lat},{lon},{unix(minutes[k])},{centre_alt[k]:.4f}')

        # Astronomical darkness: the Sun's centre 18 degrees down, found to the second.
        def is_dark(t):
            return observer.at(t).observe(sun).apparent().altaz()[0].degrees < -18
        is_dark.step_days = 1 / 48
        times, values = almanac.find_discrete(noon, ts.tt_jd(noon.tt + 1), is_dark)
        dusk = [unix(t) for t, v in zip(times, values) if v]
        dawn = [unix(t) for t, v in zip(times, values) if not v]
        if dark.all():
            print(f'night,{lat},{lon},{unix(noon)},1,{unix(noon)},{unix(noon) + 86400}')
        elif dusk and dawn:
            print(f'night,{lat},{lon},{unix(noon)},1,{dusk[0]},{dawn[-1]}')
        elif not dark.any():
            print(f'night,{lat},{lon},{unix(noon)},0,0,0')

        # The window: dark, the centre 10 degrees up, the Moon down or faint (lit at the middle
        # of the night); its longest run of minutes.
        if dark.any():
            index = np.nonzero(dark)[0]
            middle = minutes[(index[0] + index[-1]) // 2]
            lit = almanac.fraction_illuminated(eph, 'moon', middle)
            moon_ok = np.ones(len(minutes), bool)
            if lit >= 0.10:
                moon_ok = observer.at(minutes).observe(moon).apparent().altaz()[0].degrees < 0
            good = dark & (centre_alt > 10) & moon_ok
            best, start = (0, 0), None
            for i, g in enumerate(list(good) + [False]):
                if g and start is None:
                    start = i
                elif not g and start is not None:
                    if i - start > best[1] - best[0]:
                        best = (start, i)
                    start = None
            if best[1] - best[0] - 1 >= 30:
                a, b = best[0], best[1] - 1
                top = a + int(np.argmax(centre_alt[a:b + 1]))
                print(f'window,{lat},{lon},{unix(noon)},1,{unix(minutes[a])},{unix(minutes[b])},'
                      f'{unix(minutes[top])},{centre_alt[top]:.4f}')
            elif best[1] - best[0] < 20:
                print(f'window,{lat},{lon},{unix(noon)},0,0,0,0,0')

        # Messier objects: altitudes at three times, and the best time in the dark.
        for i in range(0, 110, 9):
            for k in (0, 600, 1200):
                alt = altitude(place, MESSIER[i], minutes[k])
                print(f'messier,{i},{lat},{lon},{unix(minutes[k])},{alt:.4f}')
            if dark.any() and not dark.all():
                alt = observer.at(minutes).observe(MESSIER[i]).apparent().altaz()[0].degrees
                masked = np.where(dark, alt, -999)
                print(f'best,{i},{lat},{lon},{unix(noon)},{unix(minutes[int(np.argmax(masked))])}')
            elif not dark.any():
                print(f'best,{i},{lat},{lon},{unix(noon)},0')
