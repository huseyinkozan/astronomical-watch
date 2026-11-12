#!/bin/sh
# Generates the LVGL fonts in main/fonts/ from JetBrains Mono (tools/fonts/, SIL Open Font
# License): ASCII, the degree sign and the Turkish letters. A monospaced font, so that numbers
# line up in columns. Needs Node.js: npx fetches lv_font_conv.
set -e
cd "$(dirname "$0")/.."
font=tools/fonts/JetBrainsMono-Regular.ttf
range=0x20-0x7E,0xB0,0xC7,0xD6,0xDC,0xE7,0xF6,0xFC,0x11E-0x11F,0x130-0x131,0x15E-0x15F
mkdir -p main/fonts
for size in 26 48; do
    npx -y lv_font_conv@1.5.3 --font "$font" --range "$range" --size "$size" --bpp 4 --no-compress \
        --format lvgl --lv-include lvgl.h --lv-font-name "jetbrains_mono_$size" \
        -o "main/fonts/jetbrains_mono_$size.c"
done
