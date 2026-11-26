#!/usr/bin/env python3
"""Genera fonts/tfield_pictos.c: pictogramas del inicio como imágenes A8 (solo alfa).

LVGL dibuja una imagen A8 con el color de `image_recolor`, así que un mismo pictograma
sirve en modo claro y oscuro. Cada uno pesa ancho × alto bytes (14 px: 196 B; 28 px: 784 B).

Origen: Tabler Icons (MIT, https://tabler.io/icons), versión outline.
Uso:  npm pack @tabler/icons@3 && tar xzf tabler-icons-*.tgz
      python3 fonts/pictos.py package/icons/outline > fonts/tfield_pictos.c
Necesita rsvg-convert (librsvg) y Pillow.
"""
import io
import re
import subprocess
import sys

from PIL import Image

# nombre en C, icono de Tabler, tamaño en px, grosor del trazo (sobre 24 unidades)
PICTOS = [
    ("tfield_picto_mail", "mail", 14, 2.4),
    ("tfield_picto_users", "users", 14, 2.4),
    ("tfield_picto_map", "map-2", 28, 2.0),
    ("tfield_picto_settings", "settings", 16, 2.3),
    ("tfield_picto_pencil", "pencil", 14, 2.4),
    ("tfield_picto_layers", "stack-2", 17, 2.2),
    ("tfield_picto_ruler", "ruler-2", 18, 2.2),
    ("tfield_picto_eye", "eye", 14, 2.3),
    ("tfield_picto_tag", "tag", 14, 2.3),
    # Equipo: rumbo (se gira) y botones de la ficha
    ("tfield_picto_arrow", "arrow-up", 20, 2.6),
    ("tfield_picto_arrow_big", "arrow-up", 30, 2.4),
    ("tfield_picto_mail16", "mail", 16, 2.2),
    ("tfield_picto_map16", "map-2", 16, 2.2),
    ("tfield_picto_locate", "current-location", 16, 2.2),
    ("tfield_picto_route", "route", 16, 2.2),
    ("tfield_picto_user_plus", "user-plus", 16, 2.2),
    ("tfield_picto_user_minus", "user-minus", 16, 2.2),
    # Ajustes: secciones y modos
    ("tfield_picto_sun", "sun", 14, 2.3),
    ("tfield_picto_locate14", "current-location", 14, 2.3),
    ("tfield_picto_map14", "map-2", 14, 2.3),
    ("tfield_picto_lock", "lock", 14, 2.3),
    ("tfield_picto_sos", "alert-triangle", 14, 2.3),
    ("tfield_picto_badge", "id-badge-2", 14, 2.3),
    ("tfield_picto_antenna", "antenna", 14, 2.3),
    ("tfield_picto_walk", "walk", 18, 2.3),
    ("tfield_picto_battery", "battery-2", 18, 2.3),
    ("tfield_picto_moon", "moon", 18, 2.3),
    ("tfield_picto_tent", "tent", 18, 2.3),
]


def render(svg_path, size, stroke):
    svg = open(svg_path, encoding="utf-8").read()
    svg = re.sub(r'stroke-width="[^"]*"', f'stroke-width="{stroke}"', svg)
    svg = svg.replace("currentColor", "#000")
    png = subprocess.run(["rsvg-convert", "-w", str(size), "-h", str(size)], input=svg.encode(),
                         capture_output=True, check=True).stdout
    img = Image.open(io.BytesIO(png)).convert("RGBA")
    return img.getchannel("A").tobytes()


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "package/icons/outline"
    out = [
        "// T-Field UI — pictogramas del inicio (A8: solo alfa; el color lo pone image_recolor).",
        "// Generado con fonts/pictos.py a partir de Tabler Icons (MIT, https://tabler.io/icons).",
        "#ifdef LV_LVGL_H_INCLUDE_SIMPLE",
        '#include "lvgl.h"',
        "#else",
        '#include "lvgl.h"',
        "#endif",
        "",
    ]
    for cname, icon, size, stroke in PICTOS:
        data = render(f"{src}/{icon}.svg", size, stroke)
        out.append(f"// {icon} · {size} px")
        out.append(f"static const uint8_t {cname}_map[] = {{")
        for i in range(0, len(data), size):
            out.append("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + size]) + ",")
        out.append("};")
        out.append(f"const lv_image_dsc_t {cname} = {{")
        # solo campos que existen igual en LVGL 9.3 (device-ui) y 9.6 (simulador)
        out.append("    .header.magic = LV_IMAGE_HEADER_MAGIC,")
        out.append("    .header.cf = LV_COLOR_FORMAT_A8,")
        out.append(f"    .header.w = {size},")
        out.append(f"    .header.h = {size},")
        out.append(f"    .header.stride = {size},")
        out.append(f"    .data_size = sizeof({cname}_map),")
        out.append(f"    .data = {cname}_map,")
        out.append("};")
        out.append("")
    print("\n".join(out))


if __name__ == "__main__":
    main()
