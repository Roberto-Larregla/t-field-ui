// T-Field UI — paleta, fuentes y medidas comunes.
// Pantalla T-Deck Plus: 320x240 px, IPS LCD, RGB565.
#pragma once

#include "lvgl.h"
#include <cstdint>

LV_FONT_DECLARE(tfield_font_12)
LV_FONT_DECLARE(tfield_font_12b)
LV_FONT_DECLARE(tfield_font_14)
LV_FONT_DECLARE(tfield_font_14b)
LV_FONT_DECLARE(tfield_font_24b)

namespace tfield {

// Paleta "Claro" (el modo oscuro la traduce color por color). En una LCD los colores no
// cambian el consumo: el modo oscuro es para la noche (no encandila), no para ahorrar.
namespace col {
constexpr uint32_t BG       = 0xEEF3F6; // fondo de franjas y encabezados
constexpr uint32_t SURF     = 0xFFFFFF; // superficies
constexpr uint32_t INK      = 0x10212B; // texto principal y bordes fuertes
constexpr uint32_t INK2     = 0x4A5A66; // texto secundario
constexpr uint32_t LINE     = 0xC9D3DA; // bordes suaves
constexpr uint32_t LINE2    = 0xDCE3E8; // separadores de filas
constexpr uint32_t MUTED    = 0x8796A1; // desactivado
constexpr uint32_t BLUE     = 0x1F63B5; // Mensajes
constexpr uint32_t BLUE_DK  = 0x174C8C; // Mensajes presionado
constexpr uint32_t BLUE_BG  = 0xE8F0FA; // selección suave
constexpr uint32_t GREEN    = 0x2E6B34; // Mapa
constexpr uint32_t PURPLE   = 0x3B2E8F; // Equipo
constexpr uint32_t SLATE    = 0x3F4E59; // Ajustes
constexpr uint32_t AMBER    = 0x8A5A00; // dato antiguo
constexpr uint32_t AMBER_BG = 0xFFF6E5;
constexpr uint32_t RED      = 0xC2271D; // batería crítica
} // namespace col

// Modo oscuro: los mismos nombres de color, con su par oscuro (theme.cpp). Cambiar de modo
// rehace las pantallas (App::rebuild_ui), así que basta con elegir el color al crearlas.
extern bool g_dark;
uint32_t dark_of(uint32_t hex);
inline lv_color_t c(uint32_t hex) { return lv_color_hex(g_dark ? dark_of(hex) : hex); }

constexpr int32_t SCREEN_W = 320;
constexpr int32_t SCREEN_H = 240;

// Límites de texto. Meshtastic mide en bytes (UTF-8): "ñ" o "á" usan 2.
constexpr uint32_t QUICK_MAX_CHARS = 14;   // respuestas rápidas (caracteres): caben en los botones del inicio
constexpr uint32_t MSG_MAX_BYTES = 200;    // mensaje de texto (el paquete admite ~230 bytes)
constexpr uint32_t WP_NAME_MAX_BYTES = 29; // nombre de punto (Waypoint.name)
constexpr uint32_t WP_NOTE_MAX_BYTES = 99; // nota de punto (Waypoint.description)
constexpr uint32_t SHORT_NAME_MAX_BYTES = 4;
constexpr uint32_t LONG_NAME_MAX_BYTES = 39;
constexpr uint32_t CHANNEL_NAME_MAX_BYTES = 11;
constexpr size_t CHAT_KEEP_MESSAGES = 60;   // por chat, en memoria
constexpr size_t CHAT_TOTAL_MESSAGES = 600; // entre todos los chats (~300 B cada uno: ~180 KB)
constexpr size_t CHAT_SHOW_MESSAGES = 15;   // burbujas al abrir un chat (cada una son 4–7 objetos);
                                            // "Mensajes anteriores" suma de a 15
constexpr size_t TRAIL_TEAM_POINTS = 200;   // rastro de cada compañero
constexpr size_t TRAIL_OTHER_POINTS = 10;   // rastro de los nodos ajenos (pueden ser 240)
// Red grande (Meshtastic guarda hasta 250 nodos en el ESP32-S3)
constexpr size_t TEAM_LIST_OTHERS = 50;   // "Toda la red": los ajenos más cercanos
constexpr size_t SHARE_OTHERS = 20;       // al compartir: los ajenos más cercanos
// Mapa: marcadores solo para lo que se ve (cada uno son 2 objetos LVGL)
constexpr size_t MAP_NODE_MARKS = 40; // nodos ajenos a la vista (mi equipo, siempre)
constexpr size_t MAP_WP_MARKS = 40;   // puntos a la vista, los más cercanos al centro
constexpr size_t MAP_WP_LABELS = 12;  // con nombre

} // namespace tfield
