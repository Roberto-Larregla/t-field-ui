// Pruebas: arranque de LVGL sin pantalla, entrada simulada y ejecución aislada con fork.
#include "harness.h"
#include "demo.h"

#include <cstring>
#include <deque>
#include <sys/wait.h>
#include <unistd.h>

namespace th {

int fails = 0, passes = 0;
tfield::App *app = nullptr;
Log rec;

std::vector<Test> &registry()
{
    static std::vector<Test> r;
    return r;
}

// ---------------------------------------------------------------- LVGL
static uint16_t draw_buf[tfield::SCREEN_W * tfield::SCREEN_H / 4];
static int32_t ptr_x = 0, ptr_y = 0;
static bool ptr_down = false;
static std::deque<uint32_t> keys;
static bool key_released = true;
static bool hold_key = false; // clic de la bola mantenido
static uint32_t held_key = 0;

uint64_t flushed_px = 0;

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *)
{
    flushed_px += (uint64_t)lv_area_get_width(a) * (uint64_t)lv_area_get_height(a);
    lv_display_flush_ready(d);
}

static void pointer_read(lv_indev_t *, lv_indev_data_t *data)
{
    static int32_t last_x = 0, last_y = 0;
    static bool last_down = false;
    bool down = ptr_down;
    if (down && !last_down && (std::abs(ptr_x - last_x) > 2 || std::abs(ptr_y - last_y) > 2))
        down = false; // primero se informa la nueva posición, sin presionar
    data->point.x = ptr_x;
    data->point.y = ptr_y;
    data->state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    last_x = ptr_x;
    last_y = ptr_y;
    last_down = down;
}

static void keypad_read(lv_indev_t *, lv_indev_data_t *data)
{
    static uint32_t last = 0;
    if (hold_key) { // tecla mantenida (clic de la bola)
        data->key = held_key;
        data->state = LV_INDEV_STATE_PRESSED;
        last = held_key;
        key_released = false;
        return;
    }
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

static lv_indev_t *kp = nullptr;

static void init_lvgl()
{
    lv_init();
    lv_display_t *d = lv_display_create(tfield::SCREEN_W, tfield::SCREEN_H);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, draw_buf, nullptr, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush_cb);
    lv_indev_t *p = lv_indev_create();
    lv_indev_set_type(p, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(p, pointer_read);
    kp = lv_indev_create();
    lv_indev_set_type(kp, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(kp, keypad_read);
}

void tick(uint32_t ms)
{
    while (ms > 0) {
        uint32_t s = ms > 10 ? 10 : ms;
        lv_tick_inc(s);
        lv_timer_handler();
        ms -= s;
    }
}

void tap(int32_t x, int32_t y)
{
    ptr_x = x;
    ptr_y = y;
    ptr_down = false;
    tick(40);
    ptr_down = true;
    tick(120);
    ptr_down = false;
    tick(80);
}

void hold(int32_t x, int32_t y, uint32_t ms)
{
    ptr_x = x;
    ptr_y = y;
    ptr_down = false;
    tick(40);
    ptr_down = true;
    tick(ms);
    ptr_down = false;
    tick(80);
}

void key(uint32_t k)
{
    if (app && app->handle_key(k)) {
        tick(30);
        return;
    }
    keys.push_back(k);
    for (int i = 0; i < 50 && !(keys.empty() && key_released); i++)
        tick(10); // presionar y soltar, como una tecla real
    tick(40);
}

uint64_t pixels_for(const std::function<void()> &action)
{
    tick(120); // terminar lo pendiente
    uint64_t before = flushed_px;
    action();
    tick(120);
    return flushed_px - before;
}

void ball(uint32_t dir)
{
    key(dir);
    tick(250); // el controlador de la bola limita a 4 movimientos por segundo
}

void ball_press(uint32_t ms)
{
    // Enter presionado de verdad durante ms (el botón de la bola es un GPIO)
    held_key = LV_KEY_ENTER;
    hold_key = true;
    tick(ms);
    hold_key = false;
    tick(80);
}

void kb(uint32_t code)
{
    // el teclado T-Deck manda Enter como 0x0D; device-ui lo traduce a LV_KEY_ENTER
    key(code == 0x0D ? LV_KEY_ENTER : code);
}

void kb_text(const std::string &s) { type(s); }

void type(const std::string &s)
{
    for (size_t i = 0; i < s.size();) {
        unsigned char c0 = (unsigned char)s[i];
        size_t n = c0 < 0x80 ? 1 : (c0 >> 5) == 6 ? 2 : (c0 >> 4) == 14 ? 3 : 4;
        uint32_t v = 0;
        std::memcpy(&v, s.data() + i, n); // LVGL espera el carácter UTF-8 empaquetado
        key(v);
        i += n;
    }
}

static bool hidden_up(lv_obj_t *o)
{
    for (; o; o = lv_obj_get_parent(o))
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN))
            return true;
    return false;
}

