"""PPM frames of the host preview -> PNG, plus off | on pairs and a x3 crop of the car (build/preview)."""
import glob
import os
import sys

from PIL import Image

d = sys.argv[1]
for f in sorted(glob.glob(os.path.join(d, "*.ppm"))):
    Image.open(f).save(f[:-4] + ".png")
n = 0
while os.path.exists(os.path.join(d, "%02d_on.png" % n)):
    on, off = Image.open(os.path.join(d, "%02d_on.png" % n)), Image.open(os.path.join(d, "%02d_off.png" % n))
    s = Image.new("RGB", (1288, 480), (255, 0, 255))
    s.paste(off, (0, 0)); s.paste(on, (648, 0))
    s.save(os.path.join(d, "pair%02d.png" % n))
    box = (170, 150, 470, 380)
    a, b = off.crop(box).resize((900, 690), Image.NEAREST), on.crop(box).resize((900, 690), Image.NEAREST)
    s = Image.new("RGB", (1808, 690), (255, 0, 255))
    s.paste(a, (0, 0)); s.paste(b, (908, 0))
    s.save(os.path.join(d, "crop%02d.png" % n))
    n += 1
print("pairs", n)
