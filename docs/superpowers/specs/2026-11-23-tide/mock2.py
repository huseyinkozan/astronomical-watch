# The tide icon as the owner described it (2026-11-23): a wave whose crest reaches the icon's top
# at high water; at low water no wave, a flat blue sea line at 20 % of the icon's height with the
# part below it blue. Between them the crest grows with the level. Drawn at five levels, three
# times larger, and once at its size on the watch face.
#     tools/.venv/bin/python docs/superpowers/specs/2026-11-23-tide/mock2.py
import math, os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..', '..', '..')
FONT = os.path.join(ROOT, 'tools', 'fonts', 'JetBrainsMono-Regular.ttf')
F26 = ImageFont.truetype(FONT, 26)
SEA = (64, 128, 224)        # the water
SURFACE = (150, 200, 255)   # its line, lighter
SIZE = 44                   # the icon's box on the face, as the weekday ring's
BASE = 0.2                  # the calm sea's line, a share of the height


def surface(shape, x):
    # The wave's height over the calm line, 0 to 1, at x (0 to 1 across the box).
    if shape == 0:   # one round crest in the middle
        return ((1 + math.cos(2 * math.pi * (x - 0.5))) / 2) ** 1.5
    if shape == 1:   # two crests, as a sine
        return (1 + math.cos(2 * math.pi * (2 * x - 0.5))) / 2
    # A crest leaning right, as a wave running to the shore: a slow rise, a steep front.
    p = 0.68
    return (x / p) ** 2 if x < p else max(0.0, 1 - ((x - p) / (1 - p)) ** 0.6)


def icon(level, scale=1, shape=0):
    s = SIZE * scale
    im = Image.new('RGB', (s, s), (0, 0, 0))
    d = ImageDraw.Draw(im)
    crest = (1 - BASE) * level
    top = []
    for i in range(s + 1):
        height = BASE + crest * surface(shape, i / s)
        top.append((i, (s - 1) * (1 - height)))
    d.polygon(top + [(s, s), (0, s)], fill=SEA)
    d.line(top, fill=SURFACE, width=max(2, scale))
    return im


def sheet():
    levels = [0, 0.25, 0.5, 0.75, 1]
    names = ['B1  Tek tepe', 'B2  Sinüs dalgası', 'B3  Kıyıya vuran dalga']
    big = 3
    pad = 30
    row = SIZE * big + 70
    face = Image.open(os.path.join(ROOT, 'docs', 'screenshots', 'tr', 'face-0.png')).convert('RGB')
    width = pad + 330 + len(levels) * (SIZE * big + pad) + 3 * (SIZE + 20) + pad
    out = Image.new('RGB', (width, 70 + 3 * row), (30, 30, 34))
    d = ImageDraw.Draw(out)
    d.text((pad, 10), 'Gelgit simgesi: alçak, %25, %50, %75, yüksek; sağda gerçek boyutta (%89)', font=F26,
           fill=(255, 176, 64))
    for shape in range(3):
        y = 60 + shape * row
        d.text((pad, y + SIZE * big / 2 - 14), names[shape], font=F26, fill=(230, 230, 230))
        for i, level in enumerate(levels):
            x = pad + 330 + i * (SIZE * big + pad)
            out.paste(icon(level, big, shape), (x, y))
            label = 'alçak' if level == 0 else ('yüksek' if level == 1 else '%%%d' % round(level * 100))
            d.text((x + (SIZE * big - d.textlength(label, font=F26)) / 2, y + SIZE * big + 6), label, font=F26,
                   fill=(160, 160, 160))
        # At its size, over the face's own top-right corner.
        crop = face.crop((300, 30, 390, 110)).copy()
        cd = ImageDraw.Draw(crop)
        cd.rectangle([18, 14, 70, 66], fill=(0, 0, 0))
        crop.paste(icon(0.89, 1, shape), (44 - SIZE // 2, 40 - SIZE // 2))
        crop = crop.resize((crop.width * 2, crop.height * 2), Image.NEAREST)
        out.paste(crop, (width - pad - crop.width, y))
    out.save(os.path.join(HERE, 'mock2.png'))
    print(os.path.join(HERE, 'mock2.png'))


if __name__ == '__main__':
    sheet()
