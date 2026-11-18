# Mock-ups of the configurable face templates (active and always-on) for the design talk.
import math, sys
from PIL import Image, ImageDraw, ImageFont, ImageChops

D = 'docs/screenshots/tr/'
FONT = 'tools/fonts/JetBrainsMono-Regular.ttf'
LIGHT = 'tools/fonts/JetBrainsMono-Light.ttf'
WHITE, GREY, DIM, AMBER = (255, 255, 255), (160, 160, 160), (70, 70, 74), (255, 176, 64)
MOONWHITE, SUN = (184, 196, 220), (255, 190, 60)
SEG = [0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F]

def font(size, light=False):
    return ImageFont.truetype(LIGHT if light else FONT, size)

def base(name):
    """The corner gauges of a published screenshot on black."""
    src = Image.open(D + name).convert('RGB')
    mask = Image.new('L', src.size, 255)
    ImageDraw.Draw(mask).rounded_rectangle((14, 14, 396, 488), radius=108, fill=0)
    out = Image.new('RGB', src.size, (0, 0, 0))
    out.paste(src, (0, 0), mask)
    return out

MOON = Image.open(D + 'face-0.png').convert('RGB').crop((52, 418, 98, 464))

def text(img, xy, s, size, colour, anchor='mm', light=False):
    ImageDraw.Draw(img).text(xy, s, font=font(size, light), fill=colour, anchor=anchor)

def hexagon(x0, y0, x1, y1):
    if x1 - x0 >= y1 - y0:
        h = (y1 - y0) / 2; m = (y0 + y1) / 2
        return [(x0, m), (x0 + h, y0), (x1 - h, y0), (x1, m), (x1 - h, y1), (x0 + h, y1)]
    h = (x1 - x0) / 2; m = (x0 + x1) / 2
    return [(m, y0), (x1, y0 + h), (x1, y1 - h), (m, y1), (x0, y1 - h), (x0, y0 + h)]

def sevenSegment(img, s, cx, cy, height, colour, aod=False):
    w, h = height * 0.62, height
    t, g = height / 11, height / 40
    gap, around, colon = w * 0.16, w * 0.18, t
    total = 4 * w + 2 * gap + 2 * around + colon
    x = cx - total / 2; y = cy - h / 2
    xs = [x, x + w + gap, x + 2 * w + gap + 2 * around + colon, x + 3 * w + 2 * gap + 2 * around + colon]
    S = 4
    layer = Image.new('L', (img.width * S, img.height * S), 0); dr = ImageDraw.Draw(layer)
    m = h / 2
    boxes = [(t/2+g, 0, w-t/2-g, t), (w-t, t/2+g, w, m-g), (w-t, m+g, w, h-t/2-g), (t/2+g, h-t, w-t/2-g, h),
             (0, m+g, t, h-t/2-g), (0, t/2+g, t, m-g), (t/2+g, m-t/2, w-t/2-g, m+t/2)]
    for X, d in zip(xs, [int(c) for c in s if c.isdigit()]):
        for i, b in enumerate(boxes):
            if SEG[d] & (1 << i):
                dr.polygon([((X + px) * S, (y + py) * S) for px, py in hexagon(*b)], fill=255)
    cxc = x + 2 * w + gap + around + colon / 2
    for yy in (y + h * 0.3, y + h * 0.7):
        dr.rectangle(((cxc - colon / 2) * S, (yy - colon / 2) * S, (cxc + colon / 2) * S, (yy + colon / 2) * S), fill=255)
    mask = layer.resize(img.size, Image.NEAREST if aod else Image.LANCZOS)
    if aod:
        mask = mask.point(lambda v: 255 if v > 127 else 0)
        dots = Image.new('L', img.size, 0); pd = dots.load()
        for yy in range(0, img.height, 2):
            for xx in range(0, img.width, 2):
                pd[xx, yy] = 255
        mask = ImageChops.multiply(mask, dots)
    img.paste(Image.new('RGB', img.size, colour), (0, 0), mask)

def digital(img, style, cx, cy, height, colour, aod=False):
    if style == 'seg':
        sevenSegment(img, '19:19', cx, cy, height, colour, aod)
    else:
        text(img, (cx, cy), '19:19', int(height * 1.25), colour, light=aod)

