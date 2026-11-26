// Navegación y estado compartido entre pantallas.
#include "tfield/app.h"
#include "tfield/i18n.h"
#include "tfield/format.h"
#include "tfield/gpx.h"
#include "tfield/ui.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace tfield {

using namespace ui;

void App::init()
{
#if LV_USE_THEME_DEFAULT
    // tema base claro (los colores los pone cada pantalla): igual en el simulador y en
    // el firmware, aunque su configuración de LVGL traiga el tema oscuro
    if (lv_display_t *d = lv_display_get_default())
        lv_display_set_theme(d, lv_theme_default_init(d, lv_palette_main(LV_PALETTE_BLUE),
                                                      lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT));
#endif
    home.create(this);
    messages_screen.create(this);
    quick_screen.create(this);
    map_screen.create(this);
    team_screen.create(this);
    settings_screen.create(this);
    quick_ = quick_defaults();
    home.set_quick(quick_);
    build_sos();
    lv_screen_load(home.screen);
}

lv_group_t *App::active_group() const
{
    if (sos_overlay_ && !lv_obj_has_flag(sos_overlay_, LV_OBJ_FLAG_HIDDEN))
        return sos_group_;
    switch (current_) {
    case ScreenId::Home:
        return home.group;
    case ScreenId::Messages:
        return messages_screen.group;
    case ScreenId::QuickReplies:
        return quick_screen.group;
    case ScreenId::Map:
        return map_screen.active_group();
    case ScreenId::Team:
        return team_screen.active_group();
    case ScreenId::Settings:
        return settings_screen.group;
    }
    return nullptr;
}

View App::snapshot() const
{
    View v;
    v.id = current_;
    if (current_ == ScreenId::Team)
        v.node = team_screen.detail_num();
    if (current_ == ScreenId::Messages)
        v.chat = messages_screen.current;
    if (current_ == ScreenId::Settings)
        v.section = (int8_t)settings_screen.section();
    return v;
}

void App::show(ScreenId id) { go(View{id, 0, ""}, true, false); }

// Navega a una vista. push: guardar la actual para volver con Esc.
// restoring: se vuelve atrás, así que se conserva el estado de la pantalla (p. ej. el mapa).
void App::go(const View &v, bool push, bool restoring)
{
    View cur = snapshot();
    if (cur.id == ScreenId::Settings && v.id != ScreenId::Settings)
        flush_settings(); // al salir de Ajustes queda guardado
    bool same = cur.id == v.id && cur.node == v.node && cur.section == v.section && (v.chat.empty() || cur.chat == v.chat);
    if (same && !restoring)
        return;
    if (push && !same) {
        history_.push_back(cur);
        if (history_.size() > 12)
            history_.erase(history_.begin());
    }
    current_ = v.id;
    lv_obj_t *scr = nullptr;
    switch (v.id) {
    case ScreenId::Home:
        history_.clear(); // el inicio es la raíz
        home.clear_sent();
        scr = home.screen;
        if (settings_.theme == 2 && wants_dark() != g_dark)
            apply_look(); // anocheció mientras estabas en otra pantalla
        break;
    case ScreenId::Messages: {
        // al volver: el mismo chat; al entrar: el elegido en el inicio, o el último abierto
        std::string id = !v.chat.empty() ? v.chat : (!force_chat_.empty() ? force_chat_ : home.selected_id());
        force_chat_.clear();
        if (id.empty())
            id = messages_screen.current;
        if (!id.empty())
            open_chat(id);
        messages_screen.refresh_conversation(false); // mismo chat y nada nuevo: queda igual (y en su lugar)
        messages_screen.focus_input();
        scr = messages_screen.screen;
        break;
    }
    case ScreenId::QuickReplies:
        if (!restoring)
            quick_screen.load(quick_);
        scr = quick_screen.screen;
        break;
    case ScreenId::Map:
        if (!restoring)
            map_screen.on_enter();
        scr = map_screen.screen;
        break;
    case ScreenId::Team:
        if (v.node)
            team_screen.show_detail(v.node);
        else
            team_screen.show_list();
        scr = team_screen.screen;
        break;
    case ScreenId::Settings:
        if (v.section >= 0)
            settings_screen.show_section(v.section);
        else
            settings_screen.show_home();
        scr = settings_screen.screen;
        break;
    }
    lv_screen_load(scr);
    if (v.id == ScreenId::Map)
        map_screen.shown(); // lo que cambió mientras no se veía
    refresh_input_group(); // el teclado/trackball sigue a la pantalla activa
}

void App::back()
{
    if (history_.empty()) {
        go(View{}, false, true);
        return;
    }
    View v = history_.back();
    history_.pop_back();
    go(v, false, true);
}

void App::set_status(const Status &s)
{
    status_ = s;
    if (!settings_.short_name.empty()) // el nombre de Ajustes es el del nodo
        status_.short_name = settings_.short_name;
    if (status_.epoch) { // día y mes en el idioma de la interfaz ("vie 20" / "Fri 20")
        static const char *dias[] = {"dom", "lun", "mar", "mi\xC3\xA9", "jue", "vie", "s\xC3\xA1" "b"};
        static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        static const char *meses[] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"};
        static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        int32_t off = (int32_t)(status_.hour * 3600 + status_.minute * 60) - (int32_t)(status_.epoch % 86400);
        if (off > 14 * 3600)
            off -= 86400;
        if (off < -12 * 3600)
            off += 86400;
        off = (int32_t)std::lround(off / 900.0) * 900;
        int64_t local = (int64_t)status_.epoch + off;
        std::string iso = iso8601((uint32_t)local); // "2026-11-20T14:32:00Z"
        int wd = (int)(((local / 86400) % 7 + 11) % 7); // 1970-01-01 fue jueves
        int mon = std::atoi(iso.substr(5, 2).c_str()), mday = std::atoi(iso.substr(8, 2).c_str());
        const char *const *dt = english() ? days : dias;
        status_.day = std::string(dt[wd]) + " " + std::to_string(mday);
        const char *const *mt = english() ? months : meses;
        status_.month = mt[(mon + 11) % 12];
    }
    home.set_status(status_);
    messages_screen.set_status(status_);
    if (settings_.msg_keep_days && status_.epoch && status_.epoch - last_purge_ >= 600) {
        last_purge_ = status_.epoch;
        purge_old_messages();
    }
    // automático: oscuro de noche, en un momento tranquilo (no a medio escribir)
    if (settings_.theme == 2 && wants_dark() != g_dark && !look_pending_ && !ui_busy() &&
        (current_ == ScreenId::Home || lv_display_get_inactive_time(nullptr) > 60000))
        apply_look();
}

void App::set_team(const std::vector<TeamNode> &team)
{
    // "mi equipo" lo deciden los ajustes (se guardan), no el adaptador: un nodo nuevo
    // que aparece en la red no entra solo al equipo
    team_ = team;
    for (auto &n : team_)
        n.in_team = std::find(settings_.team.begin(), settings_.team.end(), n.num) != settings_.team.end();
    std::vector<TeamNode> mine;
    for (auto &n : team_)
        if (n.in_team)
            mine.push_back(n);
    home.set_team(mine);
    // chats directos restaurados antes de conocer el nodo ("!123"): su nombre y color
    bool renamed = false;
    for (auto &ch : chats_)
        if (!ch.is_channel && !ch.name.empty() && ch.name[0] == '!' && ch.id.rfind("node:", 0) == 0)
            for (auto &n : team_)
                if ("node:" + std::to_string(n.num) == ch.id && !n.name.empty()) {
                    ch.name = n.name;
                    ch.color = n.color;
                    renamed = true;
                }
    if (renamed)
        publish_chats();
    messages_screen.refresh_meta();
    map_screen.set_team(team_);
    team_screen.set_nodes(team_);
    // con 250 nodos llega telemetría seguido: los chats solo se reordenan si cambió el equipo
    std::vector<uint32_t> members;
    for (auto &n : mine)
        members.push_back(n.num);
    std::sort(members.begin(), members.end());
    if (members != members_) {
        members_ = members;
        publish_chats(); // el orden "equipo primero" depende de quién está en el equipo
    }
    if (current_ == ScreenId::Settings)
        settings_screen.refresh();
}

void App::set_in_team(uint32_t num, bool in)
{
    Settings s = settings_;
    auto it = std::find(s.team.begin(), s.team.end(), num);
    if (in && it == s.team.end())
        s.team.push_back(num);
    if (!in && it != s.team.end())
        s.team.erase(it);
    settings_.team = s.team; // primero, para que set_team ya lo vea
    std::vector<TeamNode> copy = team_;
    set_team(copy);
    update_settings(s); // se guarda
    if (on_team_changed) {
        std::vector<uint32_t> ids;
        for (auto &n : team_)
            if (n.in_team)
                ids.push_back(n.num);
        on_team_changed(ids);
    }
}

void App::show_node(uint32_t num) { go(View{ScreenId::Team, num, ""}, true, false); }

void App::show_node_on_map(uint32_t num)
{
    show(ScreenId::Map);
    map_screen.focus_node(num);
}

void App::navigate_to_node(uint32_t num)
{
    show(ScreenId::Map);
    map_screen.nav_to_node(num);
}

std::string App::direct_chat_id(uint32_t num, bool create)
{
    std::string name;
    uint32_t color = 0;
    for (auto &n : team_)
        if (n.num == num)
            name = n.name, color = n.color;
    std::string id = "node:" + std::to_string(num);
    for (auto &ch : chats_)
        if (!ch.is_channel && (ch.id == id || (!name.empty() && ch.name == name)))
            return ch.id;
    if (!create)
        return "";
    Chat ch; // primer mensaje con este nodo: se crea el chat
    ch.id = id;
    ch.name = name.empty() ? "!" + std::to_string(num) : name;
    ch.color = color;
    chats_.insert(chats_.begin(), ch);
    publish_chats();
    return id;
}

