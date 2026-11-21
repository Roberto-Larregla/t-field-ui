<p align="center">
  <img src="docs/img/banner.jpg" alt="T-Field UI for Meshtastic: field communication, mapping and navigation" width="100%">
</p>

# T-Field UI

**English** · [Español](README.es.md)

[![Tests](https://github.com/Roberto-Larregla/t-field-ui/actions/workflows/pruebas.yml/badge.svg)](https://github.com/Roberto-Larregla/t-field-ui/actions/workflows/pruebas.yml)
[![Firmware](https://github.com/Roberto-Larregla/t-field-ui/actions/workflows/firmware.yml/badge.svg)](https://github.com/Roberto-Larregla/t-field-ui/actions/workflows/firmware.yml)

A field user interface for the **LILYGO T-Deck Plus** running **Meshtastic**: messaging, a map with UTM coordinates, GPX tracks, team tracking and SOS. It replaces the standard Meshtastic screen interface and works with any other Meshtastic node, the phone app and repeaters.

The interface comes in **English and Spanish**, with a **light, dark or automatic** theme (dark from sunset to sunrise).

> **Status:** first version on the device. The interface is complete and passes the automated tests; the firmware integration is new. If something doesn't work, please report it in [Issues](https://github.com/Roberto-Larregla/t-field-ui/issues).

![T-Field UI screens, light and dark](docs/img/screens-en.png)

## Features

- **Home:** time, remaining daylight, own battery and GPS; distance, bearing, height difference, battery and signal for each teammate; latest chats and quick replies.
- **Messages:** chats and channels, editable quick replies, a byte counter (Meshtastic's real limit) and shared waypoints you can open on the map. Delete one chat (press twice), all chats, or messages older than 1, 7 or 30 days.
- **Map:** base map tiles from the SD card or a UTM grid; waypoints (P1, P2…), recorded GPX tracks (R1, R2…), a ruler, a navigation target and each teammate's trail.
- **Team:** "my team" and the whole mesh; a card for each node, request positions from everyone, and traceroute.
- **SOS:** sent on the channel and directly to each teammate, showing who received it.
- **Settings:** display (brightness, theme, language), GPS, alerts, messages, map, a private channel with encrypted invitations, SOS, node and time, and radio.
- **Dark mode:** no glare at night, with the base map dimmed. On the T-Deck's IPS screen it doesn't save battery: the backlight does that.
- **Keyboard and trackball:** everything works without touching the screen.

## Installation

T-Field UI is installed by building the Meshtastic firmware with a version of [`device-ui`](https://github.com/meshtastic/device-ui) (the library that draws Meshtastic's screen) that shows T-Field UI. Only one line of the firmware changes.

### What you need

- A LILYGO **T-Deck Plus**. The T-Deck without "Plus" also works, but it has no GPS.
- A USB-C **data** cable (some cables only charge).
- A computer with Windows, macOS or Linux, [VS Code](https://code.visualstudio.com) and the [PlatformIO](https://platformio.org/install/ide?install=vscode) extension. If you prefer the terminal, [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) is enough.
- [Git](https://git-scm.com/downloads).
- Optional: a microSD card for base maps, tracks and waypoints.

If the T-Deck has never run Meshtastic, install the standard firmware first with the [Meshtastic Web Flasher](https://flasher.meshtastic.org) ("Full Erase and Install"). That prepares the device's memory.

### 1. Download the Meshtastic firmware

In a terminal:

```sh
git clone https://github.com/meshtastic/firmware.git
cd firmware
git checkout fe89ac1ddbbe02cdcc4d15a33f1967f5dcd55daa
git submodule update --init
```

T-Field UI is built against that commit.

### 2. Point the firmware to the T-Field UI version of device-ui

Open `platformio.ini` (in the `firmware` folder) and find the `[device-ui_base]` section. Replace the line that starts with `https://github.com/meshtastic/device-ui/archive/` with:

```ini
	https://github.com/Roberto-Larregla/device-ui/archive/9de6bfecbcef2a61e37be167a1fa17cab3c9501a.zip
```

Keep the tab at the start of the line. Nothing else in the firmware changes: the radio, the mesh, GPS and encryption are still Meshtastic's.

### 3. Build and install

1. Open the `firmware` folder in VS Code. PlatformIO downloads what it needs the first time; that can take a while.
2. Choose the **`env:t-deck-tft`** environment (PlatformIO bar at the bottom of the window).
3. Put the T-Deck in download mode: switch it **off**, press and hold the **trackball**, switch it **on**, and release the trackball after 2–3 seconds. The screen stays black.
4. Connect the T-Deck to the computer with the USB-C cable and click **Upload** (the → arrow in the PlatformIO bar).
5. When it finishes, switch the T-Deck off and on again.

From the terminal, steps 2 and 4 are: `pio run -e t-deck-tft -t upload`

The firmware keeps the device's Meshtastic settings (region, channels, node name). To go back to the standard interface, install the official firmware again with the Web Flasher.

If the build stops with `IncompatiblePlatform` and `PlatformIO Core v6.2.0-post.1`, it is a known problem of the ESP32 platform's second build pass. Run this once and build again:

```sh
~/.platformio/penv/bin/python -m ensurepip
~/.platformio/penv/bin/python -m pip install "pioarduino==6.2.0"
```

On Windows the folder is `%USERPROFILE%\.platformio\penv\Scripts\`.

### 4. Prepare the microSD card (optional)

Format the card as FAT32 or exFAT. T-Field UI reads base maps from `/maps` and creates the `/tfield` folder for its own files:

```
/maps/<name>/<z>/<x>/<y>.png       base map: 256 × 256 px PNG tiles
/maps/<name>/<name>.pmtiles        or a single PMTiles archive (PNG tiles)
/tfield/rutas/                     recorded tracks (GPX)
/tfield/puntos.gpx                 waypoints (GPX)
```

Each folder in `/maps` is a base map: the button at the top of the map shows the current one and lets you pick another. Without a card, or outside the tiles, the map shows a UTM grid. GPX files open in QGIS, Google Earth and most GPS apps.

To make tiles from your own maps in [QGIS](https://qgis.org): *Processing Toolbox › Raster tools › Generate XYZ tiles (Directory)*, PNG format, zoom 10 to 17. Copy the output folder into `/maps`.

### 5. First setup on the device

Settings is the fourth tab on the home screen (or the `4` key).

1. **Settings › Display:** language, theme and brightness.
2. **Settings › Advanced:** the **LoRa region** of your country (see [Meshtastic's list](https://meshtastic.org/docs/configuration/region-by-country/)) and the **preset**. Every device in the group needs the same region and preset. Applying restarts the radio.
3. **Settings › Node & time:** short name (4 characters), long name and time zone.
4. **Team › Whole mesh:** open each teammate's card and choose **Add to team**. They then appear on the home screen with distance and bearing.
5. **Settings › Private channel** (optional): create the channel and invite your team. Invitations are end-to-end encrypted, so they only reach nodes whose public key your device already knows (nodes it has heard).
6. **Settings › SOS:** choose who receives your SOS.

## Known limitations

- **Sound:** message alerts use Meshtastic's notification settings (set them in the Meshtastic app). T-Field UI's own volume and "sound for my team / others / points" options do not change the sound yet.
- **Private channel and shared waypoints:** the "Join" and "Show on map" buttons work between devices running T-Field UI. Other Meshtastic apps receive shared waypoints as standard waypoints.
- **Track recording** asks the GPS for a fix every few seconds while it records, which uses more battery; the previous interval comes back when you stop.

## For developers

- `include/tfield/`, `src/`: the interface (C++17, LVGL 9). It knows nothing about Meshtastic: an adapter fills its data (`set_team`, `add_message`, `set_position`…) and answers its hooks (`on_send_text`, `on_sos`, `on_settings`, `on_save_file`…).
- The adapter is [`TFieldView`](https://github.com/Roberto-Larregla/device-ui/blob/master/source/graphics/TFT/TFieldView.cpp) in the device-ui fork. The fork's `library.json` pins the T-Field UI commit it uses.
- `sim/` and `tests/`: a simulator and automated tests, built with CMake. They run on every push ([Tests](https://github.com/Roberto-Larregla/t-field-ui/actions/workflows/pruebas.yml)). [Firmware](https://github.com/Roberto-Larregla/t-field-ui/actions/workflows/firmware.yml) builds the T-Deck firmware with the current code and keeps the `.bin` files as a download.
- Limits sized for the T-Deck (ESP32-S3, 8 MB PSRAM): 60 messages per chat and 600 in total, a 200-point trail per teammate, 10 base-map tiles in memory. Only what changes is redrawn.

<details>
<summary>Regenerate the fonts</summary>

From the `fonts/` folder (each `.c` file records in its header the exact command used):

```sh
npm i -g lv_font_conv
lv_font_conv --no-compress --bpp 4 --size 12 --font src/AtkinsonHyperlegible-Regular.ttf \
  -r 0x20-0x7E -r 0xA0-0xFF -r 0x2013,0x2014,0x2022,0x2026,0x2039,0x203A,0x2212 \
  --format lvgl --lv-include lvgl.h --lv-font-name tfield_font_12 -o tfield_font_12.c
```
</details>

## Credits

- **Author:** Roberto Larregla.
- Developed with help from **Claude**.
- [LVGL](https://lvgl.io) (MIT) and [Meshtastic](https://meshtastic.org) (GPL-3.0).
- [Atkinson Hyperlegible](https://www.brailleinstitute.org/freefont/) font by the Braille Institute (SIL Open Font License 1.1, see `fonts/OFL.txt`).

## License

GPL-3.0, the same as `meshtastic/device-ui`. See [LICENSE](LICENSE).