def analog(img, style, cx, cy, r, colour, aod=False):
    dr = ImageDraw.Draw(img)
    for i in range(0 if style == 'plain' else (60 if style in ('thin', 'one') else 12)):
        a = math.radians(i * (6 if style in ('thin', 'one') else 30))
        major = style in ('thin', 'one') and i % 5 == 0 or style not in ('thin', 'one')
        inner = r * (0.86 if major else 0.93)
        dr.line((cx + inner * math.sin(a), cy - inner * math.cos(a), cx + r * math.sin(a), cy - r * math.cos(a)),
                fill=colour if major else GREY, width=3 if major else 1)
    hour, minute = 19 + 19 / 60, 19
    widths = {'classic': (6, 4), 'bold': (12, 8), 'thin': (5, 3), 'one': (6, 0), 'plain': (7, 4)}[style]
    hand = lambda frac, length, width, col: dr.line(
        (cx, cy, cx + length * math.sin(2 * math.pi * frac), cy - length * math.cos(2 * math.pi * frac)),
        fill=col, width=width)
    if style == 'one':
        # One hand: the hour hand reads the minutes on the fine scale.
        hand(hour / 12, r * 0.92, widths[0], AMBER if not aod else colour)
    else:
        hand(hour / 12, r * 0.55, widths[0], colour)
        hand(minute / 60, r * 0.85, widths[1], colour)
    dr.ellipse((cx - 6, cy - 6, cx + 6, cy + 6), fill=colour)

def weekday(img, cx, cy, day, letter, r=22, aod=False):
    """Day of the week: its first letter in a ring of seven notches, the first `day` of them lit."""
    dr = ImageDraw.Draw(img)
    for i in range(7):
        start = -90 + i * (360 / 7) + 4
        end = start + 360 / 7 - 8
        lit = i < day
        col = (WHITE if aod else AMBER) if lit else (DIM if not aod else (0, 0, 0))
        if aod and not lit:
            continue
        dr.arc((cx - r, cy - r, cx + r, cy + r), start, end, fill=col, width=4)
    text(img, (cx, cy + 1), letter, 24, WHITE)

def halfArcs(img, cx, base_y, aod=False, milky=True):
    dr = ImageDraw.Draw(img)
    def dotted(radius, colour, a0=180, a1=360, step=6):
        for a in range(a0, a1 + 1, step):
            x = cx + radius * math.cos(math.radians(a)); y = base_y + radius * math.sin(math.radians(a))
            dr.ellipse((x - 1.5, y - 1.5, x + 1.5, y + 1.5), fill=colour)
    if milky:
        # The Milky Way: a large circle through the observer (the base's centre), turned by the
        # galactic plane's tilt, drawn as a faint band inside the half disc.
        band = Image.new('L', img.size, 0); bd = ImageDraw.Draw(band)
        R, tilt = 260, math.radians(-60)
        ox, oy = cx + R * math.cos(tilt), base_y + R * math.sin(tilt)
        bd.ellipse((ox - R, oy - R, ox + R, oy + R), outline=255, width=16)
        clip = Image.new('L', img.size, 0)
        ImageDraw.Draw(clip).pieslice((cx - 160, base_y - 160, cx + 160, base_y + 160), 180, 360, fill=255)
        band = ImageChops.multiply(band, clip)
        if aod:
            dots = Image.new('L', img.size, 0); pd = dots.load()
            for yy in range(0, img.height, 2):
                for xx in range(0, img.width, 2):
                    pd[xx, yy] = 255
            band = ImageChops.multiply(band, dots)
        else:
            band = band.point(lambda v: v * 0.45)
        img.paste(Image.new('RGB', img.size, (150, 170, 255) if not aod else (0, 255, 255)), (0, 0), band)
    dotted(150, SUN if not aod else (255, 255, 0))
    dotted(105, MOONWHITE if not aod else WHITE)
    dr.line((cx - 165, base_y, cx + 165, base_y), fill=GREY if not aod else WHITE, width=1)
    def body(radius, frac, colour, size):
        a = math.radians(180 + 180 * frac)
        x = cx + radius * math.cos(a); y = base_y + radius * math.sin(a)
        dr.ellipse((x - size, y - size, x + size, y + size), fill=colour)
    body(150, 0.91, SUN, 8)
    img.paste(MOON.resize((26, 26)), (int(cx - 13), int(base_y - 105 - 13)))

def fullArcs(img, cx, cy, aod=False):
    dr = ImageDraw.Draw(img)
    for radius, colour in ((150, SUN), (122, MOONWHITE)):
        for a in range(0, 360, 5):
            x = cx + radius * math.cos(math.radians(a)); y = cy + radius * math.sin(math.radians(a))
            dr.ellipse((x - 1.5, y - 1.5, x + 1.5, y + 1.5), fill=colour if not aod else WHITE)
    # Sun on the outer ring, the Moon on the inner one (positions by time of day).
    a = math.radians(-90 + 360 * (19.3 / 24)); dr.ellipse((cx + 150 * math.cos(a) - 9, cy + 150 * math.sin(a) - 9,
                                                         cx + 150 * math.cos(a) + 9, cy + 150 * math.sin(a) + 9), fill=SUN)
    a = math.radians(-90 + 360 * 0.55); img.paste(MOON.resize((24, 24)), (int(cx + 122 * math.cos(a) - 12),
                                                                          int(cy + 122 * math.sin(a) - 12)))