void App::open_direct_chat(uint32_t num)
{
    force_chat_ = direct_chat_id(num, true);
    show(ScreenId::Messages);
}

static std::string node_name(const std::vector<TeamNode> &t, uint32_t num)
{
    for (auto &n : t)
        if (n.num == num)
            return n.name;
    return tr("nodo");
}

void App::request_position(uint32_t num)
{
    team_screen.set_status(num, tr("Pidiendo su posici\xC3\xB3n a ") + node_name(team_, num) + "\xE2\x80\xA6", col::INK2);
    if (on_request_position)
        on_request_position(num);
}

void App::position_received(uint32_t num)
{
    team_screen.set_status(num, tr("Posici\xC3\xB3n recibida a las ") + now_hhmm(), col::GREEN);
    auto it = std::find(pos_wait_.begin(), pos_wait_.end(), num);
    if (it == pos_wait_.end())
        return;
    pos_wait_.erase(it);
    size_t got = pos_total_ - pos_wait_.size();
    if (pos_wait_.empty()) {
        team_screen.set_note(pos_total_ == 1 ? tr("Lleg\xC3\xB3 la posici\xC3\xB3n \xC2\xB7 ") + now_hhmm()
                                             : tr("Llegaron las ") + std::to_string(pos_total_) + tr(" posiciones \xC2\xB7 ") + now_hhmm(),
                             col::GREEN);
        return;
    }
    std::string missing;
    for (uint32_t n : pos_wait_)
        missing += (missing.empty() ? "" : ", ") + node_name(team_, n);
    team_screen.set_note(std::to_string(got) + tr(" de ") + std::to_string(pos_total_) + tr(" \xC2\xB7 faltan ") + missing, col::INK);
}

std::vector<const TeamNode *> App::team_people() const
{
    std::vector<const TeamNode *> out;
    for (auto &n : team_)
        if (n.in_team && !n.relay)
            out.push_back(&n);
    return out;
}

int App::request_team_positions()
{
    pos_wait_.clear();
    for (auto *n : team_people()) {
        pos_wait_.push_back(n->num);
        if (on_request_position)
            on_request_position(n->num);
    }
    pos_total_ = pos_wait_.size();
    if (pos_total_)
        team_screen.set_note(tr("Pidiendo su posici\xC3\xB3n a ") + std::to_string(pos_total_) + (pos_total_ == 1 ? tr(" nodo\xE2\x80\xA6") : tr(" nodos\xE2\x80\xA6")),
                             col::INK2);
    return (int)pos_total_;
}

void App::traceroute(uint32_t num)
{
    team_screen.set_status(num, tr("Consultando la ruta de se\xC3\xB1" "al\xE2\x80\xA6"), col::INK2);
    if (on_traceroute)
        on_traceroute(num);
}

void App::traceroute_result(uint32_t num, const std::string &route)
{
    team_screen.set_status(num, tr("Ruta: ") + route, col::INK);
}

void App::add_trail_point(uint32_t num, const TrackPoint &p)
{
    auto &t = trails_[num];
    if (!t.empty() && haversine_m(t.back().lat, t.back().lon, p.lat, p.lon) < 5)
        return;
    bool mine = false;
    for (auto &n : team_)
        if (n.num == num)
            mine = n.in_team;
    // memoria acotada: con 250 nodos en la red, los ajenos guardan poco, y la capacidad es la
    // justa (un vector que crece al doble reservaría hasta 60 % más con cientos de rastros)
    size_t keep = mine ? TRAIL_TEAM_POINTS : TRAIL_OTHER_POINTS;
    if (t.capacity() != keep + 1) {
        if (t.size() > keep)
            t.erase(t.begin(), t.begin() + (t.size() - keep));
        std::vector<TrackPoint> fit;
        fit.reserve(keep + 1);
        fit.assign(t.begin(), t.end());
        t.swap(fit);
    }
    t.push_back(p);
    if (t.size() > keep)
        t.erase(t.begin(), t.begin() + (t.size() - keep));
}

const std::vector<TrackPoint> &App::trail(uint32_t num) { return trails_[num]; }

void App::set_position(const Position &p)
{
    position_ = p;
    map_screen.set_position(p);
}

void App::set_waypoints(const std::vector<Waypoint> &w)
{
    waypoints_ = w;
    map_screen.set_waypoints(w);
}

void App::set_track(const Track &t)
{
    track_ = t;
    restat_track();
    map_screen.set_track(t);
    home.set_recording(t.recording);
}

void App::restat_track()
{
    double d = 0, g = 0;
    float ref = track_.pts.empty() ? 0 : track_.pts[0].alt;
    for (size_t i = 1; i < track_.pts.size(); i++) {
        d += haversine_m(track_.pts[i - 1].lat, track_.pts[i - 1].lon, track_.pts[i].lat, track_.pts[i].lon);
        climb_add(g, ref, track_.pts[i].alt);
    }
    dist_acc_ = d, gain_acc_ = g, alt_ref_ = ref;
    if (track_.pts.size() >= 2) { // los del adaptador, si los trae, valen (p. ej. con barómetro)
        if (!track_.distance_m)
            track_.distance_m = (int32_t)std::lround(d);
        if (!track_.gain_m)
            track_.gain_m = (int32_t)std::lround(g);
    }
}

void App::set_map_layers(const std::vector<TileSource *> &layers) { map_screen.set_layers(layers); }

void App::refresh_input_group()
{
    lv_group_t *g = active_group();
    for (lv_indev_t *in = lv_indev_get_next(nullptr); in; in = lv_indev_get_next(in)) {
        if (lv_indev_get_type(in) != LV_INDEV_TYPE_KEYPAD && lv_indev_get_type(in) != LV_INDEV_TYPE_ENCODER)
            continue;
        // Una pantalla que aparece con la tecla aún apretada (SOS al mantener la bola 2 s):
        // el "soltar" no debe caer sobre lo que quedó enfocado en la pantalla nueva ("Volver").
        if (lv_indev_get_group(in) != g && lv_indev_get_state(in) == LV_INDEV_STATE_PRESSED)
            lv_indev_wait_release(in);
        lv_indev_set_group(in, g);
    }
}

// La ruta en curso vive en memoria: si la batería se acaba, se perdería. Se respalda en la
// tarjeta SD de a poco (agregando al final, sin reescribir), y al arrancar se recupera.
void App::flush_backup()
{
    if (backup_buf_.empty())
        return;
    if (on_append_file && on_append_file(TRACK_BACKUP, backup_buf_))
        backup_buf_.clear();
    else if (backup_buf_.size() > 16384)
        backup_buf_.clear(); // sin SD: no crecer sin límite
}

static std::string backup_line(const TrackPoint &p)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "%.7f,%.7f,%.1f,%u\n", p.lat, p.lon, p.alt, (unsigned)p.epoch);
    return buf;
}

bool App::recover_track(const std::string &csv)
{
    Track t;
    size_t pos = 0;
    while (pos < csv.size()) {
        size_t end = csv.find('\n', pos);
        if (end == std::string::npos)
            end = csv.size();
        std::string line = csv.substr(pos, end - pos);
        pos = end + 1;
        TrackPoint p{};
        unsigned ep = 0;
        if (line.empty() || line[0] == '#' || std::sscanf(line.c_str(), "%lf,%lf,%f,%u", &p.lat, &p.lon, &p.alt, &ep) != 4)
            continue; // línea cortada por el apagón: se ignora
        if (std::fabs(p.lat) > 90 || std::fabs(p.lon) > 180)
            continue;
        p.epoch = ep;
        t.pts.push_back(p);
    }
    if (t.pts.size() < 2)
        return false;
    track_totals(t.pts, t.distance_m, t.gain_m);
    if (t.pts.back().epoch > t.pts.front().epoch)
        t.duration_s = t.pts.back().epoch - t.pts.front().epoch;
    t.recording = false;
    track_ = t;
    restat_track();
    recovered_ = true;
    map_screen.set_track(track_);
    home.set_recording(track_.recording);
    return true;
}

bool App::take_recovered()
{
    bool r = recovered_;
    recovered_ = false;
    return r;
}

void App::start_recording()
{
    track_ = Track{};
    track_.recording = true;
    dist_acc_ = gain_acc_ = 0;
    alt_ref_ = (float)position_.alt;
    backup_buf_.clear();
    if (on_save_file)
        on_save_file(TRACK_BACKUP, tr("# T-Field UI \xC2\xB7 ruta en curso (lat,lon,alt,epoch)\n"));
    if (position_.valid) {
        track_.pts.push_back({position_.lat, position_.lon, (float)position_.alt, status_.epoch});
        backup_buf_ += backup_line(track_.pts.back());
    }
    map_screen.set_track(track_);
    home.set_recording(track_.recording);
    if (on_recording)
        on_recording(true);
}

void App::stop_recording()
{
    track_.recording = false;
    flush_backup();
    map_screen.set_track(track_);
    home.set_recording(track_.recording);
    if (on_recording)
        on_recording(false);
}

void App::resume_recording()
{
    track_.recording = true;
    map_screen.set_track(track_);
    home.set_recording(track_.recording);
    if (on_recording)
        on_recording(true);
}

