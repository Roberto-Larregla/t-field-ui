// T-Field UI — formato de números y cálculos geográficos simples.
#pragma once

#include "tfield/model.h"

#include <cstdint>
#include <string>

namespace tfield {

// "640 m", "1,1 km", "12 km"
std::string format_distance(int32_t meters);
// "+38 m", "−95 m"
std::string format_alt_diff(int32_t meters);
// "N", "NE", "E", "SE", "S", "SO", "O", "NO"
const char *cardinal(int bearing_deg);
// "directo", "1 salto", "3 saltos", "" si no se sabe
std::string format_hops(int hops);
// "ahora", "5 min", "2 h", "3 d"
std::string format_ago(uint32_t seconds);
std::string ago_text(uint32_t seconds); // "hace 5 min" / "5 min ago"; "ahora" / "now"
// "6 h 09" ; "45 min"
std::string format_daylight(int32_t minutes);
// recorta a n caracteres UTF-8 (no bytes)
std::string utf8_truncate(const std::string &s, size_t max_chars);
size_t utf8_length(const std::string &s);

// Distancia (m) y rumbo inicial (grados) entre dos puntos WGS84.
double haversine_m(double lat1, double lon1, double lat2, double lon2);
// Desnivel positivo con histéresis: el GPS de la T-Deck (sin barómetro) mueve la altura varios
// metros estando quieto; sumar cada subida inflaría el total, y truncar cada paso a metros
// enteros lo borraría (subiendo despacio, menos de 1 m entre puntos). Solo cuenta cuando la
// altura pasa CLIMB_STEP_M por encima de la referencia; al bajar esa cifra, la referencia baja.
constexpr float CLIMB_STEP_M = 4.0f;
void climb_add(double &gain, float &ref, float alt);
// Distancia y desnivel de una lista de puntos (en double; se redondea al final)
void track_totals(const std::vector<TrackPoint> &pts, int32_t &distance_m, int32_t &gain_m);
double bearing_deg(double lat1, double lon1, double lat2, double lon2);

// UTM (WGS84). zone 1..60, band letra C..X, e/n en metros.
struct Utm {
    int zone;
    char band;
    double e, n;
};
Utm to_utm(double lat, double lon);
// "19H 360 214 E  6 276 448 N"
std::string format_utm(const Utm &u);

// Web Mercator: coordenadas de píxel global a un nivel de zoom (teselas de 256 px).
void mercator_px(double lat, double lon, int zoom, double &x, double &y);
void mercator_latlon(double x, double y, int zoom, double &lat, double &lon);
double meters_per_px(double lat, int zoom);

// Salida y puesta de sol (algoritmo NOAA simplificado).
// Devuelve minutos desde medianoche UTC (pueden pasar de 1440 o ser negativos: la puesta
// en Magallanes cae después de medianoche UTC); false si no hay salida/puesta (polar).
bool sun_times_utc(int year, int month, int day, double lat, double lon, double &sunrise_min, double &sunset_min);

// Lo que muestra el inicio: minutos de luz que quedan y hora local de salida/puesta.
// tz_offset_min: diferencia de la hora local con UTC (Chile continental en verano: −180).
struct DaylightInfo {
    bool valid = false;       // sin posición o día/noche polar: false
    int32_t minutes_left = 0; // 0 de noche (antes de la salida o después de la puesta)
    bool night = false;
    std::string sunrise, sunset; // "06:27", "20:26"
};
DaylightInfo daylight_info(uint32_t epoch_utc, int tz_offset_min, double lat, double lon);

} // namespace tfield
