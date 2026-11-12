#!/usr/bin/env python3
"""Screenshots of the watch's active-mode pages, saved as PNG files.

The development build (CONFIG_WATCH_DEV_LOGGING) answers "S <page>" on the USB console: it
shows that page, redraws it and prints each pixel row as "SHOT <y> <x> <runs>", where each run
is "<RGB565 hex>*<count>" and runs are separated by commas. "A" gives the always-on face, drawn
without LVGL, as "SHOT RECT <x> <y> <width> <height> <0|1>" (black or white) rectangles; it is
shown here at full brightness, dimmer on the watch. Opening the port resets the board
(Linux raises DTR and RTS), so the watch starts in active mode; the command is repeated until
the watch answers.

Run inside the ESP-IDF environment, which provides pyserial:
    python tools/screenshot.py [--port /dev/ttyACM0] [--out docs/screenshots] [pages...]

The pages show a fixed scene (the "D" command: Istanbul, 2026-06-21 19:19 local time), so that
the published pictures stay the same and do not show the watch's location; --live keeps the
watch's own time and settings.

Screenshots for the repository (before a push): the setup portal on, then off with a new
password, so that the published one is not in use:
    python tools/screenshot.py --send P --after P --after N
"""

import argparse
import os
import struct
import sys
import time
import zlib

import serial



def writePng(path, width, height, rows):
    """Minimal 8-bit RGB PNG writer, so that no imaging library is needed."""
    raw = b''.join(b'\x00' + bytes(rows[y]) for y in range(height))

    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body) & 0xFFFFFFFF)

    with open(path, 'wb') as output:
        output.write(b'\x89PNG\r\n\x1a\n')
        output.write(chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)))
        output.write(chunk(b'IDAT', zlib.compress(raw, 9)))
        output.write(chunk(b'IEND', b''))


def toRgb(runs):
    """Colour runs ("<RGB565 hex>*<count>,...") to RGB bytes."""
    out = bytearray()
    for run in runs.split(','):
        colour, count = run.split('*')
        value = int(colour, 16)
        red = (value >> 11) & 0x1F
        green = (value >> 5) & 0x3F
        blue = value & 0x1F
        out += bytes(((red * 255) // 31, (green * 255) // 63, (blue * 255) // 31)) * int(count)
    return out


class LineReader:
    """Reads lines in large chunks: pyserial's readline() reads one byte per call, too slow here."""

    def __init__(self, port):
        self.port = port
        self.pending = b''

    def readline(self):
        while b'\n' not in self.pending:
            data = self.port.read(max(1, self.port.in_waiting))
            if not data:
                return ''
            self.pending += data
        line, self.pending = self.pending.split(b'\n', 1)
        return line.decode(errors='replace').strip()


def capture(port, reader, page, timeout=30, verbose=False):
    """Asks for one page (-1: the always-on face); returns (name, width, height, rows, missing
    row count) or None."""
    width = height = 0
    name = ''
    rows = None
    received = set()
    start = time.time()
    lastSent = 0
    # `timeout` is for the first answer (short when probing past the last page); the transfer
    # itself takes several seconds.
    while time.time() - start < (timeout if rows is None else 60):
        # A command sent while the watch is still starting is lost: repeat it until it answers.
        if rows is None and time.time() - lastSent > 2:
            port.write((f'S {page}' if page >= 0 else 'A').encode() + b'\n')
            lastSent = time.time()
        line = reader.readline()
        parts = line.split()
        if len(parts) < 2 or parts[0] != 'SHOT':
            if verbose and line:
                print(f'    {line}')
            continue
        if parts[1] == 'BEGIN' and len(parts) == 6:
            width, height, name = int(parts[3]), int(parts[4]), parts[5]
            rows = [bytearray(width * 3) for _ in range(height)]
        elif parts[1] == 'RECT' and rows is not None and len(parts) == 7:
            x, y, w, h, white = (int(value) for value in parts[2:])
            for row in range(y, min(y + h, height)):
                rows[row][x * 3:(x + w) * 3] = (b'\xff\xff\xff' if white else b'\x00\x00\x00') * w
                received.add(row)
        elif parts[1] == 'END':
            if rows is None:
                return None
            return name, width, height, rows, height - len(received)
        elif rows is not None and len(parts) == 4:
            y, x = int(parts[1]), int(parts[2])
            try:
                pixels = toRgb(parts[3])
            except ValueError:
                continue
            if 0 <= y < height and 0 <= x and x * 3 + len(pixels) <= width * 3:
                rows[y][x * 3:x * 3 + len(pixels)] = pixels
                received.add(y)
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('pages', nargs='*', type=int,
                        help='page numbers from 0, -1 for the always-on face (default: all)')
    parser.add_argument('--port', default='/dev/ttyACM0')
    parser.add_argument('--out', default='docs/screenshots')
    parser.add_argument('--verbose', action='store_true', help='show the watch log while waiting')
    parser.add_argument('--send', action='append', default=[],
                        help='development command to send first, e.g. P (setup portal on); repeatable')
    parser.add_argument('--after', action='append', default=[],
                        help='development command to send at the end, e.g. N (a new portal password, '
                             'so that published screenshots do not show the one in use); repeatable')
    parser.add_argument('--live', action='store_true',
                        help="the watch's own time and settings instead of the fixed scene")
    args = parser.parse_args()
    commands = args.send if args.live else ['D'] + args.send

    os.makedirs(args.out, exist_ok=True)

    # Opening the port must not reset the board: keep DTR and RTS released.
    port = serial.Serial()
    port.port = args.port
    port.baudrate = 115200
    port.timeout = 1
    port.dtr = False
    port.rts = False
    port.open()
    reader = LineReader(port)
    if commands:
        # Opening the port restarted the watch: wait for active mode, then send the commands.
        start = time.time()
        while time.time() - start < 30 and 'Active mode' not in reader.readline():
            pass
        for command in commands:
            port.write(f'{command}\n'.encode())
        time.sleep(4)
    failures = 0
    page = 0
    allPages = not args.pages
    pages = list(args.pages)
    while True:
        if allPages:
            target = page
        elif pages:
            target = pages.pop(0)
        else:
            break
        # Past the last page the watch answers nothing: then comes the always-on face (-1).
        lastTry = allPages and target > 0
        result = capture(port, reader, target, timeout=8 if lastTry else 30, verbose=args.verbose)
        if result is None:
            if lastTry:
                allPages = False
                pages = [-1]
                continue
            print(f'page {target}: no answer (is the watch in active mode?)')
            failures += 1
            break
        name, width, height, rows, missing = result
        path = os.path.join(args.out, f'{target}-{name}.png' if target >= 0 else f'{name}.png')
        writePng(path, width, height, rows)
        note = f', {missing} rows missing' if missing else ''
        print(f'{name}: {path}{note}')
        if missing:
            failures += 1
        page += 1
    for command in args.after:
        port.write(f'{command}\n'.encode())
    time.sleep(1)
    port.close()
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
