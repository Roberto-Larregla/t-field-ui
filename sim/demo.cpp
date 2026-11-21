// Datos de ejemplo para el simulador (los reemplaza el firmware en la T-Deck).
#include "demo.h"
#include "tfield/i18n.h"
#include "tfield/format.h"
#include "tfield/gpx.h"

#include <cstdlib>

#include <cmath>

namespace demo {

using namespace tfield;

static int my_alt() { return (int)std::lround(elevation(0, 0)); }

struct Peer {
    uint32_t num;
    const char *name, *long_name, *role, *hw;
    uint32_t color;
    double dn_m, de_m; // desplazamiento norte/este en metros
    bool has_pos;
    int8_t battery;
    float voltage, snr;
    int16_t rssi;
    int8_t hops;
    uint8_t signal;
    uint32_t heard_s, pos_age_s;
    uint16_t acc;
    bool in_team;
    bool relay, pki;
};

static Peer peers[] = {
    {2, "GEO2", "Geolog\xC3\xAD" "a 2", "cliente", "T-Deck Plus", col::PURPLE, 452, 452, true, 64, 3.92f, 6.5f, -92, 0, 4, 20, 90, 6, true, false, true},
    {3, "VEH1", "Camioneta", "cliente", "Heltec V3", col::AMBER, 96, -1096, true, 18, 3.55f, -8.0f, -118, 1, 0, 38 * 60, 38 * 60, 8, true, false, true},
    {4, "BASE", "Campamento", "cliente", "RAK WisBlock", col::BLUE, -1697, -1697, true, 101, 4.98f, 2.25f, -104, 0, 3, 2 * 60, 4 * 60, 3, true, false, true},
    {5, "RPT1", "Repetidor cerro", "repetidor", "RAK WisBlock", col::GREEN, 5712, -1007, true, 91, 4.11f, -1.5f, -110, 0, 2, 12 * 60, 15 * 60, 4, true, true, true},
    {9, "MNT4", "Monta\xC3\xB1" "ista", "cliente", "T-Echo", 0x6E7B85, 1200, 3100, true, 47, 3.78f, -12.0f, -121, 2, 1, 6 * 60, 6 * 60, 12, false, false, true},
    {11, "Q7XA", "Meshtastic 7xa", "cliente", "Station G2", 0x6E7B85, 0, 0, false, -1, 0, -14.5f, -124, 3, 1, 25 * 60, 0, 0, false, false, false},
};

static void offset(double dn, double de, double &lat, double &lon) { local_to_latlon(de, dn, lat, lon); }

std::vector<TeamNode> team(uint32_t elapsed_s)
{
    std::vector<TeamNode> out;
    for (auto &p : peers) {
        TeamNode n;
        n.num = p.num;
        n.name = p.name;
        n.long_name = p.long_name;
        n.role = p.role;
        n.hw = p.hw;
        n.color = p.color;
        if (p.has_pos) {
            double lat, lon;
            offset(p.dn_m, p.de_m, lat, lon);
            n.dist_m = (int32_t)haversine_m(MY_LAT, MY_LON, lat, lon);
            n.bearing_deg = (int16_t)bearing_deg(MY_LAT, MY_LON, lat, lon);
            n.has_alt = true;
            n.alt_diff_m = (int)std::lround(elevation(p.de_m, p.dn_m)) - my_alt();
            n.has_pos = true;
            n.lat = lat;
            n.lon = lon;
        }
        n.battery = p.battery;
        n.voltage = p.voltage;
        n.snr = p.snr;
        n.rssi = p.rssi;
        n.hops = p.hops;
        n.signal = p.signal;
        n.heard_ago_s = p.heard_s + elapsed_s;
        n.pos_age_s = p.pos_age_s + elapsed_s;
        n.acc_m = p.acc;
        n.stale = n.heard_ago_s > 30 * 60;
        n.in_team = p.in_team;
        n.relay = p.relay;
        n.has_pki = p.pki;
        out.push_back(n);
    }
    std::sort(out.begin(), out.end(), [](const TeamNode &a, const TeamNode &b) {
        return (a.dist_m < 0 ? 1e9 : a.dist_m) < (b.dist_m < 0 ? 1e9 : b.dist_m);
    });
    return out;
}

// Hora local de Chile (UTC−3) del 20 nov 2026 -> UTC
uint32_t now_epoch(int hour, int minute) { return epoch_from_utc(2026, 11, 20, hour + 3, minute, 0); }

int tz_shift_h = 0; // Ajustes › Zona horaria, respecto de Chile continental en verano (UTC−3)
std::string short_name = "GEO1";

Status status(int hour, int minute)
{
    Status s;
    s.epoch = now_epoch(hour, minute);
    s.hour = (uint8_t)((hour + tz_shift_h + 24) % 24);
    s.minute = minute;
    s.node_id = "!a3c1e2f0";
    s.hw = "T-Deck Plus";
    s.firmware = "2.7";
    s.day = tfield::english() ? "Fri 20" : "vie 20"; // el adaptador los escribe en el idioma elegido
    s.month = tfield::english() ? "Nov" : "nov";
    s.short_name = short_name;
    s.battery = 82;
    s.sats = 9;
    s.gps_fix = true;
    // lo mismo que hará el adaptador: luz restante y puesta, en hora local
    DaylightInfo d = daylight_info(s.epoch, (-3 + tz_shift_h) * 60, MY_LAT, MY_LON);
    if (d.valid) {
        s.sunset = d.sunset;
        s.sunrise = d.sunrise;
        s.daylight_min = d.minutes_left;
    }
    return s;
}

std::vector<Chat> chats()
{
    return {
        {"geo2", "GEO2", false, "14:30", "Afloramiento en la quebrada, tomo muestras", 1, col::PURPLE},
        {"terreno", "Terreno", true, "14:12", "BASE: Almuerzo a las 15:00", 1, 0, 1},
        {"longfast", "LongFast", true, "14:02", "VEH1: Camioneta en el port\xC3\xB3n norte", 0, 0, 0},
        {"base", "BASE", false, "13:40", "Recibido, quedo atento", 0, col::BLUE},
    };
}

std::vector<std::pair<std::string, std::vector<Message>>> messages()
{
    return {
        {"geo2",
         {{"T\xC3\xBA", 0, true, "14:05", "Voy al afloramiento P4", "Entregado"},
          {"GEO2", col::PURPLE, false, "14:21", "Ok, nosotros seguimos por la quebrada", ""},
          {"GEO2", col::PURPLE, false, "14:30", "Afloramiento en la quebrada, tomo muestras", ""}}},
        {"terreno",
         {{"BASE", col::BLUE, false, "13:58", "Llegando al punto 4, todo en orden", ""},
          {"GEO2", col::PURPLE, false, "14:06", "Vamos subiendo por la quebrada", ""},
          {"BASE", col::BLUE, false, "14:12", "Almuerzo a las 15:00", ""}}},
        {"base",
         {{"T\xC3\xBA", 0, true, "13:38", "Salimos del campamento", "Entregado"},
          {"BASE", col::BLUE, false, "13:40", "Recibido, quedo atento", ""}}},
        {"longfast",
         {{"CERRO", col::SLATE, false, "ayer", "Hola desde el cerro", ""},
          {"VEH1", col::AMBER, false, "14:02", "Camioneta en el port\xC3\xB3n norte", ""}}},
    };
}

// Rastro de ejemplo: GEO2 subió por la quebrada; la camioneta vino por el camino
static void demo_trails(App &app)
{
    for (int i = 0; i <= 30; i++) {
        double t = i / 30.0;
        double e = -300 + 752 * t + 60 * std::sin(t * 7), n = -500 + 952 * t + 40 * std::cos(t * 5);
        TrackPoint p;
        local_to_latlon(e, n, p.lat, p.lon);
        p.alt = (float)elevation(e, n);
        p.epoch = now_epoch(13, 30) + (uint32_t)(i * 120);
        app.add_trail_point(2, p);
    }
    for (int i = 0; i <= 20; i++) {
        double e = -2600 + 1504 * i / 20.0;
        double n = 0.75 * e + 180.0 * std::sin(e / 700.0) - 75.0;
        if (i == 20)
            n = 96;
        TrackPoint p;
        local_to_latlon(e, n, p.lat, p.lon);
        p.alt = (float)elevation(e, n);
        p.epoch = now_epoch(13, 20) + (uint32_t)(i * 60);
        app.add_trail_point(3, p);
    }
}

Position position()
{
    Position p;
    p.valid = true;
    p.lat = MY_LAT;
    p.lon = MY_LON;
    p.alt = my_alt();
    p.acc_m = 4;
    return p;
}

std::vector<Waypoint> waypoints()
{
    struct W {
        const char *name, *time;
        double e, n;
    } ws[] = {{"P1 Campamento", "09:12", -1697, -1697},
              {"P2 Quebrada", "12:40", -930, -760},
              {"P3 Contacto", "13:51", -380, -260},
              {"P4 Afloramiento", "14:05", 640, 820}};
    std::vector<Waypoint> out;
    uint32_t id = 1;
    for (auto &w : ws) {
        Waypoint wp;
        wp.id = id++;
        wp.name = w.name;
        wp.time = w.time;
        local_to_latlon(w.e, w.n, wp.lat, wp.lon);
        wp.alt = (int)std::lround(elevation(w.e, w.n));
        int hh = atoi(w.time), mm = atoi(w.time + 3);
        wp.epoch = now_epoch(hh, mm);
        out.push_back(wp);
    }
    return out;
}

// Ruta grabada desde el campamento, siguiendo el camino del valle
Track track()
{
    Track t;
    t.recording = true;
    double prev_lat = 0, prev_lon = 0, prev_h = 0;
    for (int i = 0; i <= 60; i++) {
        double e = -1697 + (1697 - 40) * i / 60.0;
        double n = 0.75 * e + 180.0 * std::sin(e / 700.0) - 120.0 + 45 + 6 * std::sin(i * 1.7);
        if (i > 52) // último tramo: sube hacia mi posición
            n = n + (0 - n) * (i - 52) / 8.0;
        double lat, lon;
        local_to_latlon(e, n, lat, lon);
        double h = elevation(e, n);
        if (i > 0) {
            t.distance_m += (int32_t)haversine_m(prev_lat, prev_lon, lat, lon);
            if (h > prev_h)
                t.gain_m += (int32_t)(h - prev_h);
        }
        TrackPoint tp;
        tp.lat = lat;
        tp.lon = lon;
        tp.alt = (float)h;
        tp.epoch = now_epoch(14, 32) - 72 * 60 + (uint32_t)(i * 72 * 60 / 60);
        t.pts.push_back(tp);
        prev_lat = lat, prev_lon = lon, prev_h = h;
    }
    t.duration_s = 72 * 60;
    return t;
}

// Una ruta guardada ayer, para el menú Rutas
std::vector<SavedTrack> saved_tracks()
{
    SavedTrack st;
    st.id = 1;
    st.name = "R1 Subida mirador";
    st.note = "Sendero con pircas";
    st.file = "/tfield/rutas/R1_Subida_mirador.gpx";
    double prev_lat = 0, prev_lon = 0, prev_h = 0;
    uint32_t t0 = epoch_from_utc(2026, 11, 19, 13, 5, 0);
    for (int i = 0; i <= 40; i++) {
        double e = -1697 + 420 * std::sin(i / 40.0 * 1.6), n = -1697 + 1300 * i / 40.0 + 60 * std::sin(i * 0.7);
        double lat, lon;
        local_to_latlon(e, n, lat, lon);
        double h = elevation(e, n);
        if (i > 0) {
            st.track.distance_m += (int32_t)haversine_m(prev_lat, prev_lon, lat, lon);
            if (h > prev_h)
                st.track.gain_m += (int32_t)(h - prev_h);
        }
        st.track.pts.push_back({lat, lon, (float)h, t0 + (uint32_t)i * 90});
        prev_lat = lat, prev_lon = lon, prev_h = h;
    }
    st.track.duration_s = 40 * 90;
    return {st};
}

Waypoint shared_from_geo2()
{
    Waypoint w;
    w.name = "Veta de cuarzo";
    w.note = "Muestra M-12";
    w.from = "GEO2";
    w.time = "14:36";
    w.epoch = now_epoch(14, 36);
    local_to_latlon(520, 380, w.lat, w.lon);
    w.alt = (int32_t)std::lround(elevation(520, 380));
    return w;
}

// Ajustes de ejemplo: canal privado "Terreno" con GEO2 y BASE unidos; VEH1 invitado sin respuesta
Settings settings()
{
    Settings s;
    s.team = {2, 3, 4, 5}; // GEO2, VEH1, BASE y el repetidor
    s.channel.on = true;
    s.channel.name = "Terreno";
    s.channel.chat_id = "terreno";
    for (size_t i = 0; i < s.channel.key.size(); i++)
        s.channel.key[i] = (uint8_t)(i * 37 + 11);
    s.members = {{2, ChannelMember::Joined, now_epoch(9, 5)},
                 {4, ChannelMember::Joined, now_epoch(9, 6)},
                 {3, ChannelMember::Invited, now_epoch(13, 10)}};
    return s;
}

void load(App &app, int hour, int minute)
{
    demo_trails(app);
    app.set_settings(settings());
    if (!app.load_routes()) // primera vez: la ruta de ayer, como si se hubiera guardado
        for (auto &st : saved_tracks())
            app.add_saved_track(st.name, st.note, st.track);
    app.set_map_layers({topo_layer()});
    app.set_position(position());
    app.set_waypoints(waypoints());
    app.set_track(track());
    app.set_status(status(hour, minute));
    for (auto &m : messages())
        app.set_messages(m.first, m.second);
    app.set_chats(chats());
    app.set_team(team(0));
}

} // namespace demo

namespace demo {
std::map<std::string, std::string> &files()
{
    static std::map<std::string, std::string> fs;
    return fs;
}

void attach_files(tfield::App &app)
{
    app.on_save_file = [](const std::string &path, const std::string &content) {
        files()[path] = content;
        return true;
    };
    app.on_append_file = [](const std::string &path, const std::string &content) {
        files()[path] += content;
        return true;
    };
    app.on_delete_file = [](const std::string &path) {
        files().erase(path);
        return true;
    };
    app.on_read_file = [](const std::string &path, std::string &content) {
        auto it = files().find(path);
        if (it == files().end())
            return false;
        content = it->second;
        return true;
    };
}
} // namespace demo
