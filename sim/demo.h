#pragma once
#include "tfield/app.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <utility>

namespace demo {
// Posición propia de ejemplo: Cajón del Maipo, Chile.
constexpr double MY_LAT = -33.6400, MY_LON = -70.3500;
double elevation(double e, double n);                         // m, coordenadas locales
void local_to_latlon(double e, double n, double &lat, double &lon);
tfield::TileSource *topo_layer();
tfield::Position position();
std::vector<tfield::Waypoint> waypoints();
tfield::Track track();
uint32_t now_epoch(int hour, int minute);
std::vector<tfield::SavedTrack> saved_tracks();
tfield::Waypoint shared_from_geo2();
std::vector<tfield::TeamNode> team(uint32_t elapsed_s);
tfield::Status status(int hour, int minute);
std::vector<tfield::Chat> chats();
void load(tfield::App &app, int hour, int minute);
// Tarjeta SD en memoria para los simuladores (rutas, índice, respaldo de la ruta en curso)
std::map<std::string, std::string> &files();
void attach_files(tfield::App &app);
size_t tiles_cached(); // teselas en la caché del mapa de ejemplo (128 KB cada una)
tfield::Settings settings();
extern int tz_shift_h;
extern std::string short_name;
} // namespace demo
