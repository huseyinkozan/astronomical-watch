#!/usr/bin/env python3
"""Generates main/AodGlyphs.c: the 1-bit bitmaps of the always-on face.

The always-on wakes draw without LVGL or fonts (main/AlwaysOn.cpp), and the panel's idle mode
shows each colour channel on or off, so every glyph is stored with one bit per pixel. Styles:

    Thin, Light, Regular   JetBrains Mono
    Outlines               seven segments, each a hexagon drawn as three nested dotted outlines
                           (outer: 1-pixel gaps; 2 pixels inside: 2-pixel gaps; 2 more: 3-pixel gaps)
    Dense, Sparse          seven segments tiled with dots 1 or 2 pixels apart
    Mixed                  seven segments: a dotted outline, inside tiled 2 pixels apart

each as HH:MM on one line and as HH over MM, plus the small characters of the date and the
low-battery mark (solid, and dotted for the seven-segment styles). The same bitmaps also form
LVGL fonts (1 bpp), with which the always-on page of active mode shows the styles. The moon is
drawn on the watch, since it changes with the phase.

    tools/.venv/bin/python tools/make_aod_glyphs.py    (Pillow, see tools/setup_reference.sh)
"""

import argparse
import math
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
FONTS = os.path.join(ROOT, 'tools', 'fonts')
OUTPUT = os.path.join(ROOT, 'main', 'AodGlyphs.c')

# Day names on the face (English now, Turkish for M11): their letters and the digits.
DAY_NAMES = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun', 'Pzt', 'Sal', 'Çar', 'Per', 'Cum', 'Cmt', 'Paz']
SMALL_SIZE = 36

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


