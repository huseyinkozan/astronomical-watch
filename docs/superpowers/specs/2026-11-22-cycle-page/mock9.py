# The day's entry, over the part (as the converter's keypad): the bleeding of the day as four
# keys (none, light, medium, heavy: drops of three sizes), intercourse as a key that toggles, and
# the day's date as the title; the calendar part opens the same for any past day.
import sys
sys.path.insert(0, sys.argv[2])
import mock6 as s6
import mock7 as s7
from mock2 import base, centre, F26, F20, F18, W, H, WHITE, GREY, RED, BLUE
from PIL import Image, ImageDraw
OUT = sys.argv[2]

def key(d, x1, y1, x2, y2, checked):
    d.rounded_rectangle([x1, y1, x2, y2], radius=12, outline=(230, 230, 230) if checked else (90, 90, 90), width=3 if checked else 2,
                        fill=(30, 30, 30) if checked else (0, 0, 0))

def entry(bleed, together, title):
    im, d = base('Döngü')
    d.rounded_rectangle([20, 46, W - 20, 494], radius=18, fill=(10, 10, 10), outline=(80, 80, 80), width=2)
    centre(d, 58, title, F26, WHITE)
    centre(d, 104, 'Kanama', F20, GREY)
    labels = ['yok', 'az', 'orta', 'çok']
    for i in range(4):
        x1 = 44 + i * 82; x2 = x1 + 72
        key(d, x1, 134, x2, 222, i == bleed)
        cx = (x1 + x2) / 2
        if i == 0:
            d.line([(cx - 10, 168), (cx + 10, 168)], fill=GREY, width=3)
        else:
            s7.drop(d, cx, 168, RED, s7.SCALE[i])
        d.text((cx - d.textlength(labels[i], font=F18) / 2, 192), labels[i], font=F18, fill=WHITE)
    centre(d, 246, 'Birliktelik', F20, GREY)
    key(d, 120, 276, 290, 350, together)
    if together:
        s6.sperm_egg(d, 180, 313, BLUE, 0)
    else:
        s6.egg(d, 180, 313, (90, 90, 90))
    d.text((200, 300), 'var' if together else 'yok', font=F26, fill=WHITE)
    d.rounded_rectangle([110, 390, 300, 450], radius=14, outline=(200, 200, 200), width=2)
    centre(d, 402, 'Tamam', F26, WHITE)
    return im

shots = [('Bugün (spiralde bugünün kutusu)', entry(2, False, 'Bugün, 1 Ara')), ('Takvimde geçmiş bir gün', entry(0, True, '29 Kas Pazar'))]
sheet = Image.new('RGB', (2 * (W + 20) + 20, H + 70), (28, 28, 28))
sd = ImageDraw.Draw(sheet)
for i, (name, im) in enumerate(shots):
    x = 20 + i * (W + 20)
    sheet.paste(im, (x, 50))
    sd.text((x, 14), name, font=F20, fill=(230, 230, 230))
sheet.save(OUT + '/entry.png')
