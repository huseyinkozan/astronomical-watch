#!/usr/bin/env python3
"""Animations of the README from the screenshots of tools/screenshot.py: every always-on style
(always-on-<style>.png -> always-on.gif) and every watch face layout (face-<style>.png ->
faces.gif), one after the other, as a swipe up or down on the watch steps through them.

    tools/.venv/bin/python tools/make_gifs.py    (Pillow, see tools/setup_reference.sh)
"""

import argparse
import glob
import os
import re

from PIL import Image

FRAME_MS = 1500


def frames(directory, prefix):
    paths = glob.glob(os.path.join(directory, f'{prefix}-*.png'))
    numbered = []
    for path in paths:
        match = re.fullmatch(rf'{prefix}-(\d+)\.png', os.path.basename(path))
        if match:
            numbered.append((int(match.group(1)), path))
    return [Image.open(path).convert('RGB') for _, path in sorted(numbered)]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--dir', default='docs/screenshots')
    args = parser.parse_args()
    for prefix, name in (('always-on', 'always-on.gif'), ('face', 'faces.gif')):
        images = frames(args.dir, prefix)
        if not images:
            print(f'{prefix}: no screenshots')
            continue
        path = os.path.join(args.dir, name)
        images[0].save(path, save_all=True, append_images=images[1:], duration=FRAME_MS, loop=0, optimize=True)
        print(f'{path}: {len(images)} frames')


if __name__ == '__main__':
    main()
