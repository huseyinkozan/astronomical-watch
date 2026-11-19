#!/usr/bin/env python3
"""Reference values for tests/PlanetsTest.cpp, computed with Skyfield and the JPL DE421
ephemeris, far more accurate than the watch's approximate elements.

    sh tools/setup_reference.sh                      # once
    tools/.venv/bin/python tools/make_planet_reference.py > tests/planet_reference.csv

Rows: unix time, body, quantity, value. Heliocentric positions are geometric, in the J2000
ecliptic (degrees, AU); geocentric ones astrometric J2000 right ascension (hours), declination
(degrees) and distance (AU); altitude and azimuth apparent, without refraction, at the places.
"""

import datetime
import os

from skyfield.api import Loader, wgs84
from skyfield.framelib import ecliptic_J2000_frame

HERE = os.path.dirname(os.path.abspath(__file__))
load = Loader(os.path.join(HERE, '.venv', 'skyfield-data'))
eph = load('de421.bsp')
ts = load.timescale()
sun, earth = eph['sun'], eph['earth']

# As main/Planets.h's Planets::Body.
BODIES = [('mercury', 'mercury'), ('venus', 'venus'), ('earth', 'earth'), ('mars', 'mars'),
          ('jupiter', 'jupiter barycenter'), ('saturn', 'saturn barycenter')]
PLACES = [('istanbul', 41.01, 28.98), ('sydney', -33.87, 151.21)]
# Across the elements' range 1800-2050, and the screenshots' scene (2026-06-21 19:19 UTC+3).
MOMENTS = ['2000-01-01 12:00', '2005-06-15 12:00', '2010-03-01 12:00', '2015-09-01 12:00',
           '2020-12-21 12:00', '2024-04-08 12:00', '2026-06-21 16:19', '2026-11-19 12:00',
           '2030-07-01 12:00', '2035-01-15 12:00', '2040-08-01 12:00', '2045-02-01 12:00',
           '2050-12-31 12:00']


def main():
    print('unix,body,quantity,value')
    for moment in MOMENTS:
        when = datetime.datetime.strptime(moment, '%Y-%m-%d %H:%M').replace(tzinfo=datetime.timezone.utc)
        t = ts.from_datetime(when)
        unix = round(when.timestamp())
        for name, key in BODIES:
            body = eph[key]
            lat, lon, dist = (body - sun).at(t).frame_latlon(ecliptic_J2000_frame)
            print(f'{unix},{name},helio_longitude,{lon.degrees:.6f}')
            print(f'{unix},{name},helio_latitude,{lat.degrees:.6f}')
            print(f'{unix},{name},helio_distance,{dist.au:.7f}')
            if name == 'earth':
                continue
            ra, dec, dist = earth.at(t).observe(body).radec()
            print(f'{unix},{name},ra,{ra.hours:.7f}')
            print(f'{unix},{name},dec,{dec.degrees:.6f}')
            print(f'{unix},{name},distance,{dist.au:.7f}')
            for place, latitude, longitude in PLACES:
                observer = earth + wgs84.latlon(latitude, longitude)
                alt, az, _ = observer.at(t).observe(body).apparent().altaz()
                print(f'{unix},{name},{place}_altitude,{alt.degrees:.5f}')
                print(f'{unix},{name},{place}_azimuth,{az.degrees:.5f}')


if __name__ == '__main__':
    main()
