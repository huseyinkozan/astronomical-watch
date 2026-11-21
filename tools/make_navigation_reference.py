#!/usr/bin/env python3
"""Reference values for tests/NavigationTest.cpp, computed with Skyfield and the JPL DE421
ephemeris: the nautical almanac's places and meridian passages (main/Almanac.h), a dish's
pointing at a geostationary satellite and the Sun's times at an azimuth (main/Aiming.h).

    sh tools/setup_reference.sh                      # once
    tools/.venv/bin/python tools/make_navigation_reference.py > tests/navigation_reference.csv

Rows (angles in degrees, arcminutes where named, times Unix seconds):
  place, body, time, GHA, declination, SHA, semi-diameter (arcmin), horizontal parallax (arcmin)
  pass, body, latitude, longitude, local midnight, occurs (0/1), time, geometric altitude
  dish, latitude, longitude, satellite longitude, elevation, azimuth
  sunaz, latitude, longitude, local midnight, azimuth, occurs (0/1), time
GHA, declination and SHA are apparent, geocentric, of the true equinox of the date (as a nautical
almanac gives them); the semi-diameters from the IAU radii (Sun 696000 km, Moon 1737.4 km); the
passes' and the dish's altitudes geometric (no refraction), the dish on the WGS 84 ellipsoid.
"""

import datetime
import math
import os

import numpy as np
from skyfield import almanac
from skyfield.api import Loader, wgs84
from skyfield.toposlib import ITRSPosition

HERE = os.path.dirname(os.path.abspath(__file__))
load = Loader(os.path.join(HERE, '.venv', 'skyfield-data'))
eph = load('de421.bsp')
ts = load.timescale()
earth = eph['earth']
# In Almanac::Body's order.
BODIES = [('sun', eph['sun']), ('moon', eph['moon']), ('venus', eph['venus']), ('mars', eph['mars']),
          ('jupiter', eph['jupiter barycenter']), ('saturn', eph['saturn barycenter'])]
EARTH_RADIUS = 6378.137
GEOSTATIONARY = 42164.17

# Name, latitude, longitude, UTC offset in hours.
PLACES = [
    ('istanbul', 41.01, 28.98, 3),
    ('sydney', -33.87, 151.21, 10),
    ('quito', -0.18, -78.47, -5),
    ('tromso', 69.65, 18.96, 1),
    ('denver', 39.74, -104.99, -7),
]
DATES = ['2026-03-20', '2026-06-21', '2026-11-21', '2027-04-07', '2028-09-15', '2029-12-30']


def unix(t):
    return round(t.utc_datetime().timestamp())


def at(date, hours):
    day = datetime.datetime.strptime(date, '%Y-%m-%d').replace(tzinfo=datetime.timezone.utc)
    return ts.from_datetime(day + datetime.timedelta(hours=hours))


print('# Made by tools/make_navigation_reference.py (Skyfield, DE421).')

for date in DATES:
    for hours in (0, 9.25, 17.5):
        t = at(date, hours)
        for index, (name, body) in enumerate(BODIES):
            astrometric = earth.at(t).observe(body)
            ra, dec, distance = astrometric.apparent().radec(epoch='date')
            gha = (t.gast * 15 - ra.hours * 15) % 360
            km = distance.km
            sd = math.degrees(math.asin(696000 / km)) * 60 if name == 'sun' else (
                math.degrees(math.asin(1737.4 / km)) * 60 if name == 'moon' else 0)
            hp = math.degrees(math.asin(EARTH_RADIUS / km)) * 60 if name == 'moon' else 0
            print(f'place,{index},{unix(t)},{gha:.5f},{dec.degrees:.5f},{(360 - ra.hours * 15) % 360:.5f},'
                  f'{sd:.4f},{hp:.4f}')

for name, lat, lon, offset in PLACES:
    place = wgs84.latlon(lat, lon)
    for date in DATES:
        midnight = at(date, -offset)
        end = ts.tt_jd(midnight.tt + 1)
        for index, (body_name, body) in enumerate(BODIES):
            times, events = almanac.find_discrete(midnight, end, almanac.meridian_transits(eph, body, place))
            upper = [t for t, e in zip(times, events) if e == 1]
            if upper:
                t = upper[0]
                alt = (earth + place).at(t).observe(body).apparent().altaz()[0].degrees
                print(f'pass,{index},{lat},{lon},{unix(midnight)},1,{unix(t)},{alt:.4f}')
            else:
                print(f'pass,{index},{lat},{lon},{unix(midnight)},0,0,0')

        # The Sun at a few azimuths, minute by minute, then interpolated.
        minutes = ts.tt_jd(midnight.tt + np.arange(0, 1441) / 1440)
        alt, az, _ = (earth + place).at(minutes).observe(eph['sun']).apparent().altaz()
        for target in (100.0, 160.0, 191.8, 250.0, 330.0):
            difference = (az.degrees - target + 180) % 360 - 180
            found = None
            for k in range(1440):
                a, b = difference[k], difference[k + 1]
                if (a <= 0) != (b <= 0) and abs(a - b) < 180 and (alt.degrees[k] > 0 or alt.degrees[k + 1] > 0):
                    t = unix(minutes[k]) + 60 * a / (a - b)
                    if alt.degrees[k] > 0 and alt.degrees[k + 1] > 0:
                        found = round(t)
                        break
                    # At the horizon: left out of the comparison.
                    found = -1
                    break
            if found is None:
                print(f'sunaz,{lat},{lon},{unix(midnight)},{target},0,0')
            elif found > 0:
                print(f'sunaz,{lat},{lon},{unix(midnight)},{target},1,{found}')

# Dishes: a geostationary satellite as an Earth-fixed point on the equator.
t = at('2026-06-21', 12)
for name, lat, lon, offset in PLACES + [('equator', 0.0, 42.0, 0), ('cape', -33.92, 18.42, 2)]:
    place = wgs84.latlon(lat, lon)
    for satellite in (42.0, 31.0, 19.2, 13.0, -7.0, 160.0):
        sat = ITRSPosition(wgs84.latlon(0, satellite, elevation_m=(GEOSTATIONARY - 6378.137) * 1000).itrs_xyz)
        alt, az, _ = (sat - place).at(t).altaz()
        print(f'dish,{lat},{lon},{satellite},{alt.degrees:.4f},{az.degrees:.4f}')
