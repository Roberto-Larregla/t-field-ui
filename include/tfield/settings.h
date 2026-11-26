// T-Field UI — ajustes que se cambian desde la propia T-Deck.
// Los que son de Meshtastic (pantalla, GPS, nombres, zona horaria, radio, canal) los aplica
// el adaptador del firmware; el resto solo cambia el comportamiento de esta interfaz.
// Se guardan como texto "clave=valor" (ver settings_to_text) en la memoria del equipo.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tfield {

// Qué hacer con un punto que comparte otro nodo
enum class PointPolicy : uint8_t { Auto = 0, Ask = 1, Ignore = 2 };

using ChannelKey = std::array<uint8_t, 32>; // AES-256

// Canal privado del equipo (un canal secundario de Meshtastic con clave propia)
struct TeamChannel {
    bool on = false;
    std::string name = "Equipo"; // máx. 11 bytes (límite de Meshtastic)
    std::string chat_id;         // chat que lo representa en Mensajes
    ChannelKey key{};
};

// Estado de cada miembro del equipo respecto del canal privado
struct ChannelMember {
    enum State : uint8_t { None = 0, Invited = 1, Joined = 2 };
    uint32_t num = 0;
    State state = None;
    uint32_t epoch = 0; // cuándo se invitó o se unió (UTC)
};

// Tipos de nombres en el mapa, de más a menos importante
enum : uint8_t { MAP_NAMES_TEAM = 1, MAP_NAMES_SHARED = 2, MAP_NAMES_MINE = 4, MAP_NAMES_OTHERS = 8 };

// Modos de Ajustes (Terreno, Ahorro, Noche, Campamento): lo que aplica cada uno al tocarlo.
// Se cambian manteniendo apretado el modo.
struct ModeValues {
    uint8_t brightness;     // %
    uint16_t screen_off_s;  // 0 = nunca
    bool kbd_light;
    int8_t theme;           // -1 = no cambia el tema; si no, como Settings::theme
    uint16_t pos_every_s;
    bool pos_smart;
    uint8_t track_every_s;
    bool operator==(const ModeValues &o) const
    {
        return brightness == o.brightness && screen_off_s == o.screen_off_s && kbd_light == o.kbd_light &&
               theme == o.theme && pos_every_s == o.pos_every_s && pos_smart == o.pos_smart &&
               track_every_s == o.track_every_s;
    }
    bool operator!=(const ModeValues &o) const { return !(*this == o); }
};
constexpr int MODE_COUNT = 4;
inline const std::array<ModeValues, MODE_COUNT> DEFAULT_MODES = {{
    {80, 60, true, 0, 120, true, 10},     // Terreno: tema claro
    {40, 30, false, -1, 900, true, 30},   // Ahorro
    {20, 30, false, 1, 300, true, 10},    // Noche: tema oscuro
    {60, 300, true, -1, 1800, false, 60}, // Campamento: sin "antes si me muevo"
}};

struct Settings {
    // Pantalla y teclado
    uint8_t brightness = 70;    // 10..100 %
    uint16_t screen_off_s = 60; // 0 = nunca
    bool kbd_light = true;
    uint8_t theme = 0;          // 0 claro, 1 oscuro, 2 automático (oscuro de noche)
    uint8_t lang = 0;           // 0 español, 1 inglés
    uint8_t msg_keep_days = 0;  // borrar mensajes de más de N días (0 = nunca; 1, 7 o 30)
    // GPS y posición
    bool gps = true;
    uint16_t pos_every_s = 900; // envío de posición a la red
    bool pos_smart = true;      // enviar antes si me muevo
    uint8_t track_every_s = 10; // punto de ruta al grabar
    // Modos (Ajustes): sus valores, cambiables
    std::array<ModeValues, MODE_COUNT> modes = DEFAULT_MODES;
    // Mensajes
    bool team_first = true;
    // Mapa y puntos
    bool map_team_only = false;
    // Nombres en el mapa (el mapa va limpio: el nombre de algo se ve al elegirlo). Por tipo
    // (MAP_NAMES_*) y uno por uno (nodos y puntos marcados "siempre con nombre", máx. 64)
    uint8_t map_names = 0;
    uint8_t map_hidden = 0; // capas ocultas (MAP_NAMES_TEAM/SHARED/MINE; los otros nodos: map_team_only)
    std::vector<uint32_t> map_name_nodes, map_name_wps;
    PointPolicy pts_team = PointPolicy::Auto;
    PointPolicy pts_others = PointPolicy::Ask;
    bool share_all_team = true;
    // SOS
    bool sos_direct = true;  // además, directo a cada miembro, con confirmación
    bool sos_public = true;  // canal principal (público)
    bool sos_team_ch = true; // canal privado del equipo
    uint8_t sos_every_min = 2;
    // Mi equipo: nodos marcados (se guardan aquí para que no se pierdan al reiniciar)
    std::vector<uint32_t> team;
    // Chats directos vaciados con Borrar: su pestaña sigue ahí al reiniciar (máx. 32)
    std::vector<uint32_t> kept_chats;
    // Canal privado
    TeamChannel channel;
    std::vector<ChannelMember> members;
    // Nodo y hora
    std::string short_name = "GEO1";
    std::string long_name = "Geolog\xC3\xAD" "a 1";
    uint8_t tz = 0; // índice en time_zones()
    // Radio (Avanzado)
    uint8_t region = 0; // índice en lora_regions()
    uint8_t preset = 0; // índice en lora_presets()
    uint8_t hops = 3;   // 1..7

    ChannelMember *member(uint32_t num);
    const ChannelMember *member(uint32_t num) const;
};

// Opción de una lista: código para el firmware, nombre y una línea de detalle.
struct Option {
    const char *code;
    const char *name;
    const char *detail;
};
const std::vector<Option> &time_zones();   // code = zona POSIX (Meshtastic: device.tzdef)
const std::vector<Option> &lora_regions(); // code = Config.LoRaConfig.RegionCode
const std::vector<Option> &lora_presets(); // code = Config.LoRaConfig.ModemPreset

// Texto "clave=valor" por línea. Las claves desconocidas se ignoran y las que faltan
// quedan con su valor por defecto, así un archivo viejo sigue sirviendo.
std::string settings_to_text(const Settings &s);
Settings settings_from_text(const std::string &text);

std::string base64_encode(const uint8_t *data, size_t n);
bool base64_decode(const std::string &s, std::vector<uint8_t> &out);
// Huella corta de la clave para compararla a simple vista entre equipos: "A3F2 19C0"
std::string key_fingerprint(const ChannelKey &key);
// Recorta a n bytes sin cortar un carácter UTF-8 por la mitad
std::string utf8_truncate_bytes(const std::string &s, size_t max_bytes);

} // namespace tfield
