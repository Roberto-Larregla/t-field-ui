// Pruebas automáticas de T-Field UI: utilidades comunes.
// Cada prueba corre en su propio proceso (fork), con LVGL recién iniciado y una App nueva.
#pragma once

#include "tfield/app.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace th {

extern int fails, passes;
extern tfield::App *app;

inline std::string to_s(const std::string &s) { return "\"" + s + "\""; }
inline std::string to_s(const char *s) { return to_s(std::string(s ? s : "(null)")); }
inline std::string to_s(bool b) { return b ? "true" : "false"; }
template <typename T> std::string to_s(const T &v) { return std::to_string(v); }

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (cond)                                                                                                      \
            th::passes++;                                                                                              \
        else {                                                                                                         \
            th::fails++;                                                                                               \
            std::printf("  FALLA %s:%d  %s\n", __FILE__, __LINE__, #cond);                                             \
        }                                                                                                              \
    } while (0)

#define CHECK_EQ(a, b)                                                                                                 \
    do {                                                                                                               \
        auto _a = (a);                                                                                                 \
        auto _b = (b);                                                                                                 \
        if (_a == _b)                                                                                                  \
            th::passes++;                                                                                              \
        else {                                                                                                         \
            th::fails++;                                                                                               \
            std::printf("  FALLA %s:%d  %s == %s\n     obtenido: %s\n     esperado: %s\n", __FILE__, __LINE__, #a, #b,  \
                        th::to_s(_a).c_str(), th::to_s(_b).c_str());                                                   \
        }                                                                                                              \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                                                          \
    do {                                                                                                               \
        double _a = (a), _b = (b);                                                                                     \
        if (std::fabs(_a - _b) <= (tol))                                                                               \
            th::passes++;                                                                                              \
        else {                                                                                                         \
            th::fails++;                                                                                               \
            std::printf("  FALLA %s:%d  %s ≈ %s  (%.4f vs %.4f, tol %.4f)\n", __FILE__, __LINE__, #a, #b, _a, _b,     \
                        (double)(tol));                                                                                \
        }                                                                                                              \
    } while (0)

// Registro de pruebas
struct Test {
    const char *name;
    bool needs_app; // con LVGL + App + datos de ejemplo
    void (*fn)();
};
std::vector<Test> &registry();
struct Reg {
    Reg(const char *n, bool app, void (*f)()) { registry().push_back({n, app, f}); }
};
#define TEST(name) static void name(); static th::Reg reg_##name(#name, false, name); static void name()
#define APP_TEST(name) static void name(); static th::Reg reg_##name(#name, true, name); static void name()

// Entrada simulada (como en el simulador web)
void tick(uint32_t ms);
void tap(int32_t x, int32_t y);
void hold(int32_t x, int32_t y, uint32_t ms);
void key(uint32_t k);           // pasa por App::handle_key, igual que el teclado de la T-Deck
void type(const std::string &utf8);
// Texto visible en la pantalla activa o en la capa superior (SOS)
bool visible_text(const std::string &needle);
std::string visible_texts();
int count_objects();
lv_obj_t *focused();

// Hardware: píxeles enviados a la pantalla (SPI) desde el último reset
extern uint64_t flushed_px;
// píxeles que redibuja una acción (se deja correr el refresco de pantalla)
uint64_t pixels_for(const std::function<void()> &action);
// Bola de la T-Deck tal como la usará el adaptador: flechas y Enter, máx. 4 movimientos/s
void ball(uint32_t dir);                   // LV_KEY_UP/DOWN/LEFT/RIGHT
void ball_press(uint32_t ms);              // clic mantenido ms (la bola sí informa "mantenido")
// Teclado de la T-Deck: una tecla = un código, sin "mantenida", sin Esc ni flechas
void kb(uint32_t code);
void kb_text(const std::string &utf8);
constexpr uint32_t KEY_DIACRITIC = 0x110; // Shift+$ en la T-Deck (device-ui)

// Eventos registrados por los ganchos de la App
struct Log {
    std::vector<std::string> sent;   // "chat|texto"
    std::vector<std::string> sos;    // "1|texto|chats|nodos"
    std::vector<std::string> files;  // rutas guardadas
    std::vector<std::string> appends; // "ruta|contenido" agregado al final
    bool sd_ok = true;                // false: simula tarjeta SD ausente o llena
    std::map<std::string, std::string> fs; // contenido de la tarjeta SD simulada
    std::vector<std::string> cleared;      // chats borrados ("id|1" si desapareció)
    std::vector<std::string> deleted;
    std::vector<uint32_t> invites, pos_requests;
    std::vector<std::string> channel_sets;
    int settings_saved = 0;
};
extern Log rec;
void hook_app(); // conecta los ganchos al registro (th::rec)

} // namespace th
