#!/usr/bin/env python3
"""Generates main/AodGlyphs.c: the 1-bit bitmaps of the always-on face.

The always-on wakes draw without LVGL or fonts (main/AlwaysOn.cpp), and the panel's idle mode
shows each colour channel on or off, so every glyph is stored with one bit per pixel. The digit
sets of the face pages' digital clocks (FaceConfig::DigitalStyle), each as HH:MM at the size of
the Large template:

    Mono            JetBrains Mono Light 110, placed as the active face's label
    Seven segment   main/SevenSegment.cpp's geometry at 122 px, each segment filled with dots
                    one pixel apart

plus the small characters of the slot texts: the watch font's set (ASCII, the degree sign and the
Turkish letters, tools/make_fonts.sh) at 26 px, as the active face's slots. The moon and the
weekday icon are drawn on the watch.

    tools/.venv/bin/python tools/make_aod_glyphs.py    (Pillow, see tools/setup_reference.sh)
"""

import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
FONTS = os.path.join(ROOT, 'tools', 'fonts')
OUTPUT = os.path.join(ROOT, 'main', 'AodGlyphs.c')

SMALL_SIZE = 26
# The watch font's characters (tools/make_fonts.sh): ASCII, the degree sign, the Turkish letters.
SMALL_CHARACTERS = [chr(c) for c in range(0x20, 0x7F)] + list('°ÇÖÜçöüĞğİıŞş')
LARGE_HEIGHT = 122

SEGMENTS = [0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F]


def font(weight, size):
    return ImageFont.truetype(os.path.join(FONTS, f'JetBrainsMono-{weight}.ttf'), size)


def threshold(image):
    """Idle mode keeps the most significant bit of a channel: lit from 128 up."""
    return image.point(lambda value: 1 if value >= 128 else 0, mode='1')