static void collect(lv_obj_t *o, std::string &out)
{
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN))
        return;
    if (lv_obj_check_type(o, &lv_label_class)) {
        out += lv_label_get_text(o);
        out += "\n";
    } else if (lv_obj_check_type(o, &lv_textarea_class)) {
        out += lv_textarea_get_text(o);
        out += "\n";
        return;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++)
        collect(lv_obj_get_child(o, i), out);
}

std::string visible_texts()
{
    std::string out;
    collect(lv_screen_active(), out);
    collect(lv_layer_top(), out);
    return out;
}

bool visible_text(const std::string &needle) { return visible_texts().find(needle) != std::string::npos; }

static int count_rec(lv_obj_t *o)
{
    int n = 1;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++)
        n += count_rec(lv_obj_get_child(o, i));
    return n;
}

int count_objects()
{
    int n = count_rec(lv_layer_top());
    if (app)
        for (lv_obj_t *s : {app->home.screen, app->messages_screen.screen, app->quick_screen.screen, app->map_screen.screen,
                            app->team_screen.screen, app->settings_screen.screen})
            n += count_rec(s);
    return n;
}

lv_obj_t *focused() { return app ? lv_group_get_focused(app->active_group()) : nullptr; }

void hook_app()
{
    rec = Log{};
    app->on_send_text = [](const std::string &chat, const std::string &text) { rec.sent.push_back(chat + "|" + text); };
    app->on_sos = [](bool active, const std::string &text, const std::vector<std::string> &chats,
                     const std::vector<uint32_t> &direct) {
        std::string c, d;
        for (auto &x : chats)
            c += (c.empty() ? "" : ",") + x;
        for (auto x : direct)
            d += (d.empty() ? "" : ",") + std::to_string(x);
        rec.sos.push_back(std::string(active ? "1" : "0") + "|" + text + "|" + c + "|" + d);
    };
    app->on_save_file = [](const std::string &path, const std::string &content) {
        if (!rec.sd_ok)
            return false;
        rec.files.push_back(path);
        rec.fs[path] = content;
        return true;
    };
    app->on_append_file = [](const std::string &path, const std::string &content) {
        if (!rec.sd_ok)
            return false;
        rec.appends.push_back(path + "|" + content);
        rec.fs[path] += content;
        return true;
    };
    app->on_delete_file = [](const std::string &path) {
        rec.deleted.push_back(path);
        rec.fs.erase(path);
        return true;
    };
    app->on_read_file = [](const std::string &path, std::string &content) {
        auto it = rec.fs.find(path);
        if (!rec.sd_ok || it == rec.fs.end())
            return false;
        content = it->second;
        return true;
    };
    app->on_alert = [](tfield::Alert k, const std::string &what) {
        static const char *n[] = {"mensaje", "punto", "sos", "prueba"};
        rec.alerts.push_back(std::string(n[(int)k]) + "|" + what);
    };
    app->on_channel_invite = [](uint32_t num, const std::string &, const tfield::ChannelKey &) { rec.invites.push_back(num); };
    app->on_request_position = [](uint32_t num) { rec.pos_requests.push_back(num); };
    app->on_channel_set = [](const std::string &name, const tfield::ChannelKey &) { rec.channel_sets.push_back(name); };
    app->on_settings = [](const tfield::Settings &) { rec.settings_saved++; };
}

} // namespace th

// ------------------------------------------------------------------ main
int main(int argc, char **argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::string only = argc > 1 ? argv[1] : "";
    int total_fail = 0, ran = 0;
    for (auto &t : th::registry()) {
        if (!only.empty() && std::string(t.name).find(only) == std::string::npos)
            continue;
        ran++;
        pid_t pid = fork();
        if (pid == 0) {
            th::init_lvgl();
            if (t.needs_app) {
                th::app = new tfield::App();
                th::app->init();
                th::hook_app();
                demo::load(*th::app, 14, 32);
                th::rec.files.clear(); // lo que escribió la carga de ejemplo no cuenta
                th::rec.appends.clear();
                th::rec.deleted.clear();
                lv_indev_set_group(th::kp, th::app->active_group());
                th::tick(50);
            }
            t.fn();
            std::printf("%-34s %s  (%d comprobaciones)\n", t.name, th::fails ? "FALLA" : "ok", th::passes + th::fails);
            std::fflush(stdout);
            _exit(th::fails > 100 ? 100 : th::fails);
        }
        int st = 0;
        waitpid(pid, &st, 0);
        if (WIFSIGNALED(st)) {
            std::printf("%-34s SE CAYÓ (señal %d)\n", t.name, WTERMSIG(st));
            total_fail++;
        } else if (WEXITSTATUS(st)) {
            if (WEXITSTATUS(st) >= 100)
                std::printf("%-34s ERROR (código %d)\n", t.name, WEXITSTATUS(st));
            total_fail++;
        }
    }
    std::printf("\n%d pruebas, %d con fallas\n", ran, total_fail);
    return total_fail ? 1 : 0;
}