def slots(img, texts, corners, aod=False):
    """Slots: texts at the top edge, inside it, inside the bottom edge and at the bottom edge (between
    the corner gauges), and four inner corners ('moon', 'week', 'sun..', or a short text)."""
    for (y, colour), t in zip(((25, WHITE), (60, GREY), (442, GREY), (478, WHITE)), texts):
        if t:
            text(img, (205, y), t, 26, colour if not aod else (170, 170, 170), light=aod)
    places = [(66, 70), (344, 70), (66, 432), (344, 432)]
    for (x, y), c in zip(places, corners):
        if c is None:
            continue
        if c == 'moon':
            img.paste(MOON, (x - 23, y - 23))
        elif c.startswith('sun'):
            # A small sun (the font has no symbol for it) and the altitude.
            dr = ImageDraw.Draw(img); sx = x - 34
            dr.ellipse((sx - 6, y - 6, sx + 6, y + 6), fill=SUN if not aod else (255, 255, 0))
            for k in range(8):
                a = math.radians(k * 45)
                dr.line((sx + 9 * math.cos(a), y + 9 * math.sin(a), sx + 13 * math.cos(a), y + 13 * math.sin(a)),
                        fill=SUN if not aod else (255, 255, 0), width=2)
            text(img, (x + 10, y), c[3:], 26, WHITE if not aod else (200, 200, 200), light=aod)
        elif c == 'week':
            weekday(img, x, y, 7, 'P', aod=aod)
        else:
            text(img, (x, y), c, 26, WHITE if not aod else (200, 200, 200), light=aod)

def face(template, aod):
    img = base('always-on-0.png' if aod else 'face-0.png')
    # Components marked "also on always-on": the time, the corners; the dates only on the active face.
    texts = ['21 Haziran', None if aod else 'Akşam 1:28', None, None if aod else 'Ramazan 232']
    corners = ['week', 'sun13°' if not aod else None, 'moon', '21']
    colour = WHITE if not aod else (220, 220, 220)
    if template == 1:
        fullArcs(img, 205, 251, aod)
        digital(img, 'seg', 205, 251, 64, colour, aod)
        corners[1] = None
    elif template == 2:
        halfArcs(img, 205, 232, aod)
        digital(img, 'font', 205, 310, 70, colour, aod)
    elif template == 3:
        digital(img, 'seg', 205, 251, 122, colour, aod)
    else:
        analog(img, 'classic', 205, 251, 140, colour, aod)
    slots(img, texts, corners, aod)
    return img

titles = ['1 · Tam yay, ortada dijital (7 segment)', '2 · Yarım yay + Samanyolu, alt dijital',
          '3 · Büyük dijital (7 segment)', '4 · Büyük analog (klasik)']
label = ImageFont.truetype('/usr/share/fonts/TTF/DejaVuSans.ttf', 17)
sheet = Image.new('RGB', (4 * 430 + 10, 2 * (502 + 40) + 20), (40, 40, 44)); s = ImageDraw.Draw(sheet)
for i in range(4):
    for row, aod in enumerate((False, True)):
        y = 30 + row * (502 + 40)
        sheet.paste(face(i + 1, aod), (10 + i * 430, y))
        s.text((10 + i * 430, y - 24), titles[i] + (' · bekleme' if aod else ' · ön yüz'), fill=(230, 230, 230), font=label)
sheet.save(sys.argv[1] + '/sablonlar.png')

# Details: the weekday icon for the seven days, analog styles, digital styles.
detail = Image.new('RGB', (1800, 360), (0, 0, 0)); d = ImageDraw.Draw(detail)
days = 'PSÇPCCP'
names = ['Pzt', 'Sal', 'Çar', 'Per', 'Cum', 'Cmt', 'Paz']
for i in range(7):
    weekday(detail, 60 + i * 110, 70, i + 1, days[i], r=34)
    d.text((60 + i * 110, 125), names[i], font=label, fill=(200, 200, 200), anchor='mm')
for i, style in enumerate(('classic', 'bold', 'thin', 'one', 'plain')):
    analog(detail, style, 860 + i * 200, 160, 85, WHITE)
    d.text((860 + i * 200, 280), ['Klasik', 'Kalın', 'İnce bölmeli', 'Tek ibre', 'Çizgisiz'][i], font=label, fill=(200, 200, 200), anchor='mm')
sevenSegment(detail, '19:19', 200, 260, 90, WHITE)
text(detail, (560, 260), '19:19', 96, WHITE)
d.text((200, 330), 'Yedi segment', font=label, fill=(200, 200, 200), anchor='mm')
d.text((560, 330), 'JetBrains Mono', font=label, fill=(200, 200, 200), anchor='mm')
detail.save(sys.argv[1] + '/ayrintilar.png')
print('ok')
