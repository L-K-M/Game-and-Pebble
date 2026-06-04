#!/usr/bin/env python3
"""Generate the Game & Pebble launcher icon.

The Pebble launcher renders app icons as a monochrome mask (white shape on a
transparent background, tinted by the system). We draw a chubby "Pebble-Man"
silhouette so the collection is instantly recognisable in the app menu.

Output: resources/images/menu_icon.png  (24x24, within the 25x25 limit)
"""
import os
from PIL import Image, ImageDraw

SIZE = 24
OUT = os.path.join(os.path.dirname(__file__), "..", "resources", "images", "menu_icon.png")

img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

WHITE = (255, 255, 255, 255)
CLEAR = (0, 0, 0, 0)

# Body: a disc with a wedge "mouth" cut out on the right (classic chomper).
# PIL angles: 0 deg = 3 o'clock, increasing clockwise. Leaving 325..360/0..35
# empty produces a ~70 degree mouth opening toward the right.
pad = 1
d.pieslice([pad, pad, SIZE - 1 - pad, SIZE - 1 - pad], start=35, end=325, fill=WHITE)

# Eye: a small transparent dot punched out of the upper body.
eye_cx, eye_cy, eye_r = 12, 7, 2
d.ellipse([eye_cx - eye_r, eye_cy - eye_r, eye_cx + eye_r, eye_cy + eye_r], fill=CLEAR)

# A couple of "dots" being chomped, trailing into the mouth.
for cx in (19, 23):
    d.ellipse([cx - 1, 11, cx + 1, 13], fill=WHITE)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
img.save(OUT)
print("wrote", os.path.normpath(OUT), img.size)
