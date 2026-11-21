// T-Field UI — exportación GPX 1.1 (se abre en QGIS, Google Earth, Garmin, etc.)
#pragma once

#include "tfield/model.h"
#include <string>
#include <vector>

namespace tfield {

std::string iso8601(uint32_t epoch);            // "2026-11-20T17:32:00Z"
std::string xml_escape(const std::string &s);
std::string safe_filename(const std::string &name); // "Quebrada norte" -> "Quebrada_norte"
uint32_t epoch_from_utc(int year, int month, int day, int hour, int minute, int second);

std::string gpx_track(const std::string &name, const std::string &note, const Track &t);
std::string gpx_waypoints(const std::vector<Waypoint> &wps);
// Por partes, para escribir una ruta larga sin armar todo el archivo en memoria
std::string gpx_track_head(const std::string &name, const std::string &note, const Track &t);
void gpx_track_point(std::string &out, const TrackPoint &p);
std::string gpx_track_tail();
// Lee los puntos de un GPX (los que escribe T-Field UI; tolera otros con <trkpt lat lon>)
bool gpx_parse_track(const std::string &gpx, std::vector<TrackPoint> &out);
// Lee los puntos (<wpt>) que escribe gpx_waypoints; los ids se numeran desde 1
bool gpx_parse_waypoints(const std::string &gpx, std::vector<Waypoint> &out);
// Cambia nombre y nota sin tocar los puntos; false si no tiene la forma esperada
bool gpx_rename(std::string &gpx, const std::string &name, const std::string &note);
uint32_t parse_iso8601(const char *s); // "2026-11-20T17:32:00Z" → epoch (0 si no se entiende)

// Índice de rutas guardadas (texto, una por línea; termina en "fin;<generación>")
std::string route_index(const std::vector<SavedTrack> &routes, uint32_t gen);
bool parse_route_index(const std::string &text, std::vector<SavedTrack> &out, uint32_t &gen);
// Datos de una ruta a partir de sus puntos: inicio, recuadro, cantidad y, si faltan, distancia,
// duración y desnivel
void route_meta(SavedTrack &st, const std::vector<TrackPoint> &pts);

} // namespace tfield
