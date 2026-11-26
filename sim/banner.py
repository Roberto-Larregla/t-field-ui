#!/usr/bin/env python3
"""Pone la pantalla de inicio del simulador en el aparato de los banners del README.

docs/img/banner-es.jpg y banner-en.jpg traen su ilustración y textos; este script solo
cambia lo que se ve en la pantalla (en perspectiva) y rehace docs/img/social-preview.png
(la imagen para compartir el repositorio, desde el banner en inglés).

Uso, desde la raíz del repositorio:
  ./build/sim_png /tmp/es es claro && ./build/sim_png /tmp/en en claro
  python3 sim/raw2png.py /tmp/es && python3 sim/raw2png.py /tmp/en
  python3 sim/banner.py /tmp/es /tmp/en
"""
import sys

import cv2
import numpy as np
from PIL import Image

# Esquinas de la pantalla en el banner (2048x768): arriba-izq., arriba-der., abajo-der., abajo-izq.
CORNERS = np.float32([[1382.1, 124.8], [1965.0, 191.4], [1895.3, 668.6], [1313.8, 588.5]])
TONE = np.float32([0.965, 0.975, 0.99])  # la pantalla en la foto: un poco menos que blanco puro
SS = 2  # supermuestreo: el texto de la pantalla queda nítido sin escalones


def put_screen(banner_path, screen_png):
    base = np.array(Image.open(banner_path).convert("RGB")).astype(np.float32)
    # 1 px hacia afuera, para tapar el borde de la pantalla anterior
    c = CORNERS.copy()
    mid = c.mean(0)
    c += (c - mid) / np.linalg.norm(c - mid, axis=1, keepdims=True)
    x0, y0 = np.floor(c.min(0)).astype(int) - 2
    x1, y1 = np.ceil(c.max(0)).astype(int) + 2
    src = np.array(Image.open(screen_png).convert("RGB").resize((1280, 960), Image.NEAREST)).astype(np.float32)
    sh, sw = src.shape[:2]
    dst = (c - [x0, y0]) * SS
    m = cv2.getPerspectiveTransform(np.float32([[0, 0], [sw, 0], [sw, sh], [0, sh]]), dst.astype(np.float32))
    size = ((x1 - x0) * SS, (y1 - y0) * SS)
    scr = cv2.warpPerspective(src, m, size, flags=cv2.INTER_LINEAR)
    alpha = cv2.warpPerspective(np.ones((sh, sw), np.float32), m, size, flags=cv2.INTER_LINEAR)
    scr = cv2.resize(scr, (x1 - x0, y1 - y0), interpolation=cv2.INTER_AREA) * TONE
    alpha = cv2.resize(alpha, (x1 - x0, y1 - y0), interpolation=cv2.INTER_AREA)[..., None]
    region = base[y0:y1, x0:x1]
    base[y0:y1, x0:x1] = scr * alpha + region * (1 - alpha)
    out = Image.fromarray(np.clip(base + 0.5, 0, 255).astype(np.uint8))
    out.save(banner_path, quality=93, subsampling=0)
    return out


es_dir, en_dir = sys.argv[1], sys.argv[2]
put_screen("docs/img/banner-es.jpg", f"{es_dir}/inicio.png")
en = put_screen("docs/img/banner-en.jpg", f"{en_dir}/inicio.png")
social = Image.new("RGB", (1280, 640), (241, 245, 253))
social.paste(en.resize((1280, 480), Image.LANCZOS), (0, 80))
social.save("docs/img/social-preview.png", optimize=True)
print("docs/img/banner-es.jpg docs/img/banner-en.jpg docs/img/social-preview.png")
