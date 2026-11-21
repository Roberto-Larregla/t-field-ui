// Simulador web: el mismo código LVGL de la T-Deck, compilado para el navegador.
// La página llama a sim_tick() en cada cuadro y copia sim_fb() a un <canvas> de 320x240.
#include "tfield/app.h"
#include "demo.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>

using namespace tfield;

static uint16_t draw_buf[SCREEN_W * SCREEN_H / 4];
static uint8_t rgba[SCREEN_W * SCREEN_H * 4];
static bool dirty = true;

static int32_t ptr_x = 0, ptr_y = 0;
static bool ptr_down = false;
static std::deque<uint32_t> keys;
static bool key_released = true;

static App app;
static uint32_t elapsed_ms = 0, last_minute_ms = 0, last_sec_ms = 0;
static int sim_hour = 14, sim_min = 32;
static double walk_e = 0, walk_n = 0;
static std::string last_file; // último archivo "guardado" en la SD simulada

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    const uint16_t *src = reinterpret_cast<const uint16_t *>(px);
    for (int32_t y = a->y1; y <= a->y2; y++) {
        for (int32_t x = a->x1; x <= a->x2; x++) {
            uint16_t v = *src++;
            uint8_t *o = &rgba[(y * SCREEN_W + x) * 4];
            o[0] = ((v >> 11) & 31) * 255 / 31;
            o[1] = ((v >> 5) & 63) * 255 / 63;
            o[2] = (v & 31) * 255 / 31;
            o[3] = 255;
        }
    }
    dirty = true;
    lv_display_flush_ready(d);
}

// En pantallas táctiles no hay "hover": si el dedo aparece en otro lugar, primero se
// informa la nueva posición sin presionar, para que LVGL no lo tome como arrastre.
static void pointer_read(lv_indev_t *, lv_indev_data_t *data)
{
    static int32_t last_x = 0, last_y = 0;
    static bool last_down = false;
    bool down = ptr_down;
    if (down && !last_down && (std::abs(ptr_x - last_x) > 2 || std::abs(ptr_y - last_y) > 2))
        down = false;
    data->point.x = ptr_x;
    data->point.y = ptr_y;
    data->state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    last_x = ptr_x;
    last_y = ptr_y;
    last_down = down;
}

// Cada tecla se entrega como "presionada" y luego "soltada".
static void keypad_read(lv_indev_t *, lv_indev_data_t *data)
{
    static uint32_t last = 0;
    if (!keys.empty() && key_released) {
        last = keys.front();
        keys.pop_front();
        data->key = last;
        data->state = LV_INDEV_STATE_PRESSED;
        key_released = false;
    } else {
        data->key = last;
        data->state = LV_INDEV_STATE_RELEASED;
        key_released = true;
    }
    data->continue_reading = !keys.empty();
}

