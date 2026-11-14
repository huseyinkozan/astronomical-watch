#!/usr/bin/env python3
"""Generates main/AodGlyphs.c: the 1-bit bitmaps of the always-on face.

The always-on wakes draw without LVGL or fonts (main/AlwaysOn.cpp), and the panel's idle mode
shows each colour channel on or off, so every glyph is stored with one bit per pixel. Digit
sets, each as HH:MM:

    Thin, Regular   JetBrains Mono
    Dense           seven segments, each a hexagon tiled with dots 1 pixel apart
    Thin 120        the smaller Thin digits of the sky arcs face

plus the small characters of the date and time (solid, and dotted for the seven-segment style). The same
bitmaps also form
LVGL fonts (1 bpp), with which the always-on page of active mode shows the styles. The moon is
drawn on the watch, since it changes with the phase.

    tools/.venv/bin/python tools/make_aod_glyphs.py    (Pillow, see tools/setup_reference.sh)
"""

import argparse
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
FONTS = os.path.join(ROOT, 'tools', 'fonts')
OUTPUT = os.path.join(ROOT, 'main', 'AodGlyphs.c')

# Day names on the face (English now, Turkish for M11): their letters and the digits.
DAY_NAMES = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun', 'Pzt', 'Sal', 'Çar', 'Per', 'Cum', 'Cmt', 'Paz']
SMALL_SIZE = 36
# The always-on date "21 June" (the sky faces): the month names' letters, English and Turkish.
MONTH_NAMES = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September',
               'October', 'November', 'December', 'Ocak', 'Şubat', 'Mart', 'Nisan', 'Mayıs', 'Haziran',
               'Temmuz', 'Ağustos', 'Eylül', 'Ekim', 'Kasım', 'Aralık']

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


