# Tide mock-ups (2026-11-23), drawn over the watch's own screenshots: A a corner gauge, B a corner
# icon, C a text line, D an information part with the day's curve, E the portal's setting (only
# the interval: the gauge and the icon join the Faces tab's existing corner lists).
# The model (research/tide-gauge.md): high water at the Moon's transit plus the place's lunitidal
# interval, a semidiurnal cosine of 12 h 25 min, its range larger at new and full Moon (spring)
# than at the quarters (neap). The example: the screenshots' scene (21 June 2026, 19:19), a
# lunitidal interval of 5 h 20 min (Casio's manual's example), high water at 18:00.
#     tools/.venv/bin/python docs/superpowers/specs/2026-11-23-tide/mock.py
import math, os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..', '..', '..')
SHOTS = os.path.join(ROOT, 'docs', 'screenshots', 'tr')
FONT = os.path.join(ROOT, 'tools', 'fonts', 'JetBrainsMono-Regular.ttf')
F26 = ImageFont.truetype(FONT, 26)
F20 = ImageFont.truetype(FONT, 20)
F18 = ImageFont.truetype(FONT, 18)
F16 = ImageFont.truetype(FONT, 16)
W, H = 410, 502
WHITE = (255, 255, 255); GREY = (160, 160, 160); DIM = (70, 70, 70)
RISING = (64, 208, 224)    # cyan while the water rises
FALLING = (64, 128, 224)   # blue while it falls
PERIOD_H = 12.42
HIGH = 18.0                # today's high water, hours
NOW = 19 + 19 / 60


def level(t):
    # 0 low, 1 high.
    return (1 + math.cos(2 * math.pi * (t - HIGH) / PERIOD_H)) / 2


def rising(t):
    # The level's slope: -sin, so the water rises where the sine is negative.
    return math.sin(2 * math.pi * (t - HIGH) / PERIOD_H) < 0


def corner_gauge(d, share, colour):
    # The bottom-left corner, as Corners: a quarter circle 3 px inside the edge, 5 px wide, from
    # its lower end, over a channel of a dim tone of its colour; a notch in the middle.
    cx, cy = 120, H - 121
    box = [cx - 117, cy - 117, cx + 117, cy + 117]
    d.arc(box, 90, 180, fill=(0, 0, 0), width=9)
    channel = tuple(int(c * 0.3) for c in colour)
    d.arc(box, 90, 180, fill=channel, width=5)
    d.arc(box, 90, 90 + 90 * share, fill=colour, width=5)
    a = math.radians(135)
    d.line([(cx + 112 * math.cos(a), cy + 112 * math.sin(a)), (cx + 117 * math.cos(a), cy + 117 * math.sin(a))], fill=WHITE, width=2)


def tide_icon(d, cx, cy, share, up):
    # A round window with the water at its level and an arrow: up rising, down falling.
    r = 22
    d.ellipse([cx - r - 6, cy - r - 6, cx + r + 6, cy + r + 6], fill=(0, 0, 0))
    mask = Image.new('L', (2 * r + 1, 2 * r + 1), 0)
    ImageDraw.Draw(mask).ellipse([0, 0, 2 * r, 2 * r], fill=255)
    water = Image.new('RGB', (2 * r + 1, 2 * r + 1), (0, 0, 0))
    wd = ImageDraw.Draw(water)
    top = int(2 * r * (1 - share))
    colour = RISING if up else FALLING
    for x in range(2 * r + 1):
        y = top + 2 * math.sin(x / 5.0)
        wd.line([(x, y), (x, 2 * r)], fill=colour)
    canvas = d._image
    canvas.paste(water, (cx - r, cy - r), mask)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=GREY, width=2)
    tx = cx + r + 2
    if up:
        d.polygon([(tx, cy - 4), (tx + 10, cy - 4), (tx + 5, cy - 12)], fill=WHITE)
    else:
        d.polygon([(tx, cy + 4), (tx + 10, cy + 4), (tx + 5, cy + 12)], fill=WHITE)


def blank_text(d, x0, y0, x1, y1):
    d.rectangle([x0, y0, x1, y1], fill=(0, 0, 0))


def option_a():
    im = Image.open(os.path.join(SHOTS, 'face-0.png')).convert('RGB')
    d = ImageDraw.Draw(im)
    blank_text(d, 40, 405, 100, 460)  # the Moon's phase icon, to show the corner alone
    corner_gauge(d, level(NOW), RISING if rising(NOW) else FALLING)
    return im


def option_b():
    im = Image.open(os.path.join(SHOTS, 'face-0.png')).convert('RGB')
    d = ImageDraw.Draw(im)
    tide_icon(d, 340, 70, level(NOW), rising(NOW))
    return im


def option_c():
    im = Image.open(os.path.join(SHOTS, 'face-1.png')).convert('RGB')
    d = ImageDraw.Draw(im)
    blank_text(d, 92, 46, 380, 82)
    low = HIGH + PERIOD_H / 2
    left = low - NOW
    text = 'Alçak su %d:%02d' % (int(left), round((left % 1) * 60))
    d.text((W / 2 - d.textlength(text, font=F26) / 2 + 30, 48), text, font=F26, fill=(160, 160, 160))
    return im


