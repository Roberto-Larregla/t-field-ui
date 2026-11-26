#!/usr/bin/env python3
"""Arma la imagen de pantallas del README (docs/img/pantallas.png y screens-en.png).

Uso, desde la raíz del repositorio:
  ./build/sim_png /tmp/es es claro && ./build/sim_png /tmp/eso es oscuro
  python3 sim/raw2png.py /tmp/es && python3 sim/raw2png.py /tmp/eso
  python3 sim/composite.py /tmp/es /tmp/eso docs/img/pantallas.png es
(y lo mismo con "en" para docs/img/screens-en.png)
"""
import os, sys
from PIL import Image, ImageDraw, ImageFont

claro, oscuro, out = sys.argv[1], sys.argv[2], sys.argv[3]
lang = sys.argv[4] if len(sys.argv) > 4 else "es"
BG, LINE, INK, INK2 = (238, 243, 246), (201, 211, 218), (16, 33, 43), (74, 90, 102)
FONTS = os.path.join(os.path.dirname(__file__), "..", "fonts", "src")
BOLD = ImageFont.truetype(os.path.join(FONTS, "AtkinsonHyperlegible-Bold.ttf"), 26)
REG = ImageFont.truetype(os.path.join(FONTS, "AtkinsonHyperlegible-Regular.ttf"), 22)

# (captura, carpeta, x, y): pantallas a 2x (640x480) con marco de 2 px y la leyenda debajo
SLOTS = [("inicio", claro, 40, 40), ("mapa", oscuro, 720, 40),
         ("equipo_ficha", claro, 40, 624), ("mensajes", oscuro, 720, 624),
         ("ajustes_modo", claro, 40, 1208), ("sos", claro, 720, 1208)]
CAPTIONS = {
    "es": [("Inicio", "hora, luz del día, equipo y chats"), ("Mapa", "modo oscuro, mapa base atenuado"),
           ("Ficha del nodo", "distancia, batería, señal"), ("Mensajes", "modo oscuro"),
           ("Ajustes", "modos de un toque y configuración"), ("SOS", "canal y directo, con confirmación")],
    "en": [("Home", "time, daylight, team and chats"), ("Map", "dark mode, dimmed base map"),
           ("Node card", "distance, battery, signal"), ("Messages", "dark mode"),
           ("Settings", "one-tap modes and all settings"), ("SOS", "channel and direct, with confirmation")],
}[lang]

img = Image.new("RGB", (1400, 1792), BG)
d = ImageDraw.Draw(img)
for (name, folder, x, y), (title, detail) in zip(SLOTS, CAPTIONS):
    d.rectangle([x - 2, y - 2, x + 641, y + 481], fill=LINE)
    img.paste(Image.open(f"{folder}/{name}.png").convert("RGB").resize((640, 480), Image.NEAREST), (x, y))
    d.text((x, y + 516), title, font=BOLD, fill=INK, anchor="ls")
    d.text((x + d.textlength(title, font=BOLD) + 12, y + 516), detail, font=REG, fill=INK2, anchor="ls")
img.save(out, optimize=True)
print(out)