def fontStyle(weight, size):
    fnt = font(weight, size)
    digits, box = renderText(fnt, '0123456789')
    colonImage = Image.new('L', (size * 2, size * 2), 0)
    ImageDraw.Draw(colonImage).text((size // 2, size * 3 // 2), ':', font=fnt, fill=255, anchor='ls')
    colonImage = threshold(colonImage)
    left, _, right, _ = colonImage.getbbox()
    colon = colonImage.crop((left, box[1], right, box[3]))
    step = round(fnt.getlength('0'))
    width, height = digits[0].size
    # A narrow colon: the monospaced one would take a whole digit cell.
    space = round(size * 0.08)
    colonX = step + width + space
    minutesX = colonX + colon.size[0] + space
    x = [0, step, colonX, minutesX, minutesX + step]
    y = [0, 0, 0, 0, 0]
    return digits, colon, x, y, minutesX + step + width, height


# --- Seven segments --------------------------------------------------------------------------

DIGIT_W, DIGIT_H, STROKE, JOINT_GAP = 76, 170, 20, 4


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


def segmentDigit(digit):
    w, h, t, g = DIGIT_W, DIGIT_H, STROKE, JOINT_GAP
    m = h // 2
    boxes = [(t // 2 + g, 0, w - t // 2 - g, t), (w - t, t // 2 + g, w, m - g), (w - t, m + g, w, h - t // 2 - g),
             (t // 2 + g, h - t, w - t // 2 - g, h), (0, m + g, t, h - t // 2 - g), (0, t // 2 + g, t, m - g),
             (t // 2 + g, m - t // 2, w - t // 2 - g, m + t // 2)]
    size = (w + 1, h + 1)
    pixels = set()
    for i in range(7):
        if SEGMENTS[digit] & (1 << i):
            tiledFill(pixels, hexagon(*boxes[i]), 1, size)
    return toImage(pixels, size)


def segmentColon():
    # Two squares of 14 pixels, placed like the colon of the font styles.
    w, h = 15, DIGIT_H + 1
    pixels = set()
    for top in (DIGIT_H * 3 // 10 - 7, DIGIT_H - DIGIT_H * 3 // 10 - 7):
        square = [(0, top), (14, top), (14, top + 14), (0, top + 14)]
        tiledFill(pixels, square, 1, (w, h))
    return toImage(pixels, (w, h))


def segmentStyle():
    digits = [segmentDigit(d) for d in range(10)]
    colon = segmentColon()
    # Positions on even pixels, so that the dot grids of all glyphs line up; the colon in the
    # middle between the hours and the minutes.
    step = 90
    hoursEnd = step + DIGIT_W + 1
    minutesX = hoursEnd + 30
    minutesX += -minutesX % 6
    colonX = round((hoursEnd + minutesX - colon.size[0]) / 12) * 6
    x = [0, step, colonX, minutesX, minutesX + step]
    return digits, colon, x, [0, 0, 0, 0, 0], x[4] + DIGIT_W + 1, DIGIT_H + 1


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


def dottedImage(image):
    """Every second pixel of every second row (glyph coordinates), for the dotted styles."""
    out = Image.new('1', image.size, 0)
    for y in range(0, image.size[1], 2):
        for x in range(0, image.size[0], 2):
            if image.getpixel((x, y)):
                out.putpixel((x, y), 1)
    return out


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


def lvglFont(writer, name, codes, images, advances, offsetsX, lineHeight):
    """An LVGL font (lv_font_fmt_txt, 1 bpp): the glyphs hang from the top of the line, so a
    label at (x, y) puts a glyph of the line's full height at (x + ofs_x, y)."""
    lines = ['    { .bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0 },']
    for image, advance, offsetX in zip(images, advances, offsetsX):
        if image is None or image.size[0] == 0:
            lines.append(f'    {{ .bitmap_index = 0, .adv_w = {advance * 16}, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0 }},')
            continue
        lines.append(f'    {{ .bitmap_index = {writer.add(image)}, .adv_w = {advance * 16}, .box_w = {image.size[0]}, '
                     f'.box_h = {image.size[1]}, .ofs_x = {offsetX}, .ofs_y = 0 }},')
    first = codes[0]
    offsets = ', '.join(str(code - first) for code in codes)
    return (f'static const lv_font_fmt_txt_glyph_dsc_t {name}_glyphs[] = {{\n' + '\n'.join(lines) + '\n};\n'
            f'static const uint16_t {name}_codes[] = {{ {offsets} }};\n'
            f'static const lv_font_fmt_txt_cmap_t {name}_cmap = {{\n'
            f'    .range_start = {first}, .range_length = {codes[-1] - first + 1}, .glyph_id_start = 1,\n'
            f'    .unicode_list = {name}_codes, .glyph_id_ofs_list = NULL, .list_length = {len(codes)},\n'
            f'    .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY\n}};\n'
            f'static const lv_font_fmt_txt_dsc_t {name}_dsc = {{\n'
            f'    .glyph_bitmap = s_bits, .glyph_dsc = {name}_glyphs, .cmaps = &{name}_cmap, .kern_dsc = NULL,\n'
            f'    .kern_scale = 0, .cmap_num = 1, .bpp = 1, .kern_classes = 0, .bitmap_format = 0\n}};\n'
            f'const lv_font_t {name} = {{\n'
            f'    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt, .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,\n'
            f'    .line_height = {lineHeight}, .base_line = 0, .subpx = LV_FONT_SUBPX_NONE,\n'
            f'    .underline_position = 0, .underline_thickness = 0, .dsc = &{name}_dsc, .fallback = NULL,\n'
            f'    .user_data = NULL\n}};\n')


def preview(styles, path):
    """19:19 of every style side by side, for a check by eye."""
    sheet = Image.new('1', (len(styles) * 420, 420), 0)
    for i, (_, (digits, colon, x, y, width, height), _) in enumerate(styles):
        left = i * 420 + (410 - width) // 2
        top = (420 - height) // 2
        glyphs = [digits[1], digits[9], colon, digits[1], digits[9]]
        for k in range(5):
            if x[k] >= 0 and glyphs[k] is not None:
                sheet.paste(glyphs[k], (left + x[k], top + y[k]))
    sheet.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--preview', help='also write a PNG with 19:19 in every style')
    args = parser.parse_args()

    styles = [
        ('Thin', fontStyle('Thin', 140), False),
        ('Regular', fontStyle('Regular', 120), False),
        ('Dense', segmentStyle(), True),
        # The digits of the sky arcs face (AlwaysOn style 3), under the arcs.
        ('Thin 120', fontStyle('Thin', 120), False),
    ]
    if args.preview:
        preview(styles, args.preview)

    writer = Writer()
    fonts = []
    styleLines = []
    for index, (name, (digits, colon, x, y, width, height), dotted) in enumerate(styles):
        fontName = f'aodDigits{index}'
        # The preview page draws the digits as one-character labels at the layout's places.
        images = digits + [colon]
        advances = [image.size[0] for image in images]
        fonts.append(lvglFont(writer, fontName, list(range(0x30, 0x3B)), images, advances,
                              [0] * 11, digits[0].size[1]))
        glyphs = ',\n        '.join(writer.glyph(image) for image in digits)
        styleLines.append(
            f'    // {name}\n'
            f'    {{ &{fontName},\n'
            f'      {{ {glyphs} }},\n'
            f'      {writer.glyph(colon)},\n'
            f'      {{ {", ".join(str(v) for v in x)} }},\n'
            f'      {{ {", ".join(str(v) for v in y)} }},\n'
            f'      {width}, {height}, {"true" if dotted else "false"} }}')

    characters = sorted({c for name in DAY_NAMES + MONTH_NAMES for c in name} | set('0123456789: '))
    small, _ = renderText(font('Regular', SMALL_SIZE), characters)
    advance = round(font('Regular', SMALL_SIZE).getlength('0'))
    codes = [ord(c) for c in characters]
    small = [Image.new('1', (0, 0)) if c == ' ' else image for c, image in zip(characters, small)]
    dotted = [dottedImage(image) if image.size[0] else image for image in small]
    advances = [advance] * len(characters)
    lineHeight = max(image.size[1] for image in small)
    fonts.append(lvglFont(writer, 'aodSmallFont', codes, small, advances, [0] * len(codes), lineHeight))
    fonts.append(lvglFont(writer, 'aodSmallDottedFont', codes, dotted, advances, [0] * len(codes), lineHeight))
    smallGlyphs = ',\n    '.join(writer.glyph(image) for image in small)
    dottedGlyphs = ',\n    '.join(writer.glyph(image) for image in dotted)

    rows = []
    for i in range(0, len(writer.data), 16):
        rows.append('    ' + ' '.join(f'0x{b:02X},' for b in writer.data[i:i + 16]))
    with open(OUTPUT, 'w', encoding='utf-8') as output:
        output.write('// Generated by tools/make_aod_glyphs.py from JetBrains Mono (tools/fonts/, SIL Open Font\n'
                     '// License); do not edit.\n\n'
                     '#include "AodGlyphs.h"\n\n'
                     f'static const uint8_t s_bits[{len(writer.data)}] = {{\n' + '\n'.join(rows) + '\n};\n\n'
                     + '\n'.join(fonts) + '\n'
                     'const AodStyle aodStyles[AOD_STYLE_COUNT] = {\n' + ',\n'.join(styleLines) + '\n};\n\n'
                     'const uint16_t aodSmallCodes[AOD_SMALL_COUNT] = { '
                     + ', '.join(f'0x{c:X}' for c in codes) + ' };\n\n'
                     'const AodGlyph aodSmall[AOD_SMALL_COUNT] = {\n    ' + smallGlyphs + '\n};\n\n'
                     'const AodGlyph aodSmallDotted[AOD_SMALL_COUNT] = {\n    ' + dottedGlyphs + '\n};\n\n'
                     f'const int aodSmallAdvance = {advance};\n')
    print(f'{OUTPUT}: {len(writer.data)} bytes of bitmaps; AOD_STYLE_COUNT must be {len(styles)}, '
          f'AOD_SMALL_COUNT {len(codes)}')


if __name__ == '__main__':
    main()
