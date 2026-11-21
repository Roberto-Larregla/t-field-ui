#!/usr/bin/env python3
"""Convierte capturas RGB565 crudas (320x240) a PNG, a 1x y 3x."""
import glob, struct, sys
from PIL import Image
folder = sys.argv[1] if len(sys.argv) > 1 else "out"
for p in sorted(glob.glob(folder + "/*.raw")):
    w, h = 320, 240
    px = struct.unpack("<%dH" % (w * h), open(p, "rb").read())
    img = Image.new("RGB", (w, h))
    img.putdata([(((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31) for v in px])
    img.save(p[:-4] + ".png")
    img.resize((w * 3, h * 3), Image.NEAREST).save(p[:-4] + "@3x.png")
