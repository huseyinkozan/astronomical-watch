#!/usr/bin/env python3
"""Reference places of the planets on the arc faces for tests/PlanetsTest.cpp: the faces' rule
for the Sun and the Moon (main/Corners.cpp, Corners::arcPlace) applied to Skyfield's rise and set
times (DE421, the centre through -0.5667 degrees).

    tools/.venv/bin/python tools/make_planet_arcs.py > tests/planet_arcs.csv

Rows: unix time, latitude, longitude, body, up (1/0), Full arcs angle, Half arcs angle (degrees,
LVGL's: clockwise from 3 o'clock; the Half arcs angle only while up, else "none").
"""

import datetime
import os

from skyfield import almanac
from skyfield.api import Loader, wgs84

HERE = os.path.dirname(os.path.abspath(__file__))
load = Loader(os.path.join(HERE, '.venv', 'skyfield-data'))
eph = load('de421.bsp')
ts = load.timescale()

BODIES = [('mercury', 'mercury'), ('venus', 'venus'), ('mars', 'mars'), ('jupiter', 'jupiter barycenter'),
          ('saturn', 'saturn barycenter'), ('uranus', 'uranus barycenter'), ('neptune', 'neptune barycenter')]
# The screenshots' scene, a winter evening, and the southern hemisphere.
CASES = [('2026-06-21 16:19', 41.0082, 28.9784), ('2026-11-19 18:00', 41.0082, 28.9784),
         ('2026-06-21 10:00', -33.87, 151.21), ('2027-01-15 03:00', -0.18, -78.47)]


def events(observer, body, now):
    t0, t1 = ts.tt_jd(now.tt - 2), ts.tt_jd(now.tt + 2)
    rises, rising = almanac.find_risings(observer, body, t0, t1, horizon_degrees=-0.5667)
    sets, setting = almanac.find_settings(observer, body, t0, t1, horizon_degrees=-0.5667)
    return sorted([(t.tt, True) for t, ok in zip(rises, rising) if ok] +
                  [(t.tt, False) for t, ok in zip(sets, setting) if ok])


def place(ev, n, south):
    for i in range(len(ev) - 1):
        if not ev[i][0] <= n < ev[i + 1][0]:
            continue
        up = ev[i][1]
        if up:
            a, b, c = ev[i][0], ev[i + 1][0], ev[i + 2][0]
        else:
            a, b, c = ev[i - 1][0], ev[i][0], ev[i + 1][0]
        part = (n - a) / (b - a)
        span = 360 * (b - a) / (c - a)
        full = (270 - span / 2 + span * part) if up else (270 + span / 2 + (360 - span) * (n - b) / (c - b))
        half = (360 - 180 * part if south else 180 + 180 * part) if up else None
        return up, full % 360, half
    return None


def main():
    print('unix,latitude,longitude,body,up,full,half')
    for moment, latitude, longitude in CASES:
        when = datetime.datetime.strptime(moment, '%Y-%m-%d %H:%M').replace(tzinfo=datetime.timezone.utc)
        now = ts.from_datetime(when)
        observer = eph['earth'] + wgs84.latlon(latitude, longitude)
        for name, key in BODIES:
            p = place(events(observer, eph[key], now), now.tt, latitude < 0)
            if p is None:
                continue
            up, full, half = p
            half_text = 'none' if half is None else f'{half:.2f}'
            print(f'{round(when.timestamp())},{latitude},{longitude},{name},{int(up)},{full:.2f},{half_text}')


if __name__ == '__main__':
    main()
