// Tema (claro, oscuro, automático) e idioma: la interfaz se rehace sin perder el lugar
#include "demo.h"
#include "harness.h"
#include "tfield/i18n.h"

#include <cctype>
#include <set>

using namespace tfield;
using th::app;

static size_t lv_used()
{
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    return m.total_size - m.free_size;
}

static double luma(lv_color_t c) { return (0.2126 * c.red + 0.7152 * c.green + 0.0722 * c.blue) / 255.0; }

APP_TEST(tema_oscuro_vuelve_al_mismo_lugar)
{
    app->show(ScreenId::Map);
    app->map_screen.zoom_by(1);
    uint8_t z = app->map_screen.zoom();
    double la0, lo0;
    app->map_screen.center(la0, lo0);
    app->show_settings_section(SettingsScreen::S_SCREEN);
    CHECK(!g_dark);
    for (int i = 0; i < 3; i++) // Brillo, Apagar pantalla, Luz del teclado…
        th::key(LV_KEY_DOWN);
    th::key(LV_KEY_RIGHT); // …Tema › Oscuro
    th::tick(50);
    CHECK(g_dark);
    CHECK_EQ((int)app->settings().theme, 1);
    CHECK(app->current() == ScreenId::Settings);
    CHECK_EQ(app->settings_screen.section(), (int)SettingsScreen::S_SCREEN);
    CHECK(luma(lv_obj_get_style_bg_color(lv_screen_active(), LV_PART_MAIN)) < 0.2);
    CHECK(th::visible_text("Oscuro"));
    th::key(LV_KEY_RIGHT); // sigue en la misma fila: Automático
    th::tick(50);
    CHECK_EQ((int)app->settings().theme, 2);
    th::key(LV_KEY_ESC);
    CHECK(app->current() == ScreenId::Map); // la historia se mantiene
    double la1, lo1;
    app->map_screen.center(la1, lo1);
    CHECK_EQ((int)app->map_screen.zoom(), (int)z);
    CHECK_NEAR(la1, la0, 1e-9);
}

APP_TEST(tema_sin_fugas_de_memoria)
{
    Settings s = app->settings();
    s.theme = 1;
    app->update_settings(s);
    th::tick(50);
    s.theme = 0;
    app->update_settings(s);
    th::tick(50);
    size_t base = lv_used();
    int objs = th::count_objects();
    for (int i = 0; i < 12; i++) {
        s.theme = (uint8_t)(i % 2);
        s.lang = (uint8_t)((i / 2) % 2);
        app->update_settings(s);
        th::tick(30);
    }
    s.theme = 0;
    s.lang = 0;
    app->update_settings(s);
    th::tick(50);
    std::printf("  LVGL: %u KB antes, %u KB después de 13 cambios; objetos %d → %d\n", (unsigned)(base / 1024),
                (unsigned)(lv_used() / 1024), objs, th::count_objects());
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    CHECK(lv_used() <= base + 8 * 1024);
#endif
    CHECK(th::count_objects() <= objs + 10);
}

APP_TEST(tema_automatico_de_noche)
{
    Settings s = app->settings();
    s.theme = 2;
    app->update_settings(s);
    th::tick(50);
    CHECK(!g_dark); // 14:32, de día
    app->show(ScreenId::QuickReplies);
    Status st = app->status();
    st.daylight_min = 0; // anocheció
    app->set_status(st);
    th::tick(50);
    CHECK(!g_dark); // editando respuestas rápidas: espera
    app->show(ScreenId::Home);
    th::tick(50);
    CHECK(g_dark); // en el inicio, sí
    CHECK(app->current() == ScreenId::Home);
}

// ------------------------------------------- inglés: nada queda en español
static void collect(lv_obj_t *o, std::vector<std::string> &out)
{
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN))
        return;
    if (lv_obj_check_type(o, &lv_label_class))
        out.push_back(lv_label_get_text(o));
    if (lv_obj_check_type(o, &lv_textarea_class))
        out.push_back(lv_textarea_get_placeholder_text(o));
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++)
        collect(lv_obj_get_child(o, (int32_t)i), out);
}

static bool spanish(const std::string &t)
{
    static const char *marks[] = {"\xC3\xA1", "\xC3\xA9", "\xC3\xAD", "\xC3\xB3", "\xC3\xBA", "\xC3\xB1", "\xC2\xBF", "\xC2\xA1"};
    for (auto *m : marks)
        if (t.find(m) != std::string::npos)
            return true;
    static const char *words[] = {" de ", " la ", " el ", " con ", " sin ", " para ", " por ", "hace ", "Sin ", "Toca", "toca",
                                  "Guardar", "Ruta", "Punto", "Equipo", "Mapa", "Ajustes", "Mensaje", "Canal", "Nodo",
                                  "Enviar", "Volver", "Borrar", "Cerrar", "equipo", "posici"};
    std::string s = " " + t + " ";
    for (auto *w : words)
        if (s.find(w) != std::string::npos)
            return true;
    return false;
}