extern "C" {

uint8_t *sim_fb(void) { return rgba; }

// Contenido del último archivo GPX generado (para descargarlo desde la página)
const char *sim_last_file(void) { return last_file.c_str(); }

int sim_dirty(void)
{
    int d = dirty ? 1 : 0;
    dirty = false;
    return d;
}

void sim_pointer(int x, int y, int down)
{
    ptr_x = x;
    ptr_y = y;
    ptr_down = down != 0;
}

// Códigos: 27 Esc · 13 Enter · 9 siguiente · 11 anterior · 8 borrar · 19/20 der/izq · otros = carácter
// LVGL espera los caracteres como bytes UTF-8 empaquetados en un uint32 (orden de memoria).
static uint32_t utf8_packed(uint32_t cp)
{
    uint8_t b[4] = {0, 0, 0, 0};
    if (cp < 0x80)
        return cp;
    if (cp < 0x800) {
        b[0] = 0xC0 | (cp >> 6);
        b[1] = 0x80 | (cp & 0x3F);
    } else if (cp < 0x10000) {
        b[0] = 0xE0 | (cp >> 12);
        b[1] = 0x80 | ((cp >> 6) & 0x3F);
        b[2] = 0x80 | (cp & 0x3F);
    } else {
        b[0] = 0xF0 | (cp >> 18);
        b[1] = 0x80 | ((cp >> 12) & 0x3F);
        b[2] = 0x80 | ((cp >> 6) & 0x3F);
        b[3] = 0x80 | (cp & 0x3F);
    }
    uint32_t v;
    std::memcpy(&v, b, 4);
    return v;
}

void sim_key(uint32_t code)
{
    if (code == tfield::KEY_DIACRITIC) { // Shift+$ de la T-Deck (tildes)
        app.handle_key(code);
        return;
    }
    if (code >= 0x80 && code != 127)
        code = utf8_packed(code);
    if (code == 27)
        code = LV_KEY_ESC;
    else if (code == 13)
        code = LV_KEY_ENTER;
    if (app.handle_key(code))
        return;
    keys.push_back(code);
}

void sim_tick(uint32_t ms)
{
    elapsed_ms += ms;
    // el reloj de la demo avanza en tiempo real desde las 14:32
    if (elapsed_ms - last_minute_ms >= 60000) {
        last_minute_ms = elapsed_ms;
        if (++sim_min == 60) {
            sim_min = 0;
            sim_hour = (sim_hour + 1) % 24;
        }
        app.set_status(demo::status(sim_hour, sim_min));
        app.set_team(demo::team(elapsed_ms / 1000));
    }
    // mientras se graba una ruta, la demo "camina" hacia el noreste
    if (elapsed_ms - last_sec_ms >= 2000) {
        last_sec_ms = elapsed_ms;
        if (app.track().recording) {
            walk_e += 2.2;
            walk_n += 2.4;
            Position p = app.position();
            demo::local_to_latlon(walk_e, walk_n, p.lat, p.lon);
            p.alt = (int32_t)demo::elevation(walk_e, walk_n);
            app.set_position(p);
            uint32_t ep = demo::now_epoch(sim_hour, sim_min) + (elapsed_ms - last_minute_ms) / 1000;
            app.add_track_point({p.lat, p.lon, (float)p.alt, ep});
        }
    }
    // eventos de la demo: llegan cosas de la red
    static int step = 0;
    auto incoming = [](const char *chat, const char *from, uint32_t color, const char *text) {
        Message m;
        m.from = from;
        m.from_color = color;
        m.time = app.now_hhmm();
        m.text = text;
        app.add_message(chat, m);
    };
    if (step == 0 && elapsed_ms > 25000) { // GEO2 (equipo) comparte un punto: se guarda solo
        step++;
        app.receive_waypoint(demo::shared_from_geo2(), "geo2");
    } else if (step == 1 && elapsed_ms > 40000) { // MNT4 (fuera del equipo) comparte uno: se pregunta
        step++;
        Waypoint w;
        w.name = "Mirador norte";
        w.from = "MNT4";
        w.time = app.now_hhmm();
        w.epoch = app.status().epoch;
        demo::local_to_latlon(900, 2400, w.lat, w.lon);
        w.alt = (int32_t)demo::elevation(900, 2400);
        app.receive_waypoint(w, "longfast");
    } else if (step == 2 && elapsed_ms > 55000) { // canal público, fuera del equipo: sin sonido
        step++;
        incoming("longfast", "Q7XA", 0x6E7B85, "\xC2\xBF" "Alguien cerca del embalse?");
    } else if (step == 3 && elapsed_ms > 70000) { // GEO2 escribe: suena
        step++;
        incoming("geo2", "GEO2", col::PURPLE, "Bajamos por la quebrada, nos vemos en P3");
    }
    lv_tick_inc(ms);
    lv_timer_handler();
}

} // extern "C"

