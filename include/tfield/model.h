// T-Field UI — datos que muestran las pantallas.
// No depende de Meshtastic: el adaptador del firmware llena estas estructuras.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tfield {

struct Status {
    uint8_t hour = 0, minute = 0;
    std::string day;          // "vie 20"
    std::string month;        // "nov"
    int32_t daylight_min = -1; // minutos de luz restantes (-1 = sin posición; 0 = de noche)
    std::string sunset;       // "20:41"
    std::string sunrise;      // "06:27" (se muestra de noche)
    int8_t battery = -1;      // % propio (-1 = desconocido, 101 = alimentado)
    uint8_t sats = 0;
    bool gps_fix = false;
    std::string short_name;   // "GEO1"
    uint32_t epoch = 0;       // hora UTC (segundos desde 1970)
    // para Ajustes · Nodo y hora
    std::string node_id;      // "!7f3a9c01"
    std::string hw;           // "T-Deck Plus"
    std::string firmware;     // versión del firmware
};

struct TeamNode {
    uint32_t num = 0;
    std::string name;          // nombre corto
    uint32_t color = 0;        // color de identidad
    int32_t dist_m = -1;       // -1 = sin posición
    int16_t bearing_deg = 0;   // rumbo desde mí, 0 = norte
    bool has_alt = false;
    int32_t alt_diff_m = 0;    // altura del nodo − la mía
    int8_t battery = -1;       // %, -1 desconocido, 101 alimentado
    uint8_t signal = 0;        // 0..4
    uint32_t heard_ago_s = 0;  // segundos desde la última vez escuchado
    bool stale = false;        // sin noticias hace mucho
    bool has_pos = false;
    double lat = 0, lon = 0;
    // ficha del nodo
    std::string long_name;     // "Geología 2"
    std::string role;          // "cliente", "repetidor"
    std::string hw;            // "T-Deck Plus"
    float voltage = 0;         // V (0 = desconocido)
    float snr = 0;             // dB del último paquete
    int16_t rssi = 0;          // dBm
    int8_t hops = -1;          // 0 = directo, -1 = desconocido
    uint32_t pos_age_s = 0;    // antigüedad de la posición
    uint16_t acc_m = 0;        // precisión de su GPS
    bool in_team = false;      // lo decide la App según Ajustes (Settings::team); el adaptador no lo llena
    bool relay = false;        // repetidor o router: sin persona detrás
    bool has_pki = false;      // conocemos su clave pública (mensajes directos cifrados de extremo a extremo)
};

// Posición propia
struct Position {
    bool valid = false;
    double lat = 0, lon = 0;
    int32_t alt = 0;     // m
    uint16_t acc_m = 0;  // precisión horizontal
};

// Punto guardado
struct Waypoint {
    uint32_t id = 0;
    std::string name;    // por defecto "P4"
    std::string note;
    std::string from;    // vacío = mío; si no, quién lo compartió ("GEO2")
    std::string time;    // hora local en que se marcó, "14:05"
    uint32_t epoch = 0;  // UTC
    double lat = 0, lon = 0;
    int32_t alt = 0;
};

struct TrackPoint {
    double lat = 0, lon = 0;
    float alt = 0;
    uint32_t epoch = 0; // UTC
};

// Ruta (track) en curso o guardada
struct Track {
    std::vector<TrackPoint> pts;
    bool recording = false;
    int32_t distance_m = 0;
    uint32_t duration_s = 0;
    int32_t gain_m = 0;
};

// Destinatario al compartir (un canal o un nodo)
struct Recipient {
    std::string id;         // id del chat ("terreno", "geo2") o "node:<num>"
    std::string name;       // "Terreno", "GEO2"
    bool channel = false;
    uint32_t node_num = 0;  // para envíos directos
    uint32_t color = 0;
    std::string detail;     // "canal", "directo · 640 m NE"
    bool team = false;      // nodo de mi equipo
};

// Ruta guardada. En memoria quedan solo sus datos: los puntos están en el GPX de la tarjeta
// SD y se leen al mostrarla (una ruta de un día son ~4000 puntos; con decenas de rutas no
// cabrían en la memoria). La lista se guarda en un índice en la SD (App::load_routes).
struct SavedTrack {
    uint32_t id = 0;
    std::string name;  // por defecto "R1"
    std::string note;
    std::string file;  // "/tfield/rutas/R1.gpx"
    Track track;       // distancia, duración y desnivel (sin puntos)
    TrackPoint start{}; // primer punto: fecha e "Ir al inicio"
    double lat0 = 0, lat1 = 0, lon0 = 0, lon1 = 0; // recuadro, para encuadrarla sin leerla
    uint32_t count = 0; // puntos en el archivo
};

struct Chat {
    std::string id;
    std::string name;
    bool is_channel = false;
    std::string time;    // "14:30"
    std::string preview; // último mensaje
    uint8_t unread = 0;
    uint32_t color = 0;  // color del remitente (directos)
    int8_t channel = -1; // canales: índice en Meshtastic (0 = principal, público); -1 = directo
};

struct Message {
    std::string from;
    uint32_t from_color = 0;
    bool mine = false;
    std::string time;
    std::string text;
    std::string status; // "Entregado", "Enviado"…
    uint32_t waypoint_id = 0; // mensaje de punto compartido: se puede abrir en el mapa
    uint32_t pending_wp = 0;  // punto recibido que espera "Guardar punto"
    uint32_t invite_id = 0;   // invitación a un canal privado que espera "Unirse"
};

struct QuickReplies {
    std::array<std::string, 4> text{};
};

} // namespace tfield