void App::add_track_point(const TrackPoint &p)
{
    if (!track_.recording)
        return;
    if (!track_.pts.empty()) {
        const TrackPoint &q = track_.pts.back();
        double d = haversine_m(q.lat, q.lon, p.lat, p.lon);
        if (d < 3) // ignora el ruido del GPS cuando estás quieto
            return;
        if (p.epoch && q.epoch && p.epoch > q.epoch && p.epoch - q.epoch < settings_.track_every_s)
            return; // Ajustes › GPS: un punto cada N segundos
        dist_acc_ += d;
        climb_add(gain_acc_, alt_ref_, p.alt);
        track_.distance_m = (int32_t)std::lround(dist_acc_);
        track_.gain_m = (int32_t)std::lround(gain_acc_);
        if (p.epoch > track_.pts.front().epoch)
            track_.duration_s = p.epoch - track_.pts.front().epoch;
    }
    if (track_.pts.empty())
        alt_ref_ = p.alt;
    track_.pts.push_back(p);
    backup_buf_ += backup_line(p);
    if (backup_buf_.size() >= 6 * 40) // cada ~6 puntos (1 min a 10 s): poco que perder, pocas escrituras
        flush_backup();
    map_screen.set_track(track_);
    home.set_recording(track_.recording);
}

// archivo para una ruta, distinto del de cualquier otra ruta guardada
std::string App::track_file(const std::string &name, uint32_t id) const
{
    std::string base = tr("/tfield/rutas/") + safe_filename(name);
    std::string file = base + tr(".gpx");
    for (int n = 2; n < 1000; n++) {
        bool taken = false;
        for (auto &o : saved_tracks_)
            taken |= o.id != id && o.file == file;
        if (!taken)
            break;
        file = base + "_" + std::to_string(n) + tr(".gpx");
    }
    return file;
}

// Escribe el GPX por partes (una ruta de 12 h son ~500 KB de texto): la memoria no pasa de
// unos 8 KB. Si falla a medias, se borra lo escrito.
bool App::write_track_file(const std::string &file, const std::string &name, const std::string &note, const Track &t)
{
    if (!on_save_file)
        return true;
    if (!on_append_file)
        return on_save_file(file, gpx_track(name, note, t));
    if (!on_save_file(file, gpx_track_head(name, note, t)))
        return false;
    std::string chunk;
    chunk.reserve(8400);
    for (size_t i = 0; i < t.pts.size(); i++) {
        gpx_track_point(chunk, t.pts[i]);
        if (chunk.size() >= 8000 || i + 1 == t.pts.size()) {
            if (!on_append_file(file, chunk)) {
                if (on_delete_file)
                    on_delete_file(file);
                return false;
            }
            chunk.clear();
        }
    }
    if (!on_append_file(file, gpx_track_tail())) {
        if (on_delete_file)
            on_delete_file(file);
        return false;
    }
    return true;
}

// El índice se alterna entre dos archivos: si se corta la energía mientras se escribe uno,
// el otro sigue entero (con la lista anterior)
void App::save_route_index()
{
    if (!on_save_file)
        return;
    route_gen_++;
    on_save_file(route_gen_ % 2 ? ROUTE_INDEX_B : ROUTE_INDEX_A, route_index(saved_tracks_, route_gen_));
}

bool App::load_routes()
{
    if (!on_read_file)
        return false;
    std::vector<SavedTrack> best;
    uint32_t best_gen = 0;
    bool found = false;
    for (const char *f : {ROUTE_INDEX_A, ROUTE_INDEX_B}) {
        std::string text;
        std::vector<SavedTrack> list;
        uint32_t gen = 0;
        if (on_read_file(f, text) && parse_route_index(text, list, gen) && (!found || gen > best_gen)) {
            best.swap(list);
            best_gen = gen;
            found = true;
        }
    }
    if (!found)
        return false;
    route_gen_ = best_gen;
    saved_tracks_.swap(best);
    loaded_route_ = 0;
    loaded_pts_.clear();
    for (auto &x : saved_tracks_)
        next_track_id_ = std::max(next_track_id_, x.id + 1);
    map_screen.set_saved_tracks(saved_tracks_);
    return true;
}

const std::vector<TrackPoint> *App::route_points(uint32_t id)
{
    if (loaded_route_ == id && !loaded_pts_.empty())
        return &loaded_pts_;
    for (auto &t : saved_tracks_) {
        if (t.id != id)
            continue;
        std::string gpx;
        std::vector<TrackPoint> pts;
        if (!on_read_file || !on_read_file(t.file, gpx) || !gpx_parse_track(gpx, pts) || pts.size() < 2)
            return nullptr;
        loaded_pts_.swap(pts); // la anterior se suelta
        loaded_route_ = id;
        return &loaded_pts_;
    }
    return nullptr;
}

std::string App::add_saved_track(const std::string &name, const std::string &note, const Track &t)
{
    SavedTrack st;
    st.id = next_track_id_;
    st.name = name.empty() ? tr("R") + std::to_string(st.id) : name;
    st.note = note;
    st.track.distance_m = t.distance_m;
    st.track.duration_s = t.duration_s;
    st.track.gain_m = t.gain_m;
    st.file = track_file(st.name, st.id);
    if (!write_track_file(st.file, st.name, st.note, t))
        return ""; // sin tarjeta SD (o llena)
    next_track_id_++;
    route_meta(st, t.pts);
    saved_tracks_.push_back(st);
    save_route_index();
    map_screen.set_saved_tracks(saved_tracks_);
    return st.file;
}

std::string App::save_track(const std::string &name, const std::string &note)
{
    Track t = track_;
    t.recording = false;
    std::string file = add_saved_track(name, note, t);
    if (file.empty())
        return ""; // la ruta sigue en memoria para reintentar
    track_ = Track{};
    backup_buf_.clear();
    if (on_delete_file)
        on_delete_file(TRACK_BACKUP); // el respaldo ya no hace falta
    map_screen.set_track(track_);
    home.set_recording(track_.recording);
    return file;
}

void App::set_saved_tracks(const std::vector<SavedTrack> &t)
{
    saved_tracks_ = t;
    for (auto &x : saved_tracks_) {
        if (!x.track.pts.empty()) {
            route_meta(x, x.track.pts);
            std::vector<TrackPoint>().swap(x.track.pts); // en la SD, no en memoria
        }
        next_track_id_ = std::max(next_track_id_, x.id + 1);
    }
    if (loaded_route_) {
        bool still = false;
        for (auto &x : saved_tracks_)
            still |= x.id == loaded_route_;
        if (!still)
            loaded_route_ = 0, loaded_pts_.clear();
    }
    map_screen.set_saved_tracks(saved_tracks_);
}

void App::delete_track(uint32_t id)
{
    for (auto it = saved_tracks_.begin(); it != saved_tracks_.end(); ++it)
        if (it->id == id) {
            if (on_delete_file)
                on_delete_file(it->file);
            saved_tracks_.erase(it);
            break;
        }
    if (loaded_route_ == id)
        loaded_route_ = 0, loaded_pts_.clear();
    save_route_index();
    map_screen.set_saved_tracks(saved_tracks_);
}

// Cambiar nombre o nota: se reescribe el GPX desde el archivo (con todos sus puntos), no desde
// la memoria
bool App::update_track(uint32_t id, const std::string &name, const std::string &note)
{
    bool ok = true;
    for (auto &t : saved_tracks_) {
        if (t.id != id)
            continue;
        std::string n = name.empty() ? tr("R") + std::to_string(id) : name;
        if (n == t.name && note == t.note)
            return true;
        std::string gpx;
        if (!on_read_file || !on_read_file(t.file, gpx)) {
            ok = false;
            break;
        }
        if (!gpx_rename(gpx, n, note)) { // GPX de otra forma: se reescribe con sus puntos
            Track tr;
            if (!gpx_parse_track(gpx, tr.pts)) {
                ok = false;
                break;
            }
            gpx = gpx_track(n, note, tr);
        }
        std::string file = track_file(n, id);
        // primero escribir el nuevo; el anterior se borra solo si eso funcionó
        if (on_save_file && !on_save_file(file, gpx)) {
            ok = false;
            break;
        }
        if (file != t.file && on_delete_file)
            on_delete_file(t.file);
        t.name = n;
        t.note = note;
        t.file = file;
        save_route_index();
    }
    map_screen.set_saved_tracks(saved_tracks_);
    return ok;
}

std::string App::local_hhmm(uint32_t epoch) const
{
    std::string t = local_date_time(epoch);
    return t.substr(t.rfind(' ') + 1);
}

