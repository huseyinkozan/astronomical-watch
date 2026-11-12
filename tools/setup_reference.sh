#!/bin/sh
# Sets up the Python environment that produces the astronomy reference values
# (tools/make_astronomy_reference.py): Skyfield and the JPL DE421 ephemeris (about 17 MB),
# both inside tools/.venv, which git ignores. Not needed to build or flash the firmware.
#
#     sh tools/setup_reference.sh
set -e
cd "$(dirname "$0")"
python3 -m venv .venv
.venv/bin/pip install --quiet --upgrade pip
.venv/bin/pip install --quiet -r requirements-reference.txt
# Skyfield downloads the ephemeris once into this directory.
.venv/bin/python -c "from skyfield.api import Loader; Loader('.venv/skyfield-data')('de421.bsp')"
echo "Reference environment ready: tools/.venv"
