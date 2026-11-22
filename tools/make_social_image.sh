#!/bin/sh
# The site's picture for shared links (Open Graph, 1200 x 630): three of the watch's English
# screenshots on the site's dark ground, the name above and what each shows below, so that
# someone who has never seen the watch reads the dim one as its always-on face. Needs
# ImageMagick 7 (magick).
#     sh tools/make_social_image.sh
set -e
cd "$(dirname "$0")/.."
shots=docs/screenshots/en
font=tools/fonts/JetBrainsMono-Regular.ttf
# The panel's round corners (120 px of 502, here 96 of 400).
mask=$(mktemp --suffix=.png)
magick -size 327x400 xc:black -fill white -draw 'roundrectangle 0,0 326,399 96,96' "$mask"
magick -size 1200x630 xc:'#0b0b0d' \
    \( "$shots/face-0.png" -resize x400 "$mask" -alpha off -compose CopyOpacity -composite \) \
    -compose over -geometry +95+130 -composite \
    \( "$shots/face-2.png" -resize x400 "$mask" -alpha off -compose CopyOpacity -composite \) \
    -compose over -geometry +437+130 -composite \
    \( "$shots/always-on-0.png" -resize x400 "$mask" -alpha off -compose CopyOpacity -composite \) \
    -compose over -geometry +779+130 -composite \
    -font "$font" -fill '#ffb040' -pointsize 50 -annotate +95+90 'Astronomical Watch' \
    -fill '#e8e8ea' -pointsize 24 -gravity northwest \
    -annotate +95+555 'The watch face' -annotate +437+555 'Another face page' \
    -annotate +779+555 'Always-on face' \
    -fill '#a0a0a6' -pointsize 20 -annotate +779+588 'dim, while it sleeps' \
    site/social.png
rm -f "$mask"
echo "site/social.png ready"
