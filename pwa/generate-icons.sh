#!/bin/bash

# Icon generation script for Argus Monitor PWA
# Requires ImageMagick to be installed: apt-get install imagemagick

# Create a simple shield icon using ImageMagick
# You can replace this with your own icon file

ICON_DIR="icons"
mkdir -p "$ICON_DIR"

# Create base icon using ImageMagick
# This creates a simple shield design - replace with your own design
#convert -size 512x512 xc:none \
#  -fill "#16213e" \
#  -draw "path 'M 256 50 L 450 150 L 450 300 C 450 400 350 462 256 462 C 162 462 62 400 62 300 L 62 150 Z'" \
#  -fill "#0f3460" \
#  -draw "path 'M 256 100 L 400 175 L 400 300 C 400 375 320 425 256 425 C 192 425 112 375 112 300 L 112 175 Z'" \
#  -fill "#ffffff" \
#  -font "DejaVu-Sans-Bold" -pointsize 180 \
#  -gravity center \
#  -annotate +0+10 "A" \
#  base-icon.png

# Generate all required sizes
convert base-icon.png -resize 72x72 "$ICON_DIR/icon-72x72.png"
convert base-icon.png -resize 96x96 "$ICON_DIR/icon-96x96.png"
convert base-icon.png -resize 128x128 "$ICON_DIR/icon-128x128.png"
convert base-icon.png -resize 144x144 "$ICON_DIR/icon-144x144.png"
convert base-icon.png -resize 152x152 "$ICON_DIR/icon-152x152.png"
convert base-icon.png -resize 192x192 "$ICON_DIR/icon-192x192.png"
convert base-icon.png -resize 384x384 "$ICON_DIR/icon-384x384.png"
convert base-icon.png -resize 512x512 "$ICON_DIR/icon-512x512.png"

# Clean up
rm base-icon.png

echo "Icons generated successfully in $ICON_DIR/"
echo ""
echo "Generated sizes:"
ls -lh "$ICON_DIR"
