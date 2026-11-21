<p align="center">
  <img src="docs/img/banner.jpg" alt="T-Field UI para Meshtastic: comunicación, mapa y navegación en terreno" width="100%">
</p>

# T-Field UI

[English](README.md) · **Español**

[![Pruebas](https://github.com/Roberto-Larregla/t-field-ui/actions/workflows/pruebas.yml/badge.svg)](https://github.com/Roberto-Larregla/t-field-ui/actions/workflows/pruebas.yml)

Interfaz de terreno para la **LILYGO T-Deck Plus** con **Meshtastic**: mensajes, mapa con coordenadas UTM, rutas GPX, seguimiento del equipo y SOS. Está escrita en C++ con LVGL para integrarse en [`meshtastic/device-ui`](https://github.com/meshtastic/device-ui), a tamaño real (320 × 240 px), en **español o inglés** y con tema **claro, oscuro o automático** (oscuro entre la puesta y la salida del sol).

> **Estado:** en desarrollo. La interfaz completa funciona en el simulador y pasa las pruebas automáticas. Falta el adaptador que la conecta al firmware: ver el [plan de implementación](docs/plan-implementacion.md).

![Pantallas de T-Field UI](docs/img/pantallas.png)

## Qué hace

- **Inicio:** hora, luz del día que queda, batería y GPS propios; distancia, rumbo, diferencia de altura, batería y señal de cada compañero; últimos chats y respuestas rápidas.
- **Mensajes:** chats y canales, respuestas rápidas editables, contador de bytes (el límite de Meshtastic) y puntos compartidos con «Ver en el mapa». Borrar un chat (dos pulsaciones), todos, o los mensajes de más de 1, 7 o 30 días.
- **Mapa:** mapa base desde la tarjeta SD o cuadrícula UTM; puntos (P1, P2…), rutas grabadas en GPX (R1, R2…), regla, destino y rastro de cada compañero.
- **Equipo:** «Mi equipo» y toda la red; ficha de cada nodo, pedir posiciones a todos y ruta de señal.
- **SOS:** por el canal y directo a cada miembro, mostrando quién lo recibió.
- **Ajustes:** pantalla (brillo, tema e idioma), GPS, avisos, mensajes, mapa, canal privado con invitación cifrada, SOS, nodo y hora, y radio.
- **Modo oscuro:** no encandila de noche y atenúa el mapa base. En la pantalla IPS de la T-Deck no ahorra batería: eso lo hace el brillo.
- **Teclado y bola:** todo se puede usar sin tocar la pantalla.

## Probarlo en el computador

Necesitas CMake 3.16 o más nuevo y un compilador de C++17 (Linux, macOS o WSL en Windows). LVGL se descarga sola la primera vez que se configura.

```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/tfield_tests                                 # pruebas
./build/sim_png out && python3 sim/raw2png.py out    # capturas en PNG (requiere Pillow)
```

Pruebas con detectores de errores de memoria, y una prueba al azar más larga:

```sh
cmake -B build-checks -S . -DTFIELD_CHECKS=ON && cmake --build build-checks -j && ./build-checks/tfield_tests
SEMILLA=7 PASOS=20000 ./build/tfield_tests prueba_al_azar
```

Simulador en el navegador, con [Emscripten](https://emscripten.org):

```sh
emcmake cmake -B build-web -S . -DCMAKE_BUILD_TYPE=Release && cmake --build build-web -j
cp build-web/tfield_web.js sim/web/    # luego abre sim/web/index.html
```

## Estructura

| Carpeta | Contenido |
|---|---|
| `include/tfield/`, `src/` | La interfaz: lo que irá a device-ui |
| `fonts/` | Atkinson Hyperlegible convertida a LVGL |
| `sim/` | Simulador: capturas, navegador y datos de ejemplo |
| `tests/` | Pruebas de funciones, situaciones, hardware y al azar |
| `docs/` | Plan de implementación e imágenes |

<details>
<summary>Regenerar las fuentes</summary>

Desde la carpeta `fonts/` (cada archivo `.c` guarda en su encabezado el comando exacto con que se hizo):

```sh
npm i -g lv_font_conv
lv_font_conv --no-compress --bpp 4 --size 12 --font src/AtkinsonHyperlegible-Regular.ttf \
  -r 0x20-0x7E -r 0xA0-0xFF -r 0x2013,0x2014,0x2022,0x2026,0x2039,0x203A,0x2212 \
  --format lvgl --lv-include lvgl.h --lv-font-name tfield_font_12 -o tfield_font_12.c
```
</details>

## Créditos

- **Autor:** Roberto Larregla.
- Desarrollado con la ayuda de **Claude**.
- [LVGL](https://lvgl.io) (MIT) y [Meshtastic](https://meshtastic.org) (GPL-3.0).
- Fuente [Atkinson Hyperlegible](https://www.brailleinstitute.org/freefont/), del Braille Institute (SIL Open Font License 1.1, ver `fonts/OFL.txt`).
- El mapa del simulador lo genera el propio código: no son datos reales.

## Licencia

GPL-3.0, la misma de `meshtastic/device-ui`, donde se integrará. Ver [LICENSE](LICENSE).
