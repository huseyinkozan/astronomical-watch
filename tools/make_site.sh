#!/bin/sh
# Assembles the project's site (site/) into public/: the pages, the watch's screenshots of both
# languages (docs/screenshots) and, when a release's full image is given, the browser installer's
# firmware and manifest (ESP Web Tools). SITE_URL is the address it is served at (the sharing
# preview, robots.txt and sitemap.xml need it whole); GitHub Pages by default.
#     sh tools/make_site.sh [astronomical_watch-<version>-full.bin]
# Try it: python3 -m http.server -d public 8000 (the night-red preview needs http, not file://).
set -e
cd "$(dirname "$0")/.."
site_url=${SITE_URL:-https://huseyinkozan.github.io/astronomical-watch/}
rm -rf public
mkdir -p public/screenshots/en public/screenshots/tr public/firmware
cp -R site/. public/
sed -i "s|@SITE_URL@|$site_url|g" public/index.html
printf 'User-agent: *\nAllow: /\nSitemap: %ssitemap.xml\n' "$site_url" > public/robots.txt
cat > public/sitemap.xml <<XML
<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9"><url><loc>$site_url</loc></url></urlset>
XML
cp docs/screenshots/en/*.png public/screenshots/en/
cp docs/screenshots/tr/*.png public/screenshots/tr/
# How many views each part has (tools/screenshot.py --views: <n>-<name>-2.png and on), for the
# preview's taps.
{
    printf 'const partViews = {'
    for first in docs/screenshots/en/*-2.png; do
        [ -e "$first" ] || continue
        base=$(basename "$first" -2.png)
        # 15-settings-2 and face-2 are pictures of their own, not a second view.
        [ -e docs/screenshots/en/"$base".png ] || continue
        count=$(ls docs/screenshots/en/"$base"-[0-9].png | wc -l)
        printf '"%s": %d, ' "$base" $((count + 1))
    done
    printf '};\n'
} > public/views.js
if [ -n "$1" ]; then
    image=$(basename "$1")
    version=$(echo "$image" | sed -e 's/^astronomical_watch-//' -e 's/-full\.bin$//')
    cp "$1" public/firmware/
    cat > public/firmware/manifest.json <<JSON
{
  "name": "Astronomical Watch",
  "version": "$version",
  "new_install_prompt_erase": true,
  "builds": [
    { "chipFamily": "ESP32-S3", "parts": [ { "path": "$image", "offset": 0 } ] }
  ]
}
JSON
fi
echo "public/ ready"
