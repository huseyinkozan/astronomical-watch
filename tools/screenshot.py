#!/usr/bin/env python3
"""Screenshots of the watch's active-mode pages, saved as PNG files.

The development build (CONFIG_WATCH_DEV_LOGGING) answers "S <page>" on the USB console: it
shows that page, redraws it and prints each pixel row as "SHOT <y> <x> <runs>", where each run
is "<RGB565 hex>*<count>" and runs are separated by commas. Opening the port resets the board
(Linux raises DTR and RTS), so the watch starts in active mode; the command is repeated until
the watch answers.

Run inside the ESP-IDF environment, which provides pyserial:
    python tools/screenshot.py [--port /dev/ttyACM0] [--out docs/screenshots] [pages...]

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

# Pages are numbered from the left; the watch reports each page's name and answers
# nothing for a number past its last page.
MAX_PAGES = 8


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
    """Asks for one page; returns (name, width, height, rows, missing row count) or None."""
    width = height = 0
    name = ''
    rows = None
    received = set()
    start = time.time()
    lastSent = 0
    while time.time() - start < timeout:
        # A command sent while the watch is still starting is lost: repeat it until it answers.
        if rows is None and time.time() - lastSent > 2:
            port.write(f'S {page}\n'.encode())
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
    parser.add_argument('pages', nargs='*', type=int, help='page numbers from 0 (default: all)')
    parser.add_argument('--port', default='/dev/ttyACM0')
    parser.add_argument('--out', default='docs/screenshots')
    parser.add_argument('--verbose', action='store_true', help='show the watch log while waiting')
    parser.add_argument('--send', action='append', default=[],
                        help='development command to send first, e.g. P (setup portal on); repeatable')
    parser.add_argument('--after', action='append', default=[],
                        help='development command to send at the end, e.g. N (a new portal password, '
                             'so that published screenshots do not show the one in use); repeatable')
    args = parser.parse_args()

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
    if args.send:
        # Opening the port restarted the watch: wait for active mode, then send the commands.
        start = time.time()
        while time.time() - start < 30 and 'Active mode' not in reader.readline():
            pass
        for command in args.send:
            port.write(f'{command}\n'.encode())
        time.sleep(4)
    failures = 0
    for page in args.pages or range(MAX_PAGES):
        # The last page answers quickly; a missing page answers nothing.
        result = capture(port, reader, page, timeout=30 if args.pages or page == 0 else 8, verbose=args.verbose)
        if result is None:
            if args.pages or page == 0:
                print(f'page {page}: no answer (is the watch in active mode?)')
                failures += 1
            break
        name, width, height, rows, missing = result
        path = os.path.join(args.out, f'{page}-{name}.png')
        writePng(path, width, height, rows)
        note = f', {missing} rows missing' if missing else ''
        print(f'{name}: {path}{note}')
        if missing:
            failures += 1
    for command in args.after:
        port.write(f'{command}\n'.encode())
    time.sleep(1)
    port.close()
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