static std::set<std::string> data_words()
{
    std::set<std::string> d; // datos de ejemplo (nombres, mensajes): no se traducen
    auto add = [&](const std::string &s) {
        if (s.size() >= 4)
            d.insert(s);
    };
    for (auto &c : app->chats()) {
        add(c.name);
        add(c.preview);
        for (auto &m : app->messages(c.id))
            add(m.text);
    }
    for (auto &n : app->nodes())
        add(n.name), add(n.long_name), add(n.role), add(n.hw);
    for (auto &w : app->waypoints())
        add(w.name), add(w.note);
    for (auto &t : app->saved_tracks())
        add(t.name), add(t.note);
    add(app->settings().channel.name);
    add(app->settings().long_name);
    return d;
}

static void check_screen(const char *where, std::set<std::string> &bad)
{
    th::tick(30);
    std::vector<std::string> texts;
    collect(lv_screen_active(), texts);
    collect(lv_layer_top(), texts);
    auto data = data_words();
    for (auto &t : texts) {
        if (t.empty() || !spanish(t))
            continue;
        std::string cut = t.size() > 3 && t.compare(t.size() - 3, 3, "...") == 0 ? t.substr(0, t.size() - 3) : t;
        bool is_data = false;
        for (auto &w : data) // también recortado con "..." (rótulos de un renglón)
            is_data |= t.find(w) != std::string::npos || w.find(cut) != std::string::npos;
        if (!is_data && bad.insert(t).second)
            std::printf("  [%s] %s\n", where, t.c_str());
    }
}

static void click_text(lv_group_t *g, const char *txt)
{
    for (uint32_t i = 0; i < lv_group_get_obj_count(g); i++) {
        lv_obj_t *o = lv_group_get_obj_by_index(g, i);
        std::vector<std::string> t;
        collect(o, t);
        for (auto &s : t)
            if (s.find(txt) != std::string::npos) {
                lv_obj_send_event(o, LV_EVENT_CLICKED, nullptr);
                th::tick(30);
                return;
            }
    }
}

APP_TEST(ingles_sin_textos_en_espanol)
{
    Settings s = app->settings();
    s.lang = 1;
    app->update_settings(s);
    th::tick(50);
    CHECK(english());
    CHECK(th::visible_text("Messages"));
    CHECK(th::visible_text("OK, got it")); // respuestas rápidas sin tocar: en inglés
    std::set<std::string> bad;
    check_screen("inicio", bad);
    app->open_direct_chat(2);
    check_screen("mensajes", bad);
    app->show(ScreenId::QuickReplies);
    check_screen("respuestas", bad);
    app->show(ScreenId::Home);
    app->show(ScreenId::Map);
    check_screen("mapa", bad);
    click_text(app->map_screen.group, "Points");
    check_screen("mapa · puntos", bad);
    app->handle_key(LV_KEY_ESC);
    click_text(app->map_screen.group, "Tracks");
    check_screen("mapa · rutas", bad);
    click_text(app->map_screen.group, "Subida");
    check_screen("mapa · ruta", bad);
    app->handle_key(LV_KEY_ESC);
    click_text(app->map_screen.group, "Mark");
    check_screen("mapa · punto nuevo", bad);
    click_text(app->map_screen.active_group(), "Share");
    check_screen("mapa · compartir", bad);
    app->handle_key(LV_KEY_ESC);
    app->start_recording();
    app->stop_recording();
    check_screen("mapa · ruta detenida", bad);
    app->handle_key(LV_KEY_ESC);
    app->show(ScreenId::Team);
    check_screen("equipo", bad);
    click_text(app->team_screen.group, "Whole mesh");
    check_screen("equipo · red", bad);
    app->show_node(2);
    check_screen("equipo · ficha", bad);
    app->show(ScreenId::Settings);
    check_screen("ajustes", bad);
    for (int sec = 0; sec < SettingsScreen::S_COUNT; sec++) {
        app->show_settings_section(sec);
        check_screen("ajustes · sección", bad);
    }
    app->show(ScreenId::Home);
    app->trigger_sos();
    check_screen("sos", bad);
    app->cancel_sos();
    for (auto &m : tr_missing())
        if (spanish(m))
            std::printf("  sin traducción: %s\n", m.c_str());
    CHECK(bad.empty());
}
