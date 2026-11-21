// Prueba al azar ("mono"): miles de toques, teclas y eventos de red mezclados.
// Busca caídas, errores de memoria (con ASan) y estados inconsistentes de la navegación.
// Variables: SEMILLA (por defecto 1) y PASOS (por defecto 4000).
#include "tfield/format.h"
#include "demo.h"
#include "harness.h"

#include <cstdlib>
#include <random>

using namespace tfield;
using th::app;

static void clickables(lv_obj_t *o, std::vector<lv_obj_t *> &out)
{
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN))
        return;
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE) && lv_obj_get_parent(o))
        out.push_back(o);
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++)
        clickables(lv_obj_get_child(o, i), out);
}

static lv_obj_t *screen_of(ScreenId id)
{
    switch (id) {
    case ScreenId::Home: return app->home.screen;
    case ScreenId::Messages: return app->messages_screen.screen;
    case ScreenId::QuickReplies: return app->quick_screen.screen;
    case ScreenId::Map: return app->map_screen.screen;
    case ScreenId::Team: return app->team_screen.screen;
    case ScreenId::Settings: return app->settings_screen.screen;
    }
    return nullptr;
}

static std::string rnd_text(std::mt19937 &r, int max)
{
    static const char *parts[] = {"a", "b", " ", "\xC3\xB1", "\xC3\xA1", "1", "\xF0\x9F\x98\x80", "<", "&", "\"", "\xC2\xB7", "Z"};
    std::string s;
    int n = (int)(r() % (max + 1));
    for (int i = 0; i < n; i++)
        s += parts[r() % 12];
    return s;
}

