#!/usr/bin/env python3
"""Reference rise and set times of the planets for tests/PlanetsTest.cpp (Skyfield, DE421).

    tools/.venv/bin/python tools/make_planet_risings.py > tests/planet_risings.csv

Rows: place, latitude, longitude, local midnight (Unix s), body, rise or set, Unix s or "none":
the first rising and setting of the planet's centre through -0.5667 degrees (refraction, no
semi-diameter) in the local day, by Skyfield's find_risings and find_settings.
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
# As tools/make_astronomy_reference.py: name, latitude, longitude, UTC offset of the local day.
PLACES = [('ankara', 39.93, 32.86, 3), ('istanbul', 41.01, 28.98, 3), ('tromso', 69.65, 18.96, 1),
          ('reykjavik', 64.15, -21.94, 0), ('sydney', -33.87, 151.21, 10), ('quito', -0.18, -78.47, -5),
          ('honolulu', 21.31, -157.86, -10)]
DATES = ['2026-03-20', '2026-06-21', '2026-11-19', '2027-01-15']


def main():
    print('place,latitude,longitude,midnight,body,event,value')
    for place, latitude, longitude, offset in PLACES:
        topos = wgs84.latlon(latitude, longitude)
        for date in DATES:
            day = datetime.datetime.strptime(date, '%Y-%m-%d').replace(tzinfo=datetime.timezone.utc)
            midnight = day - datetime.timedelta(hours=offset)
            start = ts.from_datetime(midnight)
            end = ts.from_datetime(midnight + datetime.timedelta(days=1))
            unix = round(midnight.timestamp())
            observer = eph['earth'] + topos
            for name, key in BODIES:
                # find_risings/find_settings (not risings_and_settings, which missed a short dip of
                # Mars below the horizon at Tromso); the flag is false when the body only grazes.
                rises, rising = almanac.find_risings(observer, eph[key], start, end, horizon_degrees=-0.5667)
                sets, setting = almanac.find_settings(observer, eph[key], start, end, horizon_degrees=-0.5667)
                rise = next((round(t.utc_datetime().timestamp()) for t, ok in zip(rises, rising) if ok), 'none')
                set_ = next((round(t.utc_datetime().timestamp()) for t, ok in zip(sets, setting) if ok), 'none')
                print(f'{place},{latitude},{longitude},{unix},{name},rise,{rise}')
                print(f'{place},{latitude},{longitude},{unix},{name},set,{set_}')


if __name__ == '__main__':
    main()