std::string App::local_date_time(uint32_t epoch) const
{
    static const char *months[] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"};
    static const char *months_en[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    // diferencia horaria a partir de la hora local que muestra el reloj
    int32_t local_s = status_.hour * 3600 + status_.minute * 60;
    int32_t utc_s = (int32_t)(status_.epoch % 86400);
    int32_t off = local_s - utc_s;
    if (off > 14 * 3600)
        off -= 86400;
    if (off < -12 * 3600)
        off += 86400;
    off = (int32_t)std::lround(off / 900.0) * 900; // husos de 15 min (los segundos no corren el huso)
    std::string iso = iso8601((uint32_t)((int64_t)epoch + off)); // "2026-11-20T14:32:00Z"
    int mon = std::atoi(iso.substr(5, 2).c_str());
    // un puntero, no "(c ? a : b)[i]": GCC 13 con UBSan compila mal ese subíndice sobre arreglos
    const char *const *mt = english() ? months_en : months;
    return std::to_string(std::atoi(iso.substr(8, 2).c_str())) + " " + mt[(mon + 11) % 12] + " " + iso.substr(11, 5);
}

void App::discard_track()
{
    track_ = Track{};
    backup_buf_.clear();
    if (on_delete_file)
        on_delete_file(TRACK_BACKUP);
    map_screen.set_track(track_);
    home.set_recording(track_.recording);
}

std::string App::next_point_name() const
{
    uint32_t id = 1;
    for (auto &w : waypoints_)
        id = std::max(id, w.id + 1);
    return tr("P") + std::to_string(id);
}

Waypoint App::draft_point() const
{
    Waypoint w;
    uint32_t id = 1;
    for (auto &x : waypoints_)
        id = std::max(id, x.id + 1);
    w.id = id;
    w.name = tr("P") + std::to_string(id);
    w.time = now_hhmm();
    w.epoch = status_.epoch;
    w.lat = position_.lat;
    w.lon = position_.lon;
    w.alt = position_.alt;
    return w;
}

bool App::export_waypoints()
{
    map_screen.set_waypoints(waypoints_);
    if (on_save_file && !on_save_file(WAYPOINTS_FILE, gpx_waypoints(waypoints_))) {
        map_screen.toast(tr("No se pudo escribir en la tarjeta SD: puntos sin guardar"));
        return false;
    }
    return true;
}

bool App::load_waypoints()
{
    std::string gpx;
    std::vector<Waypoint> w;
    if (!on_read_file || !on_read_file(WAYPOINTS_FILE, gpx) || !gpx_parse_waypoints(gpx, w))
        return false;
    for (auto &x : w)
        if (x.epoch)
            x.time = local_hhmm(x.epoch);
    waypoints_ = w;
    map_screen.set_waypoints(waypoints_);
    return true;
}

void App::add_waypoint(const Waypoint &w)
{
    waypoints_.push_back(w);
    export_waypoints();
}

void App::update_waypoint(uint32_t id, const std::string &name, const std::string &note)
{
    for (auto &w : waypoints_)
        if (w.id == id) {
            w.name = name.empty() ? tr("P") + std::to_string(id) : name;
            w.note = note;
        }
    export_waypoints();
}

// ------------------------------------------------------------------ SOS
static constexpr uint32_t SOS_RED = 0xB3261E;
static constexpr uint32_t SOS_REPEAT_MS = 2 * 60 * 1000;

std::string App::sos_text() const
{
    char buf[160];
    if (position_.valid) {
        Utm u = to_utm(position_.lat, position_.lon);
        snprintf(buf, sizeof(buf), tr("SOS %s %d%c %ld %ld \xC2\xB1%um %dm %02u:%02u"), status_.short_name.c_str(), u.zone, u.band,
                 std::lround(u.e), std::lround(u.n), position_.acc_m, (int)position_.alt, status_.hour, status_.minute);
    } else
        snprintf(buf, sizeof(buf), tr("SOS %s sin posici\xC3\xB3n GPS %02u:%02u"), status_.short_name.c_str(), status_.hour,
                 status_.minute);
    return buf;
}

void App::build_sos()
{
    lv_obj_t *top = lv_layer_top();
    sos_group_ = lv_group_create();

    sos_banner_ = ui::box(top, 0, 0, SCREEN_W, 16, SOS_RED);
    lv_obj_add_flag(sos_banner_, LV_OBJ_FLAG_CLICKABLE);
    sos_banner_lbl_ = ui::label(sos_banner_, &tfield_font_12b, col::SURF, "");
    ui::align(sos_banner_lbl_, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_add_event_cb(
        sos_banner_, [](lv_event_t *e) { static_cast<App *>(lv_event_get_user_data(e))->show_sos(true); },
        LV_EVENT_CLICKED, this);
    ui::show(sos_banner_, false);

    sos_overlay_ = ui::box(top, 0, 0, SCREEN_W, SCREEN_H, SOS_RED);
    lv_obj_add_flag(sos_overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(ui::label(sos_overlay_, &tfield_font_24b, col::SURF, tr("SOS ACTIVO")), 12, 8);
    sos_info_ = ui::label(sos_overlay_, &tfield_font_14b, col::SURF, "");
    lv_obj_set_pos(sos_info_, 12, 42);
    lv_obj_set_width(sos_info_, SCREEN_W - 24);
    lv_label_set_long_mode(sos_info_, LV_LABEL_LONG_MODE_WRAP);
    sos_count_ = ui::label(sos_overlay_, &tfield_font_12, col::SURF, "");
    lv_obj_set_pos(sos_count_, 12, 86);
    lv_obj_set_width(sos_count_, SCREEN_W - 24);
    lv_label_set_long_mode(sos_count_, LV_LABEL_LONG_MODE_WRAP);
    // confirmaciones de los envíos directos al equipo
    sos_direct_ = ui::label(sos_overlay_, &tfield_font_12b, col::SURF, "");
    lv_obj_set_pos(sos_direct_, 12, 140);
    lv_obj_set_width(sos_direct_, SCREEN_W - 24);
    lv_label_set_long_mode(sos_direct_, LV_LABEL_LONG_MODE_WRAP);

    lv_obj_t *back = ui::button(sos_overlay_, 12, 196, 84, 32, SOS_RED, col::SURF, sos_group_);
    lv_obj_set_style_border_width(back, 2, 0);
    lv_obj_set_style_outline_color(back, c(col::SURF), LV_STATE_FOCUS_KEY);
    lv_obj_center(ui::label(back, &tfield_font_12b, col::SURF, tr("Volver")));
    lv_obj_add_event_cb(
        back, [](lv_event_t *e) { static_cast<App *>(lv_event_get_user_data(e))->show_sos(false); }, LV_EVENT_CLICKED, this);
    lv_obj_t *cancel = ui::hold_button(
        sos_overlay_, 104, 196, 204, 32, col::SURF, 0xF2C4C0, SOS_RED, tr("Mant\xC3\xA9n para cancelar SOS"), 2000,
        [](void *u) { static_cast<App *>(u)->cancel_sos(); }, this, sos_group_);
    lv_obj_set_style_outline_color(cancel, c(col::SURF), LV_STATE_FOCUS_KEY);
    ui::show(sos_overlay_, false);
}

static std::string join_names(const std::vector<std::string> &v)
{
    std::string out;
    for (size_t i = 0; i < v.size(); i++)
        out += (i == 0 ? "" : (i + 1 == v.size() ? tr(" y ") : ", ")) + v[i];
    return out;
}

void App::show_sos(bool overlay)
{
    if (!sos_active_) {
        ui::show(sos_overlay_, false);
        ui::show(sos_banner_, false);
        refresh_input_group();
        return;
    }
    char buf[240];
    std::string when = local_hhmm(sos_since_);
    std::vector<std::string> names;
    for (auto &id : sos_chats_) {
        if (id == "@primary")
            names.push_back(tr("el canal principal"));
        for (auto &ch : chats_)
            if (ch.id == id)
                names.push_back(ch.name);
    }
    std::string where = names.empty() ? tr("tu equipo") : join_names(names);
    snprintf(buf, sizeof(buf), tr("Enviado a %s \xC2\xB7 %s\n%s"), where.c_str(), when.c_str(),
             position_.valid ? format_utm(to_utm(position_.lat, position_.lon)).c_str() : tr("Sin posici\xC3\xB3n GPS"));
    ui::set_text(sos_info_, buf);
    unsigned every = settings_.sos_every_min;
    snprintf(buf, sizeof(buf), tr("Se repite cada %u %s \xC2\xB7 %u %s.\nMensaje: %s"), every, every == 1 ? tr("minuto") : tr("minutos"), sos_sent_,
             sos_sent_ == 1 ? tr("env\xC3\xADo") : tr("env\xC3\xAD" "os"), sos_text().c_str());
    ui::set_text(sos_count_, buf);
    // confirmaciones del equipo
    std::vector<std::string> ok, wait;
    for (auto &a : sos_acks_)
        (a.second ? ok : wait).push_back(node_name(team_, a.first));
    if (sos_acks_.empty())
        ui::set_text(sos_direct_, "");
    else if (wait.empty())
        ui::set_text(sos_direct_, tr("Todo tu equipo lo recibi\xC3\xB3: ") + join_names(ok));
    else if (ok.empty())
        ui::set_text(sos_direct_, tr("Directo a tu equipo \xC2\xB7 esperando confirmaci\xC3\xB3n de ") + join_names(wait));
    else
        ui::set_text(sos_direct_, tr("Lo recibieron: ") + join_names(ok) + tr("\nEsperando: ") + join_names(wait));
    unsigned acked = 0;
    for (auto &a : sos_acks_)
        acked += a.second ? 1 : 0;
    if (sos_acks_.empty())
        snprintf(buf, sizeof(buf), tr("SOS activo \xC2\xB7 %u %s \xC2\xB7 toca para ver"), sos_sent_, sos_sent_ == 1 ? tr("env\xC3\xADo") : tr("env\xC3\xAD" "os"));
    else
        snprintf(buf, sizeof(buf), tr("SOS activo \xC2\xB7 %u de %u lo recibieron \xC2\xB7 toca para ver"), acked, (unsigned)sos_acks_.size());
    ui::set_text(sos_banner_lbl_, buf);
    if (overlay) {
        ui::show(sos_overlay_, true);
        ui::show(sos_banner_, false);
    } else {
        ui::show(sos_overlay_, false);
        ui::show(sos_banner_, true);
    }
    refresh_input_group();
}

void App::sos_targets(std::vector<std::string> &chats, std::vector<uint32_t> &direct) const
{
    std::string primary;
    for (auto &ch : chats_)
        if (ch.is_channel && ch.channel == 0)
            primary = ch.id;
    if (primary.empty()) // el adaptador no dijo cuál es el principal: el primer canal que no sea el del equipo
        for (auto &ch : chats_)
            if (primary.empty() && ch.is_channel && !(settings_.channel.on && ch.id == settings_.channel.chat_id))
                primary = ch.id;
    if (settings_.sos_public && !primary.empty())
        chats.push_back(primary);
    if (settings_.sos_team_ch && settings_.channel.on && !settings_.channel.chat_id.empty())
        chats.push_back(settings_.channel.chat_id);
    if (settings_.sos_direct)
        for (auto *n : team_people())
            direct.push_back(n->num);
    // nunca un SOS sin destino: si los ajustes lo dejan vacío, va por el canal principal
    if (chats.empty() && direct.empty())
        chats.push_back(primary.empty() ? "@primary" : primary); // "@primary": canal 0, aunque no haya chat
}

void App::trigger_sos()
{
    if (sos_active_) {
        show_sos(true);
        return;
    }
    sos_active_ = true;
    sos_since_ = status_.epoch;
    sos_sent_ = 1;
    sos_chats_.clear();
    std::vector<uint32_t> direct;
    sos_targets(sos_chats_, direct);
    sos_acks_.clear();
    for (uint32_t n : direct)
        sos_acks_.push_back({n, 0});
    if (on_sos)
        on_sos(true, sos_text(), sos_chats_, direct);
    sos_timer_ = lv_timer_create(
        [](lv_timer_t *t) {
            auto *a = static_cast<App *>(lv_timer_get_user_data(t));
            a->sos_sent_++;
            std::vector<uint32_t> pending; // directo: solo a quienes no han confirmado
            for (auto &x : a->sos_acks_)
                if (!x.second)
                    pending.push_back(x.first);
            if (a->on_sos)
                a->on_sos(true, a->sos_text(), a->sos_chats_, pending);
            a->show_sos(!lv_obj_has_flag(a->sos_overlay_, LV_OBJ_FLAG_HIDDEN));
        },
        (uint32_t)settings_.sos_every_min * 60u * 1000u, this);
    show_sos(true);
}

void App::sos_ack(uint32_t num)
{
    if (!sos_active_)
        return;
    for (auto &a : sos_acks_)
        if (a.first == num && !a.second)
            a.second = status_.epoch ? status_.epoch : 1;
    show_sos(!lv_obj_has_flag(sos_overlay_, LV_OBJ_FLAG_HIDDEN));
}

void App::cancel_sos()
{
    if (!sos_active_)
        return;
    sos_active_ = false;
    if (sos_timer_) {
        lv_timer_delete(sos_timer_);
        sos_timer_ = nullptr;
    }
    std::vector<uint32_t> direct;
    for (auto &a : sos_acks_)
        direct.push_back(a.first);
    if (on_sos)
        on_sos(false, tr("SOS cancelado ") + status_.short_name + " " + now_hhmm(), sos_chats_, direct);
    sos_acks_.clear();
    show_sos(false);
}

std::vector<Recipient> App::share_recipients() const
{
    std::vector<Recipient> out;
    // 1) nodos: primero los de mi equipo, luego el resto; por distancia (sin repetidores)
    std::vector<const TeamNode *> nodes;
    for (auto &n : team_)
        if (!n.relay)
            nodes.push_back(&n);
    std::stable_sort(nodes.begin(), nodes.end(), [](const TeamNode *a, const TeamNode *b) {
        if (a->in_team != b->in_team)
            return a->in_team;
        return (a->dist_m < 0 ? INT32_MAX : a->dist_m) < (b->dist_m < 0 ? INT32_MAX : b->dist_m);
    });
    // con 250 nodos: todo el equipo y solo los ajenos más cercanos
    size_t others = 0;
    for (auto *n : nodes) {
        if (!n->in_team && others++ >= SHARE_OTHERS)
            break;
        std::string id = "node:" + std::to_string(n->num);
        for (auto &ch : chats_)
            if (!ch.is_channel && ch.name == n->name)
                id = ch.id;
        std::string detail = n->in_team ? tr("directo") : tr("directo \xC2\xB7 fuera del equipo");
        if (n->dist_m >= 0)
            detail += " \xC2\xB7 " + format_distance(n->dist_m) + " " + cardinal(n->bearing_deg);
        detail += " \xC2\xB7 " + format_ago(n->heard_ago_s);
        Recipient r{id, n->name, false, n->num, n->in_team ? n->color : col::MUTED, detail};
        r.team = n->in_team;
        out.push_back(r);
    }
    // 2) chats directos con nodos que no están en la lista
    for (auto &ch : chats_) {
        if (ch.is_channel)
            continue;
        bool found = false;
        for (auto &r : out)
            found |= r.id == ch.id;
        if (!found)
            out.push_back({ch.id, ch.name, false, 0, ch.color, tr("directo")});
    }
    // 3) grupos (canales): el canal privado del equipo primero
    for (int pass = 0; pass < 2; pass++)
        for (auto &ch : chats_) {
            if (!ch.is_channel)
                continue;
            bool mine = settings_.channel.on && ch.id == settings_.channel.chat_id;
            if (mine != (pass == 0))
                continue;
            out.push_back({ch.id, ch.name, true, 0, mine ? col::PURPLE : 0,
                           mine ? tr("grupo \xC2\xB7 canal privado de tu equipo")
                                : (ch.channel == 0 ? tr("grupo \xC2\xB7 canal p\xC3\xBA" "blico, todos los nodos") : tr("grupo \xC2\xB7 todos los nodos del canal"))});
        }
    return out;
}

void App::show_waypoint(uint32_t id)
{
    show(ScreenId::Map);
    map_screen.focus_waypoint(id);
}

std::string App::share_waypoint(uint32_t id, const std::vector<Recipient> &to)
{
    const Waypoint *w = nullptr;
    for (auto &x : waypoints_)
        if (x.id == id)
            w = &x;
    if (!w || to.empty())
        return "";
    std::string names;
    last_share_.clear();
    Waypoint copy = *w;
    for (auto &r : to) {
        if (on_share_waypoint)
            on_share_waypoint(copy, r);
        last_share_.push_back(r.id);
        names += (names.empty() ? "" : ", ") + r.name;
        // queda registrado en el chat correspondiente
        bool has_chat = false;
        for (auto &ch : chats_)
            has_chat |= ch.id == r.id;
        if (has_chat) {
            Message m;
            m.mine = true;
            m.from = tr("T\xC3\xBA");
            m.time = now_hhmm();
            m.text = tr("Compartiste el punto \xC2\xAB") + copy.name + "\xC2\xBB";
            m.status = "Enviado"; // se traduce al dibujar
            m.waypoint_id = copy.id;
            add_message(r.id, m);
        }
    }
    return names;
}

void App::receive_waypoint(const Waypoint &in, const std::string &from_chat_id)
{
    Waypoint w = in;
    bool team = false;
    for (auto &n : team_)
        if (n.name == w.from)
            team = n.in_team;
    PointPolicy policy = team ? settings_.pts_team : settings_.pts_others;
    Message m;
    m.from = w.from;
    for (auto &n : team_)
        if (n.name == w.from)
            m.from_color = n.in_team ? n.color : col::MUTED;
    m.time = w.time.empty() ? now_hhmm() : w.time;
    if (policy == PointPolicy::Auto) {
        uint32_t id = 1;
        for (auto &x : waypoints_)
            id = std::max(id, x.id + 1);
        w.id = id;
        waypoints_.push_back(w);
        export_waypoints();
        m.text = tr("Comparti\xC3\xB3 el punto \xC2\xAB") + w.name + "\xC2\xBB";
        m.waypoint_id = w.id;
        map_screen.toast(w.from + tr(" comparti\xC3\xB3 ") + w.name);
    } else if (policy == PointPolicy::Ask) {
        uint32_t pid = next_pending_++;
        pending_wps_[pid] = w;
        m.text = tr("Comparti\xC3\xB3 el punto \xC2\xAB") + w.name + tr("\xC2\xBB \xC2\xB7 \xC2\xBFlo guardas?");
        m.pending_wp = pid;
        map_screen.toast(w.from + tr(" comparti\xC3\xB3 ") + w.name + tr(" \xC2\xB7 en Mensajes"));
    } else {
        m.text = tr("Comparti\xC3\xB3 un punto (no guardado por tus ajustes)");
    }
    add_message(from_chat_id, m);
}

// tras cambiar el texto de un mensaje, la vista previa del chat lo refleja si es el último
static void refresh_previews(std::vector<Chat> &chats, const std::map<std::string, std::vector<Message>> &msgs)
{
    for (auto &ch : chats) {
        auto it = msgs.find(ch.id);
        if (it == msgs.end() || it->second.empty())
            continue;
        const Message &m = it->second.back();
        ch.preview = m.mine ? tr("T\xC3\xBA: ") + m.text : (ch.is_channel ? m.from + ": " + m.text : m.text);
    }
}

uint32_t App::accept_waypoint(uint32_t pid)
{
    auto it = pending_wps_.find(pid);
    if (it == pending_wps_.end())
        return 0;
    Waypoint w = it->second;
    pending_wps_.erase(it);
    uint32_t id = 1;
    for (auto &x : waypoints_)
        id = std::max(id, x.id + 1);
    w.id = id;
    waypoints_.push_back(w);
    export_waypoints();
    for (auto &kv : messages_)
        for (auto &m : kv.second)
            if (m.pending_wp == pid) {
                m.pending_wp = 0;
                m.waypoint_id = id;
                m.text = tr("Comparti\xC3\xB3 el punto \xC2\xAB") + w.name + tr("\xC2\xBB \xC2\xB7 guardado");
                msg_rev_[kv.first]++;
            }
    refresh_previews(chats_, messages_);
    publish_chats();
    messages_screen.refresh_conversation();
    return id;
}

void App::delete_waypoint(uint32_t id)
{
    waypoints_.erase(std::remove_if(waypoints_.begin(), waypoints_.end(), [id](const Waypoint &w) { return w.id == id; }),
                     waypoints_.end());
    export_waypoints();
}

void App::set_chats(const std::vector<Chat> &chats)
{
    chats_ = chats;
    restore_kept_chats();
    publish_chats();
}

// Los chats directos vaciados con Borrar vuelven al arrancar (vacíos, al final)
void App::restore_kept_chats()
{
    for (uint32_t num : settings_.kept_chats) {
        std::string id = "node:" + std::to_string(num);
        bool have = false;
        for (auto &ch : chats_)
            have |= !ch.is_channel && (ch.id == id || chat_num(ch) == num);
        if (have)
            continue;
        Chat ch;
        ch.id = id;
        ch.name = "!" + std::to_string(num);
        for (auto &n : team_)
            if (n.num == num)
                ch.name = n.name, ch.color = n.color;
        chats_.push_back(ch);
    }
}

uint32_t App::chat_num(const Chat &ch) const
{
    if (ch.is_channel)
        return 0;
    if (ch.id.rfind("node:", 0) == 0)
        return (uint32_t)std::strtoul(ch.id.c_str() + 5, nullptr, 10);
    const TeamNode *n = chat_node(ch);
    return n ? n->num : 0;
}

void App::keep_chat(uint32_t num, bool keep)
{
    auto &k = settings_.kept_chats;
    auto it = std::find(k.begin(), k.end(), num);
    if (keep == (it != k.end()))
        return;
    if (keep) {
        k.push_back(num);
        if (k.size() > 32)
            k.erase(k.begin());
    } else
        k.erase(it);
    save_settings_soon();
}

void App::set_messages(const std::string &chat_id, const std::vector<Message> &msgs)
{
    messages_[chat_id] = msgs;
    if (messages_[chat_id].size() > CHAT_KEEP_MESSAGES)
        messages_[chat_id].erase(messages_[chat_id].begin(), messages_[chat_id].end() - CHAT_KEEP_MESSAGES);
    msg_rev_[chat_id]++;
    if (messages_screen.current == chat_id)
        messages_screen.refresh_conversation();
}

void App::add_message(const std::string &in_id, const Message &m)
{
    // un chat que no existe todavía (primer mensaje de un nodo): se crea
    std::string chat_id = in_id;
    bool known = false;
    for (auto &ch : chats_)
        known |= ch.id == chat_id;
    if (!known) {
        if (chat_id.rfind("node:", 0) == 0)
            chat_id = direct_chat_id((uint32_t)std::strtoul(chat_id.c_str() + 5, nullptr, 10), true);
        else {
            Chat ch;
            ch.id = chat_id;
            ch.name = m.mine || m.from.empty() ? chat_id : m.from;
            chats_.insert(chats_.begin(), ch);
        }
    }
    auto &list = messages_[chat_id];
    list.push_back(m);
    if (!list.back().epoch)
        list.back().epoch = status_.epoch; // hora de llegada (para el borrado automático)
    msg_rev_[chat_id]++;
    msg_added_[chat_id]++; // solo se agregó al final: la conversación abierta suma una burbuja
    if (list.size() > CHAT_KEEP_MESSAGES) // memoria acotada: los más antiguos se descartan
        list.erase(list.begin(), list.begin() + (list.size() - CHAT_KEEP_MESSAGES));
    // el chat con actividad reciente sube al primer lugar
    for (size_t i = 1; i < chats_.size(); i++)
        if (chats_[i].id == chat_id)
            std::rotate(chats_.begin(), chats_.begin() + i, chats_.begin() + i + 1);
    // tope total (un canal público concurrido más muchos directos): se descarta lo más viejo
    // del chat con menos actividad reciente
    size_t total = 0;
    for (auto &kv : messages_)
        total += kv.second.size();
    while (total > CHAT_TOTAL_MESSAGES) {
        std::vector<Message> *victim = nullptr;
        std::string vid;
        for (auto it = chats_.rbegin(); it != chats_.rend() && !victim; ++it) {
            auto f = messages_.find(it->id);
            if (it->id != chat_id && f != messages_.end() && !f->second.empty())
                victim = &f->second, vid = it->id;
        }
        for (auto it = messages_.begin(); it != messages_.end() && !victim; ++it)
            if (it->first != chat_id && !it->second.empty())
                victim = &it->second, vid = it->first;
        if (!victim)
            victim = &list, vid = chat_id;
        victim->erase(victim->begin());
        if (victim->size() < victim->capacity() / 2) // la memoria vuelve: no 60 lugares por chat
            victim->shrink_to_fit();
        msg_rev_[vid]++;
        total--;
    }
    for (auto &ch : chats_) {
        if (ch.id == chat_id) {
            ch.time = m.time;
            ch.preview = m.mine ? tr("T\xC3\xBA: ") + m.text : (ch.is_channel ? m.from + ": " + m.text : m.text);
            if (!m.mine && !(current_ == ScreenId::Messages && messages_screen.current == chat_id))
                ch.unread++;
        }
    }
    publish_chats();
}

void App::set_quick(const QuickReplies &q)
{
    quick_ = q;
    // las de fábrica de versiones anteriores, más largas que el límite actual: las nuevas
    static const char *const OLD[][2] = {{"Llego en 15 min", "En 15 minutos"}, {"There in 15 min", "In 15 minutes"}};
    for (auto &t : quick_.text)
        for (auto &o : OLD)
            if (t == o[0])
                t = o[1];
    home.set_quick(quick_);
    messages_screen.refresh_chips();
}

std::string App::now_hhmm() const
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%02u:%02u", status_.hour, status_.minute);
    return buf;
}

void App::send(const std::string &chat_id, const std::string &text)
{
    Message m;
    m.mine = true;
    m.from = tr("T\xC3\xBA");
    m.time = now_hhmm();
    m.text = text;
    m.status = "Enviado"; // se traduce al dibujar
    m.id = ++next_msg_id_;
    add_message(chat_id, m);
    if (on_send_text)
        on_send_text(chat_id, text);
}

void App::set_message_status(uint32_t id, const char *status)
{
    if (!id)
        return;
    for (auto &kv : messages_)
        for (auto &m : kv.second)
            if (m.id == id && m.mine) {
                if (m.status == status)
                    return;
                m.status = status;
                msg_rev_[kv.first]++;
                if (messages_screen.current == kv.first)
                    messages_screen.refresh_conversation(false);
                return;
            }
}

void App::save_quick(const QuickReplies &q)
{
    set_quick(q);
    if (on_save_quick)
        on_save_quick(q);
}

// Tildes con el teclado de la T-Deck (Shift+$ después de la letra, como en device-ui),
// en el orden del español: a → á → à → ä → â; n → ñ; ? → ¿; ! → ¡
static const char *const ACCENTS[] = {
    "a\xC3\xA1\xC3\xA0\xC3\xA4\xC3\xA2", "e\xC3\xA9\xC3\xA8\xC3\xAB\xC3\xAA", "i\xC3\xAD\xC3\xAC\xC3\xAF\xC3\xAE",
    "o\xC3\xB3\xC3\xB2\xC3\xB6\xC3\xB4", "u\xC3\xBA\xC3\xBC\xC3\xB9\xC3\xBB", "n\xC3\xB1",
    "c\xC3\xA7", "A\xC3\x81\xC3\x80\xC3\x84\xC3\x82", "E\xC3\x89\xC3\x88\xC3\x8B\xC3\x8A",
    "I\xC3\x8D\xC3\x8C\xC3\x8F\xC3\x8E", "O\xC3\x93\xC3\x92\xC3\x96\xC3\x94", "U\xC3\x9A\xC3\x9C\xC3\x99\xC3\x9B",
    "N\xC3\x91", "C\xC3\x87", "?\xC2\xBF", "!\xC2\xA1"};

static size_t utf8_len_at(const char *p)
{
    unsigned char c0 = (unsigned char)*p;
    return c0 < 0x80 ? 1 : (c0 >> 5) == 6 ? 2 : (c0 >> 4) == 14 ? 3 : 4;
}

static void rotate_accent(lv_obj_t *ta)
{
    uint32_t cur = lv_textarea_get_cursor_pos(ta);
    if (cur == 0)
        return;
    const char *text = lv_textarea_get_text(ta);
    const char *prev = text;
    for (uint32_t i = 0; i < cur && *text; i++) { // el carácter antes del cursor
        prev = text;
        text += utf8_len_at(text);
    }
    size_t plen = (size_t)(text - prev);
    for (const char *seq : ACCENTS)
        for (const char *v = seq; *v; v += utf8_len_at(v))
            if (utf8_len_at(v) == plen && std::strncmp(v, prev, plen) == 0) {
                const char *next = v + plen;
                if (!*next)
                    next = seq; // vuelta al principio
                std::string repl(next, utf8_len_at(next));
                lv_textarea_delete_char(ta);
                lv_textarea_add_text(ta, repl.c_str());
                return;
            }
}

bool App::handle_key(uint32_t key)
{
    lv_obj_t *focused0 = lv_group_get_focused(active_group());
    bool in_text = focused0 && lv_obj_check_type(focused0, &lv_textarea_class);
    if (key == KEY_DIACRITIC) { // Shift+$ en la T-Deck
        if (in_text)
            rotate_accent(focused0);
        return true;
    }
    // el teclado de la T-Deck no tiene Esc: Borrar fuera de un cuadro de texto es "volver"
    if (key == LV_KEY_BACKSPACE && !in_text)
        key = LV_KEY_ESC;
    if (key == LV_KEY_ESC) {
        if (sos_overlay_ && !lv_obj_has_flag(sos_overlay_, LV_OBJ_FLAG_HIDDEN)) {
            show_sos(false); // cierra la pantalla roja; el SOS sigue activo
            return true;
        }
        if (current_ == ScreenId::Map && map_screen.handle_esc())
            return true;
        if (current_ == ScreenId::QuickReplies && quick_screen.handle_esc())
            return true;
        back();
        return true;
    }
    lv_obj_t *focused = lv_group_get_focused(active_group());
    bool typing = focused && lv_obj_check_type(focused, &lv_textarea_class);
    bool multiline = typing && !lv_textarea_get_one_line(focused); // en una línea, ↑/↓ salen del cuadro
    // sobre el mapa, en "modo mover" (o con la regla), las flechas y la bola mueven el mapa
    bool on_map = current_ == ScreenId::Map && focused == map_screen.view && map_screen.pan_mode();
    // ↑/↓ recorren los elementos, salvo sobre el mapa (lo desplazan) o al escribir
    if ((key == LV_KEY_UP || key == LV_KEY_DOWN) && !on_map && !multiline) {
        if (key == LV_KEY_UP)
            lv_group_focus_prev(active_group());
        else
            lv_group_focus_next(active_group());
        return true;
    }
    if (current_ == ScreenId::Map && !typing && (key == '+' || key == '-')) {
        map_screen.zoom_by(key == '+' ? 1 : -1);
        return true;
    }
    if (current_ == ScreenId::Home && !typing && key >= '1' && key <= '4') {
        static const ScreenId tabs[] = {ScreenId::Messages, ScreenId::Team, ScreenId::Map, ScreenId::Settings};
        show(tabs[key - '1']);
        return true;
    }
    // en Mensajes, ← → sobre las pestañas cambian de chat
    if (current_ == ScreenId::Messages && !typing && (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) &&
        messages_screen.on_tab()) {
        messages_screen.step_chat(key == LV_KEY_LEFT ? -1 : 1);
        return true;
    }
    // en Mensajes, escribir lleva directo al cuadro de texto
    if (current_ == ScreenId::Messages && !typing && key >= 32 && key != 127)
        messages_screen.focus_input();
    return false;
}

void App::open_chat(const std::string &chat_id)
{
    for (auto &ch : chats_)
        if (ch.id == chat_id)
            ch.unread = 0;
    messages_screen.current = chat_id;
    publish_chats();
}

// ------------------------------------------------------------ Ajustes
void App::set_settings(const Settings &s)
{
    settings_ = s;
    map_screen.set_team_only(s.map_team_only);
    restore_kept_chats();
    publish_chats();
    settings_screen.refresh();
    if (wants_dark() != g_dark || (settings_.lang ? Lang::En : Lang::Es) != g_lang)
        rebuild_ui(); // al arrancar: tema e idioma guardados
}

// ------------------------------------------------------------- tema e idioma
const QuickReplies &App::quick_defaults() const
{
    // máximo QUICK_MAX_CHARS (14): caben completas en los botones del inicio
    static const QuickReplies es{{"OK, recibido", "Voy en camino", "En 15 minutos", "Necesito apoyo"}};
    static const QuickReplies en{{"OK, got it", "On my way", "In 15 minutes", "Need backup"}};
    return english() ? en : es;
}

bool App::wants_dark() const
{
    if (settings_.theme != 2)
        return settings_.theme == 1;
    if (status_.daylight_min >= 0) // con posición: entre la puesta y la salida del sol
        return status_.daylight_min == 0;
    return status_.hour >= 21 || status_.hour < 7; // sin posición, por la hora
}

bool App::ui_busy() const
{
    return current_ == ScreenId::QuickReplies || current_ == ScreenId::Settings ||
           (current_ == ScreenId::Map && map_screen.dialog_open());
}

// Al cambiar de idioma o de tema (a mano en Ajustes, o el automático al anochecer) se rehacen
// las pantallas. El automático espera un momento tranquilo: en el inicio, o tras un minuto sin
// tocar nada y sin algo a medio escribir.
void App::apply_look()
{
    bool lang_changed = (settings_.lang ? Lang::En : Lang::Es) != g_lang;
    if (wants_dark() == g_dark && !lang_changed) {
        look_pending_ = false;
        return;
    }
    if (look_pending_)
        return;
    look_pending_ = true;
    lv_async_call([](void *p) { // fuera del evento que lo pidió (la fila de Ajustes se borra)
        auto *a = static_cast<App *>(p);
        if (a->look_pending_)
            a->rebuild_ui();
    }, this);
}

void App::rebuild_ui()
{
    look_pending_ = false;
    bool was_en = english();
    g_dark = wants_dark();
    g_lang = settings_.lang ? Lang::En : Lang::Es;
    if (was_en != english()) { // respuestas rápidas sin tocar: las del idioma nuevo
        Lang now = g_lang;
        g_lang = was_en ? Lang::En : Lang::Es;
        bool untouched = quick_.text == quick_defaults().text;
        g_lang = now;
        if (untouched) {
            quick_ = quick_defaults();
            if (on_save_quick)
                on_save_quick(quick_);
        }
    }
    // lo que se estaba viendo
    View here = snapshot();
    std::vector<View> hist = history_;
    MapScreen::State ms = map_screen.save_state();
    bool team_all = team_screen.showing_all();
    int srow = current_ == ScreenId::Settings ? settings_screen.focused_row() : -1;
    std::string draft = messages_screen.draft();
    std::string chat = messages_screen.current;
    bool overlay = sos_overlay_ && !lv_obj_has_flag(sos_overlay_, LV_OBJ_FLAG_HIDDEN);

    // lo viejo se borra al final, cuando ya se muestra lo nuevo
    std::vector<lv_obj_t *> old_objs = {home.screen, messages_screen.screen, quick_screen.screen, map_screen.screen,
                                        team_screen.screen, settings_screen.screen, sos_banner_, sos_overlay_};
    std::vector<lv_group_t *> old_groups = {home.group, messages_screen.group, quick_screen.group, settings_screen.group,
                                            sos_group_};
    map_screen.release(old_groups);
    team_screen.release(old_groups);
    home = HomeScreen();
    messages_screen = MessagesScreen();
    quick_screen = QuickScreen();
    map_screen = MapScreen();
    team_screen = TeamScreen();
    settings_screen = SettingsScreen();
    sos_group_ = nullptr;
    sos_banner_ = sos_banner_lbl_ = sos_overlay_ = sos_info_ = sos_count_ = sos_cancel_lbl_ = sos_cancel_fill_ = nullptr;
    sos_direct_ = nullptr;

    home.create(this);
    messages_screen.create(this);
    quick_screen.create(this);
    map_screen.create(this);
    team_screen.create(this);
    settings_screen.create(this);
    build_sos();

    // los datos, de nuevo a las pantallas
    home.set_quick(quick_);
    set_status(status_);
    members_.clear(); // fuerza que los chats se publiquen
    set_team(team_);
    publish_chats();
    map_screen.restore_state(ms);
    map_screen.set_team_only(settings_.map_team_only);
    map_screen.set_position(position_);
    map_screen.set_waypoints(waypoints_);
    map_screen.set_track(track_);
    home.set_recording(track_.recording);
    map_screen.set_saved_tracks(saved_tracks_);
    team_screen.show_all(team_all);
    messages_screen.current = chat;
    settings_screen.refresh();

    go(here, false, true);
    history_ = hist;
    messages_screen.set_draft(draft);
    if (srow >= 0)
        settings_screen.focus_row(srow);
    if (sos_active_)
        show_sos(overlay);

    for (lv_obj_t *o : old_objs)
        if (o)
            lv_obj_delete(o);
    for (lv_group_t *g : old_groups)
        if (g)
            lv_group_delete(g);
    refresh_input_group();
}

void App::flush_settings()
{
    if (settings_timer_) {
        lv_timer_delete(settings_timer_);
        settings_timer_ = nullptr;
    }
    if (!settings_pending_)
        return;
    settings_pending_ = false;
    if (on_settings)
        on_settings(settings_);
}

// guardar: 2 s después del último cambio
void App::save_settings_soon()
{
    settings_pending_ = true;
    if (settings_timer_)
        lv_timer_reset(settings_timer_);
    else {
        settings_timer_ = lv_timer_create(
            [](lv_timer_t *t) {
                auto *a = static_cast<App *>(lv_timer_get_user_data(t));
                a->settings_timer_ = nullptr; // se borra solo (una vez)
                a->settings_pending_ = false;
                if (a->on_settings)
                    a->on_settings(a->settings_);
            },
            2000, this);
        lv_timer_set_repeat_count(settings_timer_, 1);
    }
}

void App::update_settings(const Settings &s)
{
    Settings prev = settings_;
    settings_ = s;
    if (!s.short_name.empty() && s.short_name != status_.short_name) {
        status_.short_name = s.short_name;
        home.set_status(status_);
        messages_screen.set_status(status_);
    }
    map_screen.set_team_only(s.map_team_only);
    publish_chats();
    if (sos_timer_ && prev.sos_every_min != s.sos_every_min)
        lv_timer_set_period(sos_timer_, (uint32_t)s.sos_every_min * 60u * 1000u);
    settings_screen.refresh();
    if (on_settings_preview)
        on_settings_preview(settings_);
    save_settings_soon();
    if (prev.theme != s.theme || prev.lang != s.lang)
        apply_look(); // a mano en Ajustes: ya
    if (prev.msg_keep_days != s.msg_keep_days)
        purge_old_messages();
}

void App::show_settings_section(int section) { go(View{ScreenId::Settings, 0, "", (int8_t)section}, true, false); }

bool App::is_team_chat(const Chat &ch) const
{
    if (ch.is_channel)
        return settings_.channel.on && ch.id == settings_.channel.chat_id;
    for (auto &n : team_)
        if (n.in_team && (ch.name == n.name || ch.id == "node:" + std::to_string(n.num)))
            return true;
    return false;
}

bool App::chat_outside_team(const Chat &ch) const { return !is_team_chat(ch); }

const TeamNode *App::chat_node(const Chat &ch) const
{
    if (ch.is_channel)
        return nullptr;
    for (auto &n : team_)
        if (ch.id == "node:" + std::to_string(n.num) || ch.name == n.name)
            return &n;
    return nullptr;
}

void App::edit_quick(int i)
{
    show(ScreenId::QuickReplies);
    if (i >= 0)
        quick_screen.focus_field(i);
}

// ------------------------------------------------------------- borrar chats
void App::forget_chat(const std::string &chat_id, bool remove)
{
    messages_.erase(chat_id);
    msg_rev_[chat_id]++;
    for (auto it = chats_.begin(); it != chats_.end(); ++it)
        if (it->id == chat_id) {
            if (remove)
                chats_.erase(it);
            else
                it->preview.clear(), it->time.clear(), it->unread = 0;
            break;
        }
    if (on_clear_chat)
        on_clear_chat(chat_id, remove);
    if (remove && messages_screen.current == chat_id)
        messages_screen.current = chats_.empty() ? std::string() : chats_.front().id;
}

void App::delete_chat(const std::string &chat_id)
{
    // se borran los mensajes; el chat (directo o canal) queda en su pestaña, vacío, y un
    // directo se anota en los ajustes para que siga ahí al reiniciar
    for (auto &ch : chats_)
        if (ch.id == chat_id && !ch.is_channel)
            if (uint32_t num = chat_num(ch))
                keep_chat(num, true);
    forget_chat(chat_id, false);
    publish_chats();
    messages_screen.show_chat(messages_screen.current);
}

void App::clear_all_chats()
{
    std::vector<std::pair<std::string, bool>> all;
    for (auto &ch : chats_) {
        all.push_back({ch.id, !ch.is_channel});
        if (!ch.is_channel) // los directos desaparecen: tampoco vuelven al reiniciar
            if (uint32_t num = chat_num(ch))
                keep_chat(num, false);
    }
    for (auto &c : all)
        forget_chat(c.first, c.second);
    publish_chats();
    messages_screen.show_chat(messages_screen.current);
}

void App::purge_old_messages()
{
    if (!settings_.msg_keep_days || !status_.epoch)
        return;
    uint32_t limit = settings_.msg_keep_days * 86400u;
    if (status_.epoch < limit)
        return;
    uint32_t cut = status_.epoch - limit;
    std::vector<std::string> emptied;
    for (auto &kv : messages_) {
        auto &list = kv.second;
        size_t n = list.size();
        list.erase(std::remove_if(list.begin(), list.end(), [cut](const Message &m) { return m.epoch && m.epoch < cut; }),
                   list.end());
        if (list.size() != n) {
            msg_rev_[kv.first]++;
            if (list.empty())
                emptied.push_back(kv.first);
        }
    }
    for (auto &id : emptied) { // un directo sin mensajes desaparece; un canal queda vacío
        bool channel = false;
        for (auto &ch : chats_)
            if (ch.id == id) {
                channel = ch.is_channel;
                if (!channel)
                    if (uint32_t num = chat_num(ch))
                        keep_chat(num, false);
            }
        forget_chat(id, !channel);
    }
    refresh_previews(chats_, messages_);
    publish_chats();
    messages_screen.refresh_conversation(false);
}

void App::publish_chats()
{
    std::vector<Chat> view = chats_; // chats_ queda por actividad reciente
    if (settings_.team_first)
        std::stable_partition(view.begin(), view.end(), [this](const Chat &c) { return is_team_chat(c); });
    home.set_chats(view);
    messages_screen.set_chats(view);
}

// ------------------------------------------------------- canal privado
ChannelKey App::new_key()
{
    ChannelKey k{};
    if (random_bytes)
        random_bytes(k.data(), k.size());
    else
        for (auto &b : k)
            b = (uint8_t)(std::rand() & 0xFF);
    return k;
}

std::string App::ensure_channel_chat(const std::string &name)
{
    for (auto &ch : chats_)
        if (ch.is_channel && ch.name == name)
            return ch.id;
    int idx = 1;
    for (auto &ch : chats_)
        if (ch.is_channel && ch.channel >= idx)
            idx = ch.channel + 1;
    Chat ch;
    ch.id = "canal:" + name;
    ch.name = name;
    ch.is_channel = true;
    ch.channel = (int8_t)std::min(idx, 7);
    ch.preview = tr("Canal privado del equipo");
    chats_.push_back(ch);
    publish_chats();
    return ch.id;
}

void App::create_channel()
{
    Settings s = settings_;
    s.channel.on = true;
    if (s.channel.name.empty())
        s.channel.name = tr("Equipo");
    s.channel.key = new_key();
    s.channel.chat_id = ensure_channel_chat(s.channel.name);
    s.members.clear();
    if (on_channel_set)
        on_channel_set(s.channel.name, s.channel.key);
    update_settings(s);
    flush_settings(); // la clave tiene que quedar guardada ya
}

void App::rename_channel(const std::string &raw)
{
    std::string name = raw;
    while (!name.empty() && name.back() == ' ')
        name.pop_back();
    name = utf8_truncate_bytes(name, 11);
    if (name.empty() || name == settings_.channel.name || !settings_.channel.on)
        return;
    Settings s = settings_;
    for (auto &ch : chats_)
        if (ch.id == s.channel.chat_id)
            ch.name = name;
    s.channel.name = name;
    for (auto &m : s.members) // el nombre es parte del canal: hay que invitar de nuevo
        m.state = ChannelMember::None;
    if (on_channel_set)
        on_channel_set(name, s.channel.key);
    update_settings(s);
    flush_settings();
}

void App::rekey_channel()
{
    if (!settings_.channel.on)
        return;
    Settings s = settings_;
    s.channel.key = new_key();
    for (auto &m : s.members)
        m.state = ChannelMember::None;
    if (on_channel_set)
        on_channel_set(s.channel.name, s.channel.key);
    update_settings(s);
    flush_settings();
}

void App::remove_channel()
{
    if (!settings_.channel.on)
        return;
    Settings s = settings_;
    std::string id = s.channel.chat_id;
    chats_.erase(std::remove_if(chats_.begin(), chats_.end(), [&](const Chat &c) { return c.id == id; }), chats_.end());
    messages_.erase(id);
    sos_chats_.erase(std::remove(sos_chats_.begin(), sos_chats_.end(), id), sos_chats_.end());
    if (messages_screen.current == id)
        messages_screen.current.clear();
    s.channel = TeamChannel{}; // el próximo empieza de cero ("Equipo", clave nueva)
    s.members.clear();
    if (on_channel_set)
        on_channel_set("", s.channel.key);
    update_settings(s);
    flush_settings();
}

int App::pending_invites() const
{
    if (!settings_.channel.on)
        return 0;
    int n = 0;
    for (auto *p : team_people()) {
        const ChannelMember *m = settings_.member(p->num);
        if (p->has_pki && !(m && m->state == ChannelMember::Joined))
            n++;
    }
    return n;
}

int App::invite_team()
{
    if (!settings_.channel.on)
        return 0;
    Settings s = settings_;
    int n = 0;
    for (auto *p : team_people()) {
        if (!p->has_pki) // sin clave pública el mensaje no iría cifrado de extremo a extremo
            continue;
        ChannelMember *m = s.member(p->num);
        if (m && m->state == ChannelMember::Joined)
            continue;
        if (!m) {
            s.members.push_back(ChannelMember{p->num});
            m = &s.members.back();
        }
        m->state = ChannelMember::Invited;
        m->epoch = status_.epoch;
        if (on_channel_invite)
            on_channel_invite(p->num, s.channel.name, s.channel.key);
        n++;
    }
    update_settings(s);
    return n;
}

void App::channel_joined(uint32_t num)
{
    Settings s = settings_;
    ChannelMember *m = s.member(num);
    if (!m) {
        s.members.push_back(ChannelMember{num});
        m = &s.members.back();
    }
    m->state = ChannelMember::Joined;
    m->epoch = status_.epoch;
    update_settings(s);
}

void App::receive_channel_invite(uint32_t from, const std::string &name, const ChannelKey &key)
{
    bool same = settings_.channel.on && settings_.channel.key == key && settings_.channel.name == name;
    uint32_t id = next_invite_++;
    invites_[id] = Invite{from, name, key};
    std::string chat = direct_chat_id(from, true);
    Message m;
    m.from = node_name(team_, from);
    for (auto &n : team_)
        if (n.num == from)
            m.from_color = n.color;
    m.time = now_hhmm();
    m.text = tr("Te invita al canal privado \xC2\xAB") + name + "\xC2\xBB";
    if (same)
        m.text += tr(" \xC2\xB7 ya est\xC3\xA1s unido");
    else if (settings_.channel.on)
        m.text += tr(" (reemplaza a \xC2\xAB") + settings_.channel.name + "\xC2\xBB)";
    m.invite_id = same ? 0 : id;
    add_message(chat, m);
}

bool App::accept_invite(uint32_t invite_id)
{
    auto it = invites_.find(invite_id);
    if (it == invites_.end())
        return false;
    Invite inv = it->second;
    invites_.erase(it);
    Settings s = settings_;
    if (s.channel.on && s.channel.name != inv.name) { // reemplaza el canal anterior
        std::string old = s.channel.chat_id;
        chats_.erase(std::remove_if(chats_.begin(), chats_.end(), [&](const Chat &c) { return c.id == old; }), chats_.end());
        messages_.erase(old);
        sos_chats_.erase(std::remove(sos_chats_.begin(), sos_chats_.end(), old), sos_chats_.end());
    }
    s.channel.on = true;
    s.channel.name = inv.name;
    s.channel.key = inv.key;
    s.channel.chat_id = ensure_channel_chat(inv.name);
    s.members.clear();
    s.members.push_back(ChannelMember{inv.from, ChannelMember::Joined, status_.epoch});
    if (on_channel_set)
        on_channel_set(inv.name, inv.key);
    if (on_channel_join_ack)
        on_channel_join_ack(inv.from);
    for (auto &kv : messages_)
        for (auto &m : kv.second)
            if (m.invite_id == invite_id) {
                m.invite_id = 0;
                m.text = tr("Te invit\xC3\xB3 al canal privado \xC2\xAB") + inv.name + tr("\xC2\xBB \xC2\xB7 te uniste");
                msg_rev_[kv.first]++;
            }
    refresh_previews(chats_, messages_);
    update_settings(s);
    flush_settings();
    messages_screen.refresh_conversation();
    return true;
}

} // namespace tfield