def renderText(fnt, characters):
    """Each character in a box shared by all of them (the same baseline), as 1-bit images."""
    size = fnt.size
    canvas = (size * 2, size * 2)
    origin = (size // 2, size * 3 // 2)
    images = []
    box = None
    for character in characters:
        image = Image.new('L', canvas, 0)
        ImageDraw.Draw(image).text(origin, character, font=fnt, fill=255, anchor='ls')
        image = threshold(image)
        bbox = image.getbbox()
        if bbox:
            box = bbox if box is None else (min(box[0], bbox[0]), min(box[1], bbox[1]),
                                            max(box[2], bbox[2]), max(box[3], bbox[3]))
        images.append(image)
    return [image.crop(box) for image in images], box


def monoStyle(weight, size):
    """HH:MM in monospaced cells, as an LVGL label of the same font shows it."""
    fnt = font(weight, size)
    origin = (size // 2, size * 3 // 2)
    digits, box = renderText(fnt, '0123456789')
    colonImage = Image.new('L', (size * 2, size * 2), 0)
    ImageDraw.Draw(colonImage).text(origin, ':', font=fnt, fill=255, anchor='ls')
    colonImage = threshold(colonImage)
    colonLeft, _, colonRight, _ = colonImage.getbbox()
    colon = colonImage.crop((colonLeft, box[1], colonRight, box[3]))
    step = round(fnt.getlength('0'))
    digitOffset = box[0] - origin[0]
    colonOffset = colonLeft - origin[0]
    x = [digitOffset, step + digitOffset, 2 * step + colonOffset, 3 * step + digitOffset, 4 * step + digitOffset]
    left = min(x)
    x = [value - left for value in x]
    width, height = digits[0].size
    return digits, colon, x, [0, 0, 0, 0, 0], x[4] + width, height


# --- Seven segments --------------------------------------------------------------------------

def hexagon(x0, y0, x1, y1):
    """A segment with pointed ends along its long axis."""
    if x1 - x0 >= y1 - y0:
        half = (y1 - y0) / 2
        middle = (y0 + y1) / 2
        return [(x0, middle), (x0 + half, y0), (x1 - half, y0), (x1, middle), (x1 - half, y1), (x0 + half, y1)]
    half = (x1 - x0) / 2
    middle = (x0 + x1) / 2
    return [(middle, y0), (x1, y0 + half), (x1, y1 - half), (middle, y1), (x0, y1 - half), (x0, y0 + half)]


def tiledFill(pixels, polygon, gap, size):
    """The inside of the polygon, on a grid of dots `gap` pixels apart (glyph coordinates)."""
    mask = Image.new('1', size, 0)
    ImageDraw.Draw(mask).polygon(polygon, fill=1)
    period = gap + 1
    for y in range(0, size[1], period):
        for x in range(0, size[0], period):
            if mask.getpixel((x, y)):
                pixels.add((x, y))


def toImage(pixels, size):
    image = Image.new('1', size, 0)
    for x, y in pixels:
        if 0 <= x < size[0] and 0 <= y < size[1]:
            image.putpixel((x, y), 1)
    return image


def even(value):
    """Glyph places on even pixels, so that the dot grids of all glyphs line up."""
    return 2 * round(value / 2)


def segmentDigit(digit, h):
    """main/SevenSegment.cpp: width 0.62 h, segments h / 11 thick, gaps h / 40 at the joints."""
    w = h * 0.62
    t = h / 11
    g = h / 40
    m = h / 2
    boxes = [(t / 2 + g, 0, w - t / 2 - g, t), (w - t, t / 2 + g, w, m - g), (w - t, m + g, w, h - t / 2 - g),
             (t / 2 + g, h - t, w - t / 2 - g, h), (0, m + g, t, h - t / 2 - g), (0, t / 2 + g, t, m - g),
             (t / 2 + g, m - t / 2, w - t / 2 - g, m + t / 2)]
    size = (int(w) + 2, h + 1)
    pixels = set()
    for i in range(7):
        if SEGMENTS[digit] & (1 << i):
            tiledFill(pixels, hexagon(*boxes[i]), 1, size)
    return toImage(pixels, size)


def segmentColon(h):
    """Two squares of h / 11 at 0.3 and 0.7 of the height."""
    side = h / 11
    size = (even(side) + 2, h + 1)
    pixels = set()
    for centre in (0.3 * h, 0.7 * h):
        top = centre - side / 2
        tiledFill(pixels, [(0, top), (side, top), (side, top + side), (0, top + side)], 1, size)
    return toImage(pixels, size)


def segmentStyle(h):
    """SevenSegment::layout: 0.16 of a digit's width between the digits of a pair, 0.18 on each
    side of the colon."""
    w = h * 0.62
    pairGap = 0.16 * w
    colonGap = 0.18 * w
    side = h / 11
    digits = [segmentDigit(d, h) for d in range(10)]
    colon = segmentColon(h)
    x2 = 2 * w + pairGap + 2 * colonGap + side
    x = [0, even(w + pairGap), even(2 * w + pairGap + colonGap), even(x2), even(x2 + w + pairGap)]
    return digits, colon, x, [0, 0, 0, 0, 0], x[4] + digits[0].size[0], h + 1


# --- Output ----------------------------------------------------------------------------------

def packed(image):
    """LVGL's 1-bpp glyph format: rows one after another without padding, MSB first."""
    width, height = image.size
    data = bytearray()
    value = 0
    count = 0
    for y in range(height):
        for x in range(width):
            value = (value << 1) | (1 if image.getpixel((x, y)) else 0)
            count += 1
            if count == 8:
                data.append(value)
                value = count = 0
    if count:
        data.append(value << (8 - count))
    return bytes(data)


class Writer:
    """One bitmap array for every glyph; each glyph starts on a byte."""

    def __init__(self):
        self.data = bytearray()
        self.offsets = {}

    def add(self, image):
        key = (image.size, image.tobytes())
        if key not in self.offsets:
            self.offsets[key] = len(self.data)
            self.data += packed(image)
        return self.offsets[key]

    def glyph(self, image):
        if image is None:
            return '{ 0, 0, NULL }'
        return f'{{ {image.size[0]}, {image.size[1]}, s_bits + {self.add(image)} }}'


def main():
    styles = [
        ('Mono', monoStyle('Light', 110)),
        ('Seven segment', segmentStyle(LARGE_HEIGHT)),
    ]

    writer = Writer()
    styleLines = []
    for name, (digits, colon, x, y, width, height) in styles:
        glyphs = ',\n        '.join(writer.glyph(image) for image in digits)
        styleLines.append(
            f'    // {name}\n'
            f'    {{ {{ {glyphs} }},\n'
            f'      {writer.glyph(colon)},\n'
            f'      {{ {", ".join(str(v) for v in x)} }},\n'
            f'      {{ {", ".join(str(v) for v in y)} }},\n'
            f'      {width}, {height} }}')

    characters = SMALL_CHARACTERS
    small, _ = renderText(font('Regular', SMALL_SIZE), characters)
    advance = round(font('Regular', SMALL_SIZE).getlength('0'))
    codes = [ord(c) for c in characters]
    small = [Image.new('1', (0, 0)) if c == ' ' else image for c, image in zip(characters, small)]
    smallGlyphs = ',\n    '.join(writer.glyph(image) for image in small)
    lineHeight = max(image.size[1] for image in small)

    rows = []
    for i in range(0, len(writer.data), 16):
        rows.append('    ' + ' '.join(f'0x{b:02X},' for b in writer.data[i:i + 16]))
    with open(OUTPUT, 'w', encoding='utf-8') as output:
        output.write('// Generated by tools/make_aod_glyphs.py from JetBrains Mono (tools/fonts/, SIL Open Font\n'
                     '// License); do not edit.\n\n'
                     '#include "AodGlyphs.h"\n\n'
                     f'static const uint8_t s_bits[{len(writer.data)}] = {{\n' + '\n'.join(rows) + '\n};\n\n'
                     'const AodStyle aodStyles[AOD_STYLE_COUNT] = {\n' + ',\n'.join(styleLines) + '\n};\n\n'
                     'const uint16_t aodSmallCodes[AOD_SMALL_COUNT] = { '
                     + ', '.join(f'0x{c:X}' for c in codes) + ' };\n\n'
                     'const AodGlyph aodSmall[AOD_SMALL_COUNT] = {\n    ' + smallGlyphs + '\n};\n\n'
                     f'const int aodSmallAdvance = {advance};\n'
                     f'const int aodSmallHeight = {lineHeight};\n')
    print(f'{OUTPUT}: {len(writer.data)} bytes of bitmaps; AOD_STYLE_COUNT must be {len(styles)}, '
          f'AOD_SMALL_COUNT {len(codes)}, advance {advance}, line {lineHeight}')


if __name__ == '__main__':
    main()