def fontStyle(weight, size, stacked):
    fnt = font(weight, size)
    digits, box = renderText(fnt, '0123456789')
    colonImage = Image.new('L', (size * 2, size * 2), 0)
    ImageDraw.Draw(colonImage).text((size // 2, size * 3 // 2), ':', font=fnt, fill=255, anchor='ls')
    colonImage = threshold(colonImage)
    left, _, right, _ = colonImage.getbbox()
    colon = colonImage.crop((left, box[1], right, box[3]))
    step = round(fnt.getlength('0'))
    width, height = digits[0].size
    if stacked:
        rowGap = round(size * 0.12)
        x = [0, step, -1, 0, step]
        y = [0, 0, -1, height + rowGap, height + rowGap]
        return digits, None, x, y, step + width, 2 * height + rowGap
    # A narrow colon: the monospaced one would take a whole digit cell.
    space = round(size * 0.08)
    colonX = step + width + space
    minutesX = colonX + colon.size[0] + space
    x = [0, step, colonX, minutesX, minutesX + step]
    y = [0, 0, 0, 0, 0]
    return digits, colon, x, y, minutesX + step + width, height


# --- Seven segments --------------------------------------------------------------------------

DIGIT_W, DIGIT_H, STROKE, JOINT_GAP = 76, 170, 20, 4


def hexagon(x0, y0, x1, y1, inset):
    """A segment with pointed ends along its long axis, `inset` pixels inside its box."""
    if x1 - x0 >= y1 - y0:
        half = (y1 - y0) / 2
        middle = (y0 + y1) / 2
        return [(x0 + inset * 1.4, middle), (x0 + half, y0 + inset), (x1 - half, y0 + inset),
                (x1 - inset * 1.4, middle), (x1 - half, y1 - inset), (x0 + half, y1 - inset)]
    half = (x1 - x0) / 2
    middle = (x0 + x1) / 2
    return [(middle, y0 + inset * 1.4), (x1 - inset, y0 + half), (x1 - inset, y1 - half),
            (middle, y1 - inset * 1.4), (x0 + inset, y1 - half), (x0 + inset, y0 + half)]


def dottedOutline(pixels, polygon, gap):
    """Walks the outline in 1-pixel steps and lights every (gap + 1)th pixel."""
    step = gap + 1
    carry = 0.0
    for i in range(len(polygon)):
        (ax, ay), (bx, by) = polygon[i], polygon[(i + 1) % len(polygon)]
        length = math.hypot(bx - ax, by - ay)
        t = carry
        while t < length:
            pixels.add((round(ax + (bx - ax) * t / length), round(ay + (by - ay) * t / length)))
            t += step
        carry = t - length


def tiledFill(pixels, polygon, gap, size):
    """The inside of the polygon, on a grid of dots `gap` pixels apart (glyph coordinates)."""
    mask = Image.new('1', size, 0)
    ImageDraw.Draw(mask).polygon(polygon, fill=1)
    period = gap + 1
    for y in range(0, size[1], period):
        for x in range(0, size[0], period):
            if mask.getpixel((x, y)):
                pixels.add((x, y))


def drawShape(pixels, polygon, inner, kind, size):
    if kind == 'outlines':
        dottedOutline(pixels, polygon, 1)
        dottedOutline(pixels, inner(2), 2)
        dottedOutline(pixels, inner(4), 3)
    elif kind == 'tiled1':
        tiledFill(pixels, polygon, 1, size)
    elif kind == 'tiled2':
        tiledFill(pixels, polygon, 2, size)
    else:
        dottedOutline(pixels, polygon, 1)
        tiledFill(pixels, inner(2), 2, size)


def toImage(pixels, size):
    image = Image.new('1', size, 0)
    for x, y in pixels:
        if 0 <= x < size[0] and 0 <= y < size[1]:
            image.putpixel((x, y), 1)
    return image


def segmentDigit(digit, kind):
    w, h, t, g = DIGIT_W, DIGIT_H, STROKE, JOINT_GAP
    m = h // 2
    boxes = [(t // 2 + g, 0, w - t // 2 - g, t), (w - t, t // 2 + g, w, m - g), (w - t, m + g, w, h - t // 2 - g),
             (t // 2 + g, h - t, w - t // 2 - g, h), (0, m + g, t, h - t // 2 - g), (0, t // 2 + g, t, m - g),
             (t // 2 + g, m - t // 2, w - t // 2 - g, m + t // 2)]
    size = (w + 1, h + 1)
    pixels = set()
    for i in range(7):
        if SEGMENTS[digit] & (1 << i):
            box = boxes[i]
            drawShape(pixels, hexagon(*box, 0), lambda inset, box=box: hexagon(*box, inset), kind, size)
    return toImage(pixels, size)


def segmentColon(kind):
    # Two squares of 14 pixels, placed like the colon of the font styles.
    w, h = 15, DIGIT_H + 1
    pixels = set()
    for top in (DIGIT_H * 3 // 10 - 7, DIGIT_H - DIGIT_H * 3 // 10 - 7):
        def square(inset, top=top):
            return [(inset, top + inset), (14 - inset, top + inset), (14 - inset, top + 14 - inset),
                    (inset, top + 14 - inset)]
        drawShape(pixels, square(0), square, kind, (w, h))
    return toImage(pixels, (w, h))


def segmentStyle(kind):
    digits = [segmentDigit(d, kind) for d in range(10)]
    colon = segmentColon(kind)
    # Positions on multiples of 6, so that the 2- and 3-pixel dot grids of all glyphs line up;
    # the colon in the middle between the hours and the minutes.
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


def battery():
    image = Image.new('L', (46, 22), 0)
    draw = ImageDraw.Draw(image)
    draw.rectangle([0, 0, 40, 20], outline=255, width=3)
    draw.rectangle([41, 6, 45, 14], fill=255)
    draw.rectangle([5, 5, 11, 15], fill=255)
    return threshold(image)


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


def stackedLayout(digits, rowGap):
    width, height = digits[0].size
    step = width + rowGap // 2
    return digits, None, [0, step, -1, 0, step], [0, 0, -1, height + rowGap, height + rowGap], step + width, \
        2 * height + rowGap


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--preview', help='also write a PNG with 19:19 in every style')
    args = parser.parse_args()

    # Digit sets, each in two layouts: HH:MM on one line, and HH over MM.
    sets = [
        ('Thin', fontStyle('Thin', 140, False), False),
        ('Light', fontStyle('Light', 140, False), False),
        ('Regular', fontStyle('Regular', 120, False), False),
        ('Outlines', segmentStyle('outlines'), True),
        ('Dense', segmentStyle('tiled1'), True),
        ('Sparse', segmentStyle('tiled2'), True),
        ('Mixed', segmentStyle('outlineTiled'), True),
    ]
    styles = []
    for name, layout, dotted in sets:
        styles.append((name, layout, dotted))
    for name, layout, dotted in sets:
        digits = layout[0]
        rowGap = 20 if dotted else round(digits[0].size[1] * 0.2)
        styles.append((name + ', stacked', stackedLayout(digits, rowGap), dotted))
    if args.preview:
        preview(styles, args.preview)

    writer = Writer()
    fonts = []
    styleLines = []
    for index, (name, (digits, colon, x, y, width, height), dotted) in enumerate(styles):
        fontName = f'aodDigits{index % len(sets)}'
        if index < len(sets):
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

    characters = sorted({c for name in DAY_NAMES for c in name} | set('0123456789 '))
    small, _ = renderText(font('Regular', SMALL_SIZE), characters)
    advance = round(font('Regular', SMALL_SIZE).getlength('0'))
    codes = [ord(c) for c in characters] + [0xE000]
    small = [Image.new('1', (0, 0)) if c == ' ' else image for c, image in zip(characters, small)] + [battery()]
    dotted = [dottedImage(image) if image.size[0] else image for image in small]
    advances = [advance] * len(characters) + [small[-1].size[0]]
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