APP_TEST(prueba_al_azar)
{
    uint32_t seed = std::getenv("SEMILLA") ? (uint32_t)std::atoi(std::getenv("SEMILLA")) : 1;
    int steps = std::getenv("PASOS") ? std::atoi(std::getenv("PASOS")) : 4000;
    std::mt19937 r(seed);
    std::printf("  semilla %u, %d pasos\n", seed, steps);
    lv_indev_t *kp = nullptr;
    for (lv_indev_t *in = lv_indev_get_next(nullptr); in; in = lv_indev_get_next(in))
        if (lv_indev_get_type(in) == LV_INDEV_TYPE_KEYPAD)
            kp = in;
    static const uint32_t keys[] = {LV_KEY_ESC, LV_KEY_ENTER, LV_KEY_ENTER, LV_KEY_UP,   LV_KEY_DOWN, LV_KEY_DOWN,
                                    LV_KEY_LEFT, LV_KEY_RIGHT, LV_KEY_NEXT, LV_KEY_PREV, LV_KEY_BACKSPACE, '1',
                                    '2',        '3',          '4',         '+',         '-',          'x'};
    static const char *froms[] = {"GEO2", "VEH1", "BASE", "MNT4", "Q7XA", "NUEV", ""};
    std::vector<uint32_t> extra_nodes;
    uint32_t elapsed = 0;
    int bad_screen = 0, bad_group = 0;
    for (int step = 0; step < steps; step++) {
        int k = (int)(r() % 100);
        if (k < 30) { // toque sobre algo que se pueda tocar
            std::vector<lv_obj_t *> c;
            clickables(lv_screen_active(), c);
            clickables(lv_layer_top(), c);
            if (!c.empty()) {
                lv_obj_t *o = c[r() % c.size()];
                lv_area_t a;
                lv_obj_get_coords(o, &a);
                int32_t x = std::clamp((a.x1 + a.x2) / 2, 0, 319), y = std::clamp((a.y1 + a.y2) / 2, 0, 239);
                if (r() % 10 == 0)
                    th::hold(x, y, 300 + r() % 2400);
                else
                    th::tap(x, y);
            }
        } else if (k < 36) { // toque o arrastre en cualquier lugar
            th::tap((int32_t)(r() % 320), (int32_t)(r() % 240));
        } else if (k < 62) {
            th::key(keys[r() % (sizeof(keys) / sizeof(keys[0]))]);
        } else if (k < 67) {
            th::type(rnd_text(r, 6));
        } else if (k < 90) { // algo llega de la red
            int e = (int)(r() % 22);
            const char *from = froms[r() % 7];
            switch (e) {
            case 0:
            case 1: {
                Message m;
                m.from = from;
                m.time = app->now_hhmm();
                m.text = r() % 8 == 0 ? "SOS " + std::string(from) + " 19H 1 2" : rnd_text(r, 40);
                const auto &ch = app->chats();
                std::string id = !ch.empty() && r() % 4 ? ch[r() % ch.size()].id : "node:" + std::to_string(100 + r() % 5);
                app->add_message(id, m);
                break;
            }
            case 2: {
                Waypoint w;
                w.name = rnd_text(r, 12);
                w.from = from;
                w.lat = -33.64 + (int)(r() % 200 - 100) * 0.0005;
                w.lon = -70.35 + (int)(r() % 200 - 100) * 0.0005;
                app->receive_waypoint(w, r() % 2 ? "geo2" : "longfast");
                break;
            }
            case 3: {
                ChannelKey key{};
                key[r() % 32] = (uint8_t)r();
                app->receive_channel_invite(2 + r() % 4, r() % 2 ? "Cumbre" : rnd_text(r, 14), key);
                break;
            }
            case 4: app->sos_ack(2 + r() % 4); break;
            case 5: app->channel_joined(2 + r() % 10); break;
            case 6: app->position_received(2 + r() % 4); break;
            case 7: app->traceroute_result(2 + r() % 4, rnd_text(r, 30)); break;
            case 8: { // la red cambia: aparecen y desaparecen nodos
                auto nodes = demo::team(elapsed / 1000);
                if (r() % 3 == 0 && !nodes.empty())
                    nodes.erase(nodes.begin() + r() % nodes.size());
                if (r() % 3 == 0)
                    extra_nodes.push_back(100 + r() % 50);
                for (uint32_t num : extra_nodes) {
                    TeamNode n;
                    n.num = num;
                    n.name = rnd_text(r, 4);
                    n.long_name = rnd_text(r, 30);
                    n.dist_m = r() % 2 ? -1 : (int32_t)(r() % 50000);
                    n.has_pos = n.dist_m >= 0;
                    n.lat = -33.6 + (int)(r() % 100) * 0.001;
                    n.lon = -70.3 + (int)(r() % 100) * 0.001;
                    n.battery = (int8_t)((int)(r() % 103) - 1);
                    n.signal = (uint8_t)(r() % 5);
                    n.heard_ago_s = r() % 100000;
                    n.stale = n.heard_ago_s > 1800;
                    n.has_pki = r() % 2;
                    n.relay = r() % 5 == 0;
                    nodes.push_back(n);
                }
                app->set_team(nodes);
                break;
            }
            case 9: {
                Position p = app->position();
                p.valid = r() % 6 != 0;
                p.lat += (int)(r() % 21 - 10) * 0.0002;
                p.lon += (int)(r() % 21 - 10) * 0.0002;
                app->set_position(p);
                if (p.valid)
                    app->add_track_point({p.lat, p.lon, (float)p.alt, app->status().epoch + elapsed / 1000});
                break;
            }
            case 10: {
                int h = (int)(r() % 24), mi = (int)(r() % 60);
                Status s = demo::status(h, mi);
                if (r() % 10 == 0)
                    s.epoch = 0;
                app->set_status(s);
                break;
            }
            case 11: { // ajustes cambiados por otro lado (p. ej. desde la app del teléfono)
                Settings s = app->settings();
                s.map_team_only = r() % 2;
                s.team_first = r() % 2;
                s.pts_others = (PointPolicy)(r() % 3);
                s.sos_every_min = (uint8_t)(1 + r() % 5);
                if (r() % 5 == 0)
                    s.channel.on = !s.channel.on;
                app->update_settings(s);
                break;
            }
            case 12: { // la lista de chats cambia (un canal desaparece)
                auto c = app->chats();
                if (!c.empty() && r() % 2)
                    c.erase(c.begin() + r() % c.size());
                app->set_chats(c);
                break;
            }
            case 13: { // puntos borrados desde otra parte mientras se editan
                auto w = app->waypoints();
                if (!w.empty())
                    w.erase(w.begin() + r() % w.size());
                app->set_waypoints(w);
                break;
            }
            case 14: {
                auto t = app->saved_tracks();
                if (!t.empty() && r() % 2)
                    t.erase(t.begin() + r() % t.size());
                app->set_saved_tracks(t);
                break;
            }
            case 15:
                if (r() % 4 == 0)
                    app->trigger_sos();
                else
                    app->cancel_sos();
                break;
            case 16: app->request_team_positions(); break;
            case 17: { // una ruta más en la SD
                Track t;
                Position p = app->position();
                int n = 2 + (int)(r() % 60);
                for (int k = 0; k < n; k++)
                    t.pts.push_back({p.lat + k * 0.0003 * ((int)(r() % 3) - 1), p.lon + k * 0.0003, (float)(1500 + r() % 30),
                                     app->status().epoch + (uint32_t)k * 10});
                app->add_saved_track(r() % 3 ? rnd_text(r, 12) : "", r() % 2 ? rnd_text(r, 20) : "", t);
                break;
            }
            case 18: { // mostrar, renombrar o borrar una ruta guardada
                const auto &t = app->saved_tracks();
                if (t.empty())
                    break;
                uint32_t id = t[r() % t.size()].id;
                int what = (int)(r() % 3);
                if (what == 0)
                    app->map_screen.show_route(id);
                else if (what == 1)
                    app->update_track(id, rnd_text(r, 14), rnd_text(r, 10));
                else
                    app->delete_track(id);
                break;
            }
            case 19: // la tarjeta SD se suelta o vuelve
                if (r() % 6 == 0)
                    th::rec.sd_ok = !th::rec.sd_ok;
                break;
            case 20: { // grabar: empezar, sumar puntos, detener
                int what = (int)(r() % 6);
                if (what == 0)
                    app->start_recording();
                else if (what == 1)
                    app->stop_recording();
                else {
                    Position p = app->position();
                    p.lat += ((int)(r() % 21) - 10) * 0.0001;
                    p.lon += ((int)(r() % 21) - 10) * 0.0001;
                    app->set_position(p);
                    app->add_track_point({p.lat, p.lon, (float)p.alt, app->status().epoch + (uint32_t)(r() % 100000)});
                }
                break;
            }
            case 21: { // ráfaga de mensajes en un canal concurrido
                Message m;
                m.from = from;
                m.time = app->now_hhmm();
                const auto &ch = app->chats();
                std::string id = !ch.empty() ? ch[r() % ch.size()].id : "node:101";
                for (int k = 0; k < 30; k++) {
                    m.text = rnd_text(r, 30);
                    app->add_message(id, m);
                }
                break;
            }
            }
        } else {
            uint32_t ms = 50 + r() % 4000;
            th::tick(ms);
            elapsed += ms;
        }
        // invariantes: la pantalla visible es la que la App cree, y el teclado maneja su grupo
        if (lv_screen_active() != screen_of(app->current()) && bad_screen++ < 3)
            std::printf("  paso %d: pantalla visible distinta de la actual\n", step);
        if (kp && lv_indev_get_group(kp) != app->active_group() && bad_group++ < 3)
            std::printf("  paso %d: el teclado no maneja el grupo de la pantalla\n", step);
    }
    CHECK_EQ(bad_screen, 0);
    CHECK_EQ(bad_group, 0);
    int objs = th::count_objects();
    std::printf("  objetos LVGL al final: %d\n", objs);
    CHECK(objs < 8000); // sin crecer sin límite
}