def option_d():
    im = Image.new('RGB', (W, H), (0, 0, 0))
    d = ImageDraw.Draw(im)
    for (cx, cy, a0, c) in [(120, 120, 180, (255, 208, 64)), (W - 121, 120, 270, (64, 208, 96)),
                            (120, H - 121, 90, (184, 196, 220)), (W - 121, H - 121, 0, (220, 220, 220))]:
        d.arc([cx - 117, cy - 117, cx + 117, cy + 117], a0, a0 + 90, fill=c, width=5)
    title = 'Bilgi'
    d.text((W / 2 - d.textlength(title, font=F26) / 2, 2), title, font=F26, fill=WHITE)
    sub = 'Gelgit'
    d.rounded_rectangle([110, 40, 300, 78], radius=12, outline=(120, 120, 120), width=2)
    d.text((W / 2 - d.textlength(sub, font=F26) / 2, 43), sub, font=F26, fill=WHITE)
    # The day's curve: 0 to 24 h, the level, now marked.
    x0, x1, y0, y1 = 40, 370, 100, 230
    d.line([(x0, y1), (x1, y1)], fill=DIM, width=1)
    for h in range(0, 25, 6):
        x = x0 + (x1 - x0) * h / 24
        d.line([(x, y1), (x, y1 + 5)], fill=GREY)
        d.text((x - 10, y1 + 6), '%02d' % h, font=F16, fill=GREY)
    pts = []
    for i in range(241):
        t = 24 * i / 240
        pts.append((x0 + (x1 - x0) * t / 24, y1 - 8 - (y1 - y0 - 16) * level(t)))
    for i in range(240):
        t = 24 * i / 240
        d.line([pts[i], pts[i + 1]], fill=RISING if rising(t) else FALLING, width=3)
    nx = x0 + (x1 - x0) * NOW / 24
    ny = y1 - 8 - (y1 - y0 - 16) * level(NOW)
    d.line([(nx, y0), (nx, y1)], fill=WHITE, width=1)
    d.ellipse([nx - 6, ny - 6, nx + 6, ny + 6], fill=WHITE)
    rows = [('Yüksek', '05:35  18:00'), ('Alçak', '11:47  00:12'), ('Şimdi', 'alçalıyor %89'),
            ('Ay', 'dördün: zayıf'), ('Gecikme', '5 sa 20 dk')]
    y = 270
    for name, value in rows:
        d.text((44, y), name, font=F26, fill=GREY)
        d.text((W - 44 - d.textlength(value, font=F26), y), value, font=F26, fill=WHITE)
        y += 38
    note = 'yaklaşık: yalnız Ay'
    d.text((W / 2 - d.textlength(note, font=F18) / 2, 466), note, font=F18, fill=GREY)
    return im


def option_e():
    # The portal's setting, in its look (dark page, a labelled field and a help line).
    im = Image.new('RGB', (W, H), (24, 24, 28))
    d = ImageDraw.Draw(im)
    sans = ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Regular.ttf', 17) if os.path.exists('/usr/share/fonts/noto/NotoSans-Regular.ttf') else F18
    bold = ImageFont.truetype('/usr/share/fonts/noto/NotoSans-Bold.ttf', 20) if os.path.exists('/usr/share/fonts/noto/NotoSans-Bold.ttf') else F20
    d.rounded_rectangle([14, 20, W - 14, 430], radius=10, outline=(70, 70, 76), width=1)
    d.text((26, 10), ' Gelgit ', font=bold, fill=(230, 230, 230), stroke_width=0)
    d.text((30, 56), 'Gecikme (Ay geçişinden yüksek suya)', font=sans, fill=(230, 230, 230))
    d.rounded_rectangle([30, 86, 180, 122], radius=6, fill=(40, 40, 46), outline=(90, 90, 96))
    d.text((42, 92), '5 : 20', font=sans, fill=(240, 240, 240))
    d.text((190, 92), 'saat : dakika', font=sans, fill=(150, 150, 156))
    help_lines = ['Limanınızın gelgit tablosundan: bir', 'yüksek su saatinden, ondan önceki Ay',
                  'geçişini çıkarın (saatin Ay sayfası',
                  'gösterir). Boş bırakılırsa gelgit', 'gösterilmez.', '',
                  'Gösterge suyun ne zaman yüksek ya da',
                  'alçak olduğunu gösterir, ne kadar',
                  'olduğunu değil. Akdeniz, Karadeniz,',
                  'Baltık gibi neredeyse kapalı',
                  'denizlerde gelgit birkaç santimetredir.']
    y = 140
    for line in help_lines:
        d.text((30, y), line, font=sans, fill=(170, 170, 176))
        y += 26
    return im


def sheet():
    panels = [('A  Köşe göstergesi', option_a()), ('B  Köşe simgesi', option_b()),
              ('C  Yazı satırı', option_c()), ('D  Bilgi: Gelgit bölümü', option_d()),
              ('E  Portal ayarı', option_e())]
    pad, top = 20, 50
    out = Image.new('RGB', (len(panels) * (W + pad) + pad, H + top + pad), (30, 30, 34))
    d = ImageDraw.Draw(out)
    for i, (label, im) in enumerate(panels):
        x = pad + i * (W + pad)
        d.text((x, 12), label, font=F26, fill=(255, 176, 64))
        out.paste(im, (x, top))
    out.save(os.path.join(HERE, 'mock.png'))
    print(os.path.join(HERE, 'mock.png'))


if __name__ == '__main__':
    sheet()