int main()
{
    lv_init();
    lv_display_t *d = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, draw_buf, nullptr, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush_cb);

    lv_indev_t *ptr = lv_indev_create();
    lv_indev_set_type(ptr, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(ptr, pointer_read);
    lv_indev_t *kp = lv_indev_create();
    lv_indev_set_type(kp, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(kp, keypad_read);

    app.on_send_text = [](const std::string &, const std::string &) {};
    // respuestas que dependen del nodo: los que están lejos (sin noticias hace rato) no contestan
    auto reachable = [](uint32_t num) {
        for (auto &n : app.nodes())
            if (n.num == num)
                return !n.stale;
        return false;
    };
    auto after = [](uint32_t ms, void (*fn)(uint32_t), uint32_t num) {
        struct Job {
            void (*fn)(uint32_t);
            uint32_t num;
        };
        lv_timer_t *t = lv_timer_create(
            [](lv_timer_t *t) {
                auto *j = static_cast<Job *>(lv_timer_get_user_data(t));
                j->fn(j->num);
                delete j;
            },
            ms, new Job{fn, num});
        lv_timer_set_repeat_count(t, 1);
    };
    // respuestas simuladas de la red (llegan después de unos segundos)
    struct Reply {
        uint32_t num;
        bool route;
    };
    auto later = [](uint32_t num, bool route, uint32_t ms) {
        auto *r = new Reply{num, route};
        lv_timer_t *t = lv_timer_create(
            [](lv_timer_t *t) {
                auto *r = static_cast<Reply *>(lv_timer_get_user_data(t));
                if (r->route)
                    app.traceroute_result(r->num, r->num == 2 ? "GEO1 \xE2\x80\xBA GEO2, directo \xC2\xB7 SNR 6,5 dB"
                                                              : "GEO1 \xE2\x80\xBA RPT1 \xE2\x80\xBA destino \xC2\xB7 2 saltos");
                else
                    app.position_received(r->num);
                delete r;
            },
            ms, r);
        lv_timer_set_repeat_count(t, 1);
    };
    app.on_request_position = [later, reachable](uint32_t num) {
        static uint32_t extra = 0; // varios pedidos seguidos: las respuestas llegan escalonadas
        if (reachable(num))
            later(num, false, 2000 + (extra++ % 4) * 1300);
    };
    app.on_traceroute = [later](uint32_t num) { later(num, true, 3500); };
    app.on_sos = [reachable, after](bool active, const std::string &text, const std::vector<std::string> &chats,
                                    const std::vector<uint32_t> &direct) {
        for (auto &id : chats) {
            Message m;
            m.mine = true;
            m.from = "T\xC3\xBA";
            m.time = app.now_hhmm();
            m.text = text;
            m.status = "Enviado";
            app.add_message(id, m);
        }
        if (!active)
            return;
        uint32_t delay = 2000;
        for (uint32_t n : direct)
            if (reachable(n)) {
                after(delay, [](uint32_t num) { app.sos_ack(num); }, n);
                delay += 1700;
            }
    };
    app.on_channel_invite = [reachable, after](uint32_t num, const std::string &, const ChannelKey &) {
        if (reachable(num))
            after(3500, [](uint32_t n) { app.channel_joined(n); }, num);
    };
    app.on_settings_preview = [](const Settings &s) { // aplicar al tiro (el guardado se agrupa)
        static const int shift[] = {0, 0, -2, 3}; // continental, Magallanes, Pascua, UTC (en noviembre)
        int sh = s.tz < 4 ? shift[s.tz] : 0; // las demás zonas: sin desfase en la demo
        bool changed = sh != demo::tz_shift_h || s.short_name != demo::short_name;
        demo::tz_shift_h = sh;
        demo::short_name = s.short_name;
        if (changed)
            app.set_status(demo::status(sim_hour, sim_min));
    };
    demo::attach_files(app); // tarjeta SD en memoria (rutas, índice, respaldo)
    app.on_save_file = [](const std::string &path, const std::string &content) {
        demo::files()[path] = content;
        if (path.find("indice-") == std::string::npos && path.find("_en_curso") == std::string::npos) // ni índice ni respaldo
            last_file = path + "\n" + content;
        return true;
    };
    app.on_append_file = [](const std::string &path, const std::string &content) {
        demo::files()[path] += content;
        if (last_file.rfind(path + "\n", 0) == 0) // la ruta se escribe por partes
            last_file += content;
        return true;
    };
    app.init();
    demo::load(app, sim_hour, sim_min);
    last_file.clear(); // la ruta de ejemplo escrita al arrancar no se muestra como exportada
    lv_indev_set_group(kp, app.active_group());
    return 0;
}
