// Pruebas con las capacidades de la T-Deck Plus:
//  - pantalla ST7789 de 320×240 por SPI a 40 MHz (cada redibujo es tiempo de SPI y batería)
//  - LVGL con 5 MB en PSRAM (RAM_SIZE=5120 en Meshtastic) compartidos con la interfaz oficial
//  - ESP32-S3 a 240 MHz; Meshtastic guarda hasta 250 nodos
//  - teclado T-Deck: un código por tecla, sin "mantenida", sin Esc ni flechas; tildes con Shift+$
//  - bola: flechas y clic (el clic sí se puede mantener), máximo 4 movimientos por segundo
//  - tarjeta SD que puede faltar, y apagones (batería)
#include "tfield/format.h"
#include "tfield/gpx.h"
#include "demo.h"
#include "harness.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <malloc.h>

using namespace tfield;
using th::app;
using th::rec;

static constexpr double SCREEN_PX = 320.0 * 240.0;
static double pct(uint64_t px) { return 100.0 * (double)px / SCREEN_PX; }
static double spi_ms(uint64_t px) { return (double)px * 16.0 / 40e6 * 1000.0; } // RGB565 a 40 MHz

static void report(const char *what, uint64_t px, double budget_pct)
{
    std::printf("  %-44s %5.1f %%  %5.1f ms SPI  (tope %.0f %%)\n", what, pct(px), spi_ms(px), budget_pct);
    CHECK(pct(px) <= budget_pct);
}

// ------------------------------------------------------- redibujo (SPI)
APP_TEST(hw_redibujo)
{
    th::tick(300);
    report("inicio: cambia el minuto", th::pixels_for([] { app->set_status(demo::status(14, 33)); }), 6);
    report("inicio: telemetría igual de 6 nodos", th::pixels_for([] { app->set_team(demo::team(0)); }), 1);
    report("inicio: 60 s después (\"hace…\")", th::pixels_for([] { app->set_team(demo::team(60)); }), 5);
    report("inicio: fix de GPS (mapa no visible)", th::pixels_for([] {
               Position p = app->position();
               p.lat += 0.00003;
               app->set_position(p);
           }), 0);
    report("inicio: mensaje a otro chat", th::pixels_for([] {
               Message m;
               m.from = "BASE";
               m.time = "14:33";
               m.text = "ok";
               app->add_message("base", m);
           }), 25);
    app->show(ScreenId::Map);
    th::tick(300);
    report("mapa: telemetría igual", th::pixels_for([] { app->set_team(demo::team(60)); }), 1);
    report("mapa: fix de GPS con 3 m de ruido", th::pixels_for([] {
               Position p = app->position();
               p.lat += 0.00003; // ~3 m: el punto y las coordenadas UTM
               app->set_position(p);
           }), 6);
    report("mapa: fix de GPS a 400 m (recentra)", th::pixels_for([] {
               Position p = app->position();
               p.lat += 0.0036;
               app->set_position(p);
           }), 80);
    report("mapa: un punto más en la ruta grabando", th::pixels_for([] {
               Position p = app->position();
               app->add_track_point({p.lat + 0.0001, p.lon, (float)p.alt, app->status().epoch + 600});
           }), 10);
    app->show(ScreenId::Team);
    th::tick(300);
    report("equipo: telemetría igual", th::pixels_for([] { app->set_team(demo::team(60)); }), 1);
    app->show(ScreenId::Settings);
    app->show_settings_section(SettingsScreen::S_SCREEN);
    th::tick(300);
    report("ajustes: cambiar el brillo", th::pixels_for([] { th::key(LV_KEY_RIGHT); }), 12);
}

// -------------------------------------------- solo teclado T-Deck y bola
APP_TEST(hw_teclado_y_bola)
{
    // Ajustes con la tecla 4 (Alt+... en la T-Deck), brillo con la bola, volver con Borrar
    th::kb('4');
    CHECK(app->current() == ScreenId::Settings);
    th::ball_press(120); // primera casilla: Pantalla
    CHECK_EQ(app->settings_screen.section(), (int)SettingsScreen::S_SCREEN);
    th::ball(LV_KEY_RIGHT);
    CHECK_EQ((int)app->settings().brightness, 80); // de 70 al siguiente: 80
    th::ball(LV_KEY_LEFT);
    th::ball(LV_KEY_LEFT);
    CHECK_EQ((int)app->settings().brightness, 40); // 80 → 60 → 40
    th::kb(LV_KEY_BACKSPACE); // sin Esc en el teclado: Borrar es "volver"
    CHECK_EQ(app->settings_screen.section(), -1);
    th::kb(LV_KEY_BACKSPACE);
    CHECK(app->current() == ScreenId::Home);

    // escribir en español: Shift+$ rota la tilde (í, ñ, ¿)
    th::kb('1');
    CHECK(app->current() == ScreenId::Messages);
    th::kb_text("Geologi");
    th::kb(th::KEY_DIACRITIC);
    th::kb_text("a en la quebrada n");
    th::kb(th::KEY_DIACRITIC);
    th::kb_text("o ?");
    th::kb(th::KEY_DIACRITIC);
    lv_obj_t *ta = th::focused();
    CHECK(ta && lv_obj_check_type(ta, &lv_textarea_class));
    if (ta)
        CHECK_EQ(std::string(lv_textarea_get_text(ta)), std::string("Geolog\xC3\xAD" "a en la quebrada \xC3\xB1o \xC2\xBF"));
    th::kb(LV_KEY_BACKSPACE); // dentro del texto, Borrar borra (no sale)
    CHECK(app->current() == ScreenId::Messages);
    th::kb(0x0D); // Enter: envía
    CHECK(!rec.sent.empty() && rec.sent.back().find("Geolog\xC3\xAD" "a") != std::string::npos);

    // mapa con la bola: tomarlo con un clic, moverlo, soltarlo
    app->show(ScreenId::Home);
    th::kb('2');
    CHECK(app->current() == ScreenId::Map);
    lv_group_t *g = app->map_screen.group;
    for (int i = 0; i < 6 && lv_group_get_focused(g) != app->map_screen.view; i++)
        th::ball(LV_KEY_DOWN);
    CHECK(lv_group_get_focused(g) == app->map_screen.view);
    double la0, lo0, la1, lo1;
    app->map_screen.center(la0, lo0);
    th::ball(LV_KEY_LEFT); // sin tomarlo, no se mueve
    app->map_screen.center(la1, lo1);
    CHECK_EQ(lo1, lo0);
    th::ball_press(120);
    CHECK(app->map_screen.pan_mode());
    th::ball(LV_KEY_LEFT);
    th::ball(LV_KEY_UP);
    app->map_screen.center(la1, lo1);
    CHECK(lo1 < lo0); // al oeste
    CHECK(la1 > la0); // al norte
    th::ball(LV_KEY_DOWN); // en modo mover, abajo mueve el mapa (no el foco)
    CHECK(lv_group_get_focused(g) == app->map_screen.view);
    th::ball_press(120); // soltar
    CHECK(!app->map_screen.pan_mode());
    uint8_t z = app->map_screen.zoom();
    th::ball(LV_KEY_DOWN); // ahora sí pasa al botón +
    th::ball_press(120);
    CHECK_EQ((int)app->map_screen.zoom(), (int)z + 1);
    th::kb(LV_KEY_BACKSPACE);
    CHECK(app->current() == ScreenId::Home);

    // SOS manteniendo el clic de la bola 2 s, y cancelarlo igual
    lv_group_focus_obj(lv_group_get_obj_by_index(app->home.group, 0));
    th::ball(LV_KEY_UP); // desde el primer chat, arriba da la vuelta al SOS (último)
    th::ball_press(600); // corto: no
    CHECK(!app->sos_active());
    th::ball_press(2300);
    CHECK(app->sos_active());
    th::ball(LV_KEY_DOWN); // a "Mantén para cancelar"
    th::ball_press(2300);
    CHECK(!app->sos_active());
}

// ---------------------------------------------------- un día pesado
// 10 rutas de días anteriores (3000 puntos cada una) en la tarjeta SD
static void sd_routes()
{
    for (uint32_t r = 1; r <= 10; r++) {
        Track t;
        for (int k = 0; k < 3000; k++)
            t.pts.push_back({-33.66 + k * 0.00001 * r, -70.37 + k * 0.00001, 1500, (uint32_t)(1000 + k * 10)});
        app->add_saved_track("R" + std::to_string(r), "", t);
    }
    rec.appends.clear();
    rec.files.clear();
}

static void heavy_day()
{
    auto nodes = demo::team(0);
    for (uint32_t i = 0; nodes.size() < 250; i++) { // Meshtastic guarda hasta 250 nodos en el ESP32-S3
        TeamNode n;
        n.num = 1000 + i;
        char nm[8];
        snprintf(nm, sizeof(nm), "N%03u", (unsigned)i);
        n.name = nm;
        n.long_name = std::string("Nodo de la red n\xC3\xBAmero ") + nm;
        n.has_pos = i % 5 != 0;
        n.lat = -33.64 + ((int)(i * 37 % 400) - 200) * 0.001;
        n.lon = -70.35 + ((int)(i * 53 % 400) - 200) * 0.001;
        n.dist_m = n.has_pos ? (int32_t)haversine_m(-33.64, -70.35, n.lat, n.lon) : -1;
        n.bearing_deg = (int16_t)(i * 7 % 360);
        n.battery = (int8_t)(i % 100);
        n.signal = (uint8_t)(i % 5);
        n.heard_ago_s = i * 30;
        n.stale = n.heard_ago_s > 1800;
        n.has_pki = true;
        n.hops = (int8_t)(i % 4);
        n.color = 0x6E7B85;
        nodes.push_back(n);
    }
    app->set_team(nodes);
    for (auto &n : nodes) // rastros: 200 posiciones por compañero, 20 por ajeno
        for (int k = 0; k < 220; k++)
            app->add_trail_point(n.num, {n.lat + k * 0.0001, n.lon + k * 0.0001, 1500, (uint32_t)(1000 + k * 60)});
    Message m;
    m.time = "12:00";
    for (int c = 0; c < 40; c++) // 40 chats llenos
        for (int k = 0; k < 70; k++) {
            m.from = c < 4 ? "GEO2" : "N" + std::to_string(c);
            m.text = "Mensaje de prueba con un largo parecido a uno real en terreno, n\xC3\xBAmero " + std::to_string(k);
            app->add_message(c == 0 ? std::string("geo2") : "node:" + std::to_string(1000 + c), m, false);
        }
    std::vector<Waypoint> wps; // 300 puntos de una campaña
    for (uint32_t i = 1; i <= 300; i++) {
        Waypoint w;
        w.id = i;
        w.name = "P" + std::to_string(i) + " muestra";
        w.note = "Nota de terreno del punto " + std::to_string(i);
        w.time = "10:00";
        w.lat = -33.64 + (int)(i % 30) * 0.002;
        w.lon = -70.35 + (int)(i / 30) * 0.002;
        wps.push_back(w);
    }
    app->set_waypoints(wps);
    Track t; // 12 h grabando, un punto cada 5 s
    t.recording = true;
    for (int k = 0; k < 8640; k++)
        t.pts.push_back({-33.64 + k * 0.000005, -70.35 + std::sin(k * 0.01) * 0.002, 1500, (uint32_t)(k * 5)});
    app->set_track(t);
}

static void visit_everything()
{
    app->show(ScreenId::Home);
    th::tick(100);
    app->open_direct_chat(2);
    th::tick(100);
    app->show(ScreenId::Team);
    lv_obj_send_event(lv_group_get_obj_by_index(app->team_screen.group, 2), LV_EVENT_CLICKED, nullptr); // Toda la red
    th::tick(100);
    app->show_node(1001);
    app->show_node_on_map(2);
    th::tick(100);
    app->map_screen.show_route(app->saved_tracks()[3].id); // se lee de la SD
    th::tick(100);
    for (int s = 0; s < SettingsScreen::S_COUNT; s++) {
        app->show_settings_section(s);
        th::tick(50);
    }
    app->show(ScreenId::Map);
    // compartir un punto: la lista de destinatarios
    lv_obj_send_event(lv_group_get_obj_by_index(app->map_screen.group, app->map_screen.group ? 8 : 0), LV_EVENT_CLICKED, nullptr);
    th::tick(100);
}

APP_TEST(hw_memoria)
{
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    sd_routes(); // lo que está en la tarjeta no cuenta como memoria
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    size_t lv_base = mon.total_size - mon.free_size;
    size_t heap0 = mallinfo2().uordblks, tiles0 = demo::tiles_cached();
    heavy_day();
    visit_everything();
    th::tick(200);
    lv_mem_monitor(&mon);
    size_t lv_now = mon.total_size - mon.free_size;
    size_t heap1 = mallinfo2().uordblks;
    size_t tile_bytes = (demo::tiles_cached() - std::min(tiles0, demo::tiles_cached())) * 256 * 256 * 2;
    size_t data = heap1 - heap0 - std::min(heap1 - heap0, tile_bytes);
    std::printf("  LVGL (pool de %u KB como en la T-Deck): base %u KB · día pesado %u KB · máximo %u KB · fragmentación %u %%\n",
                (unsigned)(mon.total_size / 1024), (unsigned)(lv_base / 1024), (unsigned)(lv_now / 1024),
                (unsigned)(mon.max_used / 1024), (unsigned)mon.frag_pct);
    std::printf("  datos de la interfaz: +%u KB (250 nodos, 40 chats, 300 puntos, 10 rutas en la SD, 12 h grabando)\n",
                (unsigned)(data / 1024));
    std::printf("  teselas del mapa en caché: %u (%u KB)\n", (unsigned)demo::tiles_cached(),
                (unsigned)(demo::tiles_cached() * 128));
    std::printf("  objetos LVGL: %d\n", th::count_objects());
    CHECK(mon.max_used < 1536u * 1024u); // menos del 30 % de los 5 MB: el resto para la interfaz oficial
    CHECK(data < 1024u * 1024u);         // PSRAM: 8 MB − 5 MB de LVGL, compartidos con Meshtastic
    CHECK(demo::tiles_cached() <= 8);
    CHECK(th::count_objects() < 3000);
#else
    std::printf("  (sin medir: esta compilación usa malloc del sistema)\n");
    CHECK(true);
#endif
}

// ----------------------------------------------------- tiempo de CPU
// el mejor de n: en un computador compartido, el ruido solo suma tiempo; el mínimo es lo propio
template <typename F> static double best_us(F f, int n = 9)
{
    std::vector<double> t;
    for (int i = 0; i < n; i++) {
        auto a = std::chrono::steady_clock::now();
        f();
        auto b = std::chrono::steady_clock::now();
        t.push_back(std::chrono::duration<double, std::micro>(b - a).count());
    }
    std::sort(t.begin(), t.end());
    return t[0];
}

static void timing(const char *what, double host_us, double budget_ms)
{
    // ESP32-S3 a 240 MHz con datos en PSRAM: ~100 veces más lento que este computador (estimado prudente)
    double est = host_us * 100.0 / 1000.0;
    std::printf("  %-46s %7.0f µs aquí · ~%5.0f ms en la T-Deck (tope %.0f)\n", what, host_us, est, budget_ms);
    if (!std::getenv("TFIELD_SIN_TIEMPOS")) // en servidores compartidos (GitHub) el tiempo varía: solo se informa
        CHECK(est <= budget_ms);
}

APP_TEST(hw_tiempos)
{
#if defined(TFIELD_CHECKS) || !defined(NDEBUG)
    std::printf("  (sin medir: hay que compilar optimizado y sin detectores)\n");
    CHECK(true);
#else
    sd_routes();
    heavy_day();
    // sin mapa base: las teselas las decodifica el adaptador (PNG/JPG de la SD) y tienen su
    // propia caché; aquí se mide lo de la interfaz (líneas, marcadores, rótulos)
    app->set_map_layers({});
    auto nodes = app->nodes();
    app->show(ScreenId::Home);
    th::tick(100);
    timing("telemetría de 250 nodos, en el inicio", best_us([&] { app->set_team(nodes); }), 50);
    app->show(ScreenId::Team);
    lv_obj_send_event(lv_group_get_obj_by_index(app->team_screen.group, 2), LV_EVENT_CLICKED, nullptr);
    th::tick(50);
    timing("telemetría de 250 nodos, en Toda la red", best_us([&] {
               nodes[7].heard_ago_s += 60;
               app->set_team(nodes);
           }), 150);
    app->show(ScreenId::Map);
    th::tick(50);
    app->map_screen.show_route(app->saved_tracks()[5].id);
    timing("mapa: acercar con 12 h de ruta y una guardada", best_us([&] { app->map_screen.zoom_by(1), app->map_screen.zoom_by(-1); }) / 2, 300);
    timing("mapa: mover 20 px (lo más común)", best_us([&] { app->map_screen.pan(20, 0), app->map_screen.pan(-20, 0); }) / 2, 200);
    static int step = 0;
    timing("mapa: un punto más en la ruta grabando", best_us([&] {
               Position p = app->position();
               step++;
               app->add_track_point({p.lat + step * 0.0002, p.lon, (float)p.alt, app->status().epoch + 600 + step * 20});
           }), 60);
    timing("mensaje nuevo en el chat abierto", best_us([&] {
               app->open_direct_chat(2);
               Message m;
               m.from = "GEO2";
               m.time = "14:50";
               m.text = "Recibido";
               app->add_message("geo2", m);
           }), 150);
    timing("entrar a Mensajes (15 burbujas a la vista)", best_us([&] {
               app->show(ScreenId::Home);
               app->open_direct_chat(2);
               app->messages_screen.refresh_conversation(true);
           }), 300);
    app->show(ScreenId::Home);
    timing("dibujar la pantalla de inicio completa", best_us([&] {
               lv_obj_invalidate(lv_screen_active());
               lv_refr_now(nullptr);
           }), 120);
#endif
}

// ------------------------------------------------ flash y tarjeta SD
APP_TEST(hw_flash_y_sd)
{
    // Ajustes: 15 toques seguidos = 1 escritura (2 s después), y al salir de Ajustes se guarda
    app->show(ScreenId::Settings);
    app->show_settings_section(SettingsScreen::S_SCREEN);
    int saved0 = rec.settings_saved;
    for (int i = 0; i < 15; i++)
        th::key(i % 2 ? LV_KEY_LEFT : LV_KEY_RIGHT);
    CHECK_EQ(rec.settings_saved, saved0);
    th::tick(2100);
    CHECK_EQ(rec.settings_saved, saved0 + 1);
    th::key(LV_KEY_RIGHT);
    th::key(LV_KEY_ESC); // a la cuadrícula de Ajustes
    th::key(LV_KEY_ESC); // fuera de Ajustes: guarda al tiro, sin esperar los 2 s
    CHECK_EQ(rec.settings_saved, saved0 + 2);

    // 1 h grabando con GPS a 1 Hz: respaldo en la SD sin reescribir, pocas escrituras
    app->start_recording();
    rec.appends.clear();
    Position p = app->position();
    for (int s = 1; s <= 3600; s++) {
        p.lat += 0.00001;
        app->set_position(p);
        app->add_track_point({p.lat, p.lon, (float)p.alt, app->status().epoch + (uint32_t)s});
    }
    size_t pts = app->track().pts.size();
    std::printf("  1 h grabando: %u puntos, %u escrituras de respaldo\n", (unsigned)pts, (unsigned)rec.appends.size());
    CHECK(pts >= 300 && pts <= 400); // un punto cada 10 s
    CHECK(rec.appends.size() <= pts / 6 + 2);
    // se acaba la batería: al arrancar, la ruta se recupera del respaldo
    std::string csv;
    for (auto &a : rec.appends)
        csv += a.substr(a.find('|') + 1);
    App *b = new App();
    b->init();
    th::app = b;
    th::hook_app();
    demo::load(*b, 14, 32);
    CHECK(b->recover_track(csv));
    CHECK(b->track().pts.size() + 6 >= pts); // a lo más se pierden los últimos 6 puntos
    CHECK(!b->track().recording);
    b->show(ScreenId::Map);
    CHECK(th::visible_text("Ruta recuperada"));
    CHECK(th::visible_text("Guardar"));
    // sin tarjeta SD: la ruta no se pierde, se puede reintentar
    rec.sd_ok = false;
    CHECK(b->save_track("R9", "").empty());
    CHECK(b->track().pts.size() >= 2);
    rec.sd_ok = true;
    std::string f = b->save_track("R9", "");
    CHECK(f == "/tfield/rutas/R9.gpx");
    CHECK(std::find(rec.deleted.begin(), rec.deleted.end(), std::string(App::TRACK_BACKUP)) != rec.deleted.end());
    CHECK(!b->recover_track("# vacío\n1,2,x\n")); // basura: nada que recuperar
    th::app = app;
}

// ------------------------------------------------------- red grande
APP_TEST(hw_250_nodos)
{
    sd_routes();
    heavy_day();
    app->show(ScreenId::Team);
    lv_obj_send_event(lv_group_get_obj_by_index(app->team_screen.group, 2), LV_EVENT_CLICKED, nullptr);
    th::tick(100);
    CHECK(th::visible_text("Se muestran los 50"));
    int rows = 0;
    lv_group_t *g = app->team_screen.group;
    for (uint32_t i = 0; i < lv_group_get_obj_count(g); i++)
        rows += lv_obj_get_width(lv_group_get_obj_by_index(g, i)) == 320 ? 1 : 0;
    CHECK(rows <= 4 + 50 + 1);
    CHECK(app->share_recipients().size() <= 3 + SHARE_OTHERS + 40 + 3);
    int before = th::count_objects();
    app->show(ScreenId::Map);
    app->map_screen.zoom_by(-3); // ver mucha red
    th::tick(100);
    int after = th::count_objects();
    std::printf("  objetos LVGL: %d antes del mapa, %d con 250 nodos y 300 puntos en el mapa\n", before, after);
    // marcadores solo para lo que se ve: mi equipo + 40 nodos + 40 puntos, 2 objetos cada uno
    CHECK(after - before <= (int)(2 * (6 + MAP_NODE_MARKS + MAP_WP_MARKS) + 40));
}

// ------------------------------------------------- rutas en la tarjeta SD
APP_TEST(hw_rutas_en_la_sd)
{
    // 12 h a un punto cada 10 s: el GPX se escribe por partes, sin armarlo entero en memoria
    Track t;
    for (int k = 0; k < 4320; k++)
        t.pts.push_back({-33.64 + k * 0.00002, -70.35 + std::sin(k * 0.01) * 0.002, (float)(1500 + k % 50), (uint32_t)(1700000000 + k * 10)});
    rec.appends.clear();
    std::string file = app->add_saved_track("Quebrada larga", "Muestras M1 a M9", t);
    CHECK(!file.empty());
    size_t biggest = 0;
    for (auto &a : rec.appends)
        biggest = std::max(biggest, a.size() - a.find('|') - 1);
    std::printf("  GPX de 4320 puntos: %u KB escritos en %u partes de hasta %u bytes\n",
                (unsigned)(rec.fs[file].size() / 1024), (unsigned)rec.appends.size(), (unsigned)biggest);
    CHECK(rec.appends.size() > 10);
    CHECK(biggest < 9000);
    const SavedTrack &st = app->saved_tracks().back();
    CHECK(st.track.pts.empty()); // en memoria solo los datos
    CHECK_EQ(st.count, 4320u);
    CHECK(st.track.distance_m > 9000);
    CHECK_EQ(st.start.epoch, 1700000000u);
    // otro arranque: la lista sale del índice, y los puntos de la SD al mostrarla
    App *b = new App();
    b->init();
    App *a0 = app;
    th::app = b;
    b->on_read_file = a0->on_read_file;
    b->on_save_file = a0->on_save_file;
    b->on_append_file = a0->on_append_file;
    b->on_delete_file = a0->on_delete_file;
    CHECK(b->load_routes());
    CHECK_EQ(b->saved_tracks().size(), a0->saved_tracks().size());
    const SavedTrack &bt = b->saved_tracks().back();
    CHECK_EQ(bt.name, std::string("Quebrada larga"));
    CHECK_EQ(bt.note, std::string("Muestras M1 a M9"));
    CHECK_EQ(bt.track.distance_m, st.track.distance_m);
    const std::vector<TrackPoint> *pts = b->route_points(bt.id);
    CHECK(pts && pts->size() == 4320);
    if (pts) {
        CHECK_NEAR(pts->back().lat, t.pts.back().lat, 1e-6);
        CHECK_EQ(pts->back().epoch, t.pts.back().epoch);
    }
    // cambiar el nombre no pierde puntos (se reescribe desde el archivo, no desde la memoria)
    CHECK(b->update_track(bt.id, "Quebrada; norte", ""));
    std::vector<TrackPoint> again;
    CHECK(gpx_parse_track(rec.fs[b->saved_tracks().back().file], again));
    CHECK_EQ(again.size(), (size_t)4320);
    CHECK(rec.fs[b->saved_tracks().back().file].find("<name>Quebrada; norte</name>") != std::string::npos);
    CHECK(!rec.fs.count(file)); // el archivo anterior se borró
    // se corta la energía escribiendo el índice: queda el otro, con la lista anterior
    std::string a_txt = rec.fs[App::ROUTE_INDEX_A], b_txt = rec.fs[App::ROUTE_INDEX_B];
    std::string &newest = b_txt.size() && a_txt.size() ? (rec.fs[App::ROUTE_INDEX_A] > rec.fs[App::ROUTE_INDEX_B] ? rec.fs[App::ROUTE_INDEX_A] : rec.fs[App::ROUTE_INDEX_B]) : rec.fs[App::ROUTE_INDEX_A];
    (void)newest;
    size_t n_before = b->saved_tracks().size();
    b->delete_track(b->saved_tracks().front().id); // escribe el índice nuevo...
    for (const char *f : {App::ROUTE_INDEX_A, App::ROUTE_INDEX_B}) { // ...y se trunca a medias
        std::vector<SavedTrack> l;
        uint32_t g = 0;
        if (parse_route_index(rec.fs[f], l, g) && l.size() == n_before - 1)
            rec.fs[f] = rec.fs[f].substr(0, rec.fs[f].size() / 2);
    }
    App *c = new App();
    c->init();
    th::app = c;
    c->on_read_file = a0->on_read_file;
    CHECK(c->load_routes());
    CHECK_EQ(c->saved_tracks().size(), n_before); // la lista de antes del corte
    // sin tarjeta: "Mostrar" avisa en vez de dibujar nada
    th::app = a0;
    rec.sd_ok = false;
    app->show(ScreenId::Map);
    app->map_screen.show_route(app->saved_tracks().front().id);
    CHECK(th::visible_text("No se pudo leer la ruta"));
    rec.sd_ok = true;
}

// --------------------------------------------- mensajes: solo lo nuevo
APP_TEST(hw_mensajes_incrementales)
{
    Message m;
    m.from = "GEO2";
    m.time = "14:00";
    for (int k = 0; k < 40; k++) {
        m.text = "Mensaje " + std::to_string(k);
        app->add_message("geo2", m, false);
    }
    app->open_direct_chat(2);
    th::tick(100);
    size_t stored = app->messages("geo2").size();
    char want[48];
    snprintf(want, sizeof(want), "Mensajes anteriores (%u)", (unsigned)(stored - CHAT_SHOW_MESSAGES));
    CHECK(th::visible_text(want));
    lv_obj_t *conv = app->messages_screen.conversation();
    CHECK_EQ((int)lv_obj_get_child_count(conv), 16); // 15 burbujas + el botón
    int objs = th::count_objects();
    lv_obj_t *first = lv_obj_get_child(conv, 1);
    m.text = "Llegando al portezuelo";
    app->add_message("geo2", m);
    th::tick(50);
    CHECK_EQ((int)lv_obj_get_child_count(conv), 16);    // una entra, la más vieja sale
    CHECK(lv_obj_get_child(conv, 1) != first);           // ...la de arriba
    CHECK(th::count_objects() - objs < 10);               // no se rehizo la conversación
    CHECK(th::visible_text("Llegando al portezuelo"));
    CHECK(lv_obj_get_scroll_bottom(conv) <= 8);           // y se ve abajo
    // leer lo anterior: de a 15
    lv_obj_send_event(lv_obj_get_child(conv, 0), LV_EVENT_CLICKED, nullptr);
    th::tick(50);
    CHECK_EQ((int)lv_obj_get_child_count(conv), 31);
    // tope total entre chats (canal público concurrido + muchos directos)
    for (int c = 0; c < 20; c++)
        for (int k = 0; k < 40; k++) {
            m.text = "x";
            app->add_message("node:" + std::to_string(2000 + c), m, false);
        }
    size_t total = 0;
    for (auto &ch : app->chats())
        total += app->messages(ch.id).size();
    CHECK(total <= CHAT_TOTAL_MESSAGES);
    CHECK_EQ(app->messages("node:2019").size(), (size_t)40); // el más reciente, entero
}

// ------------------------------------- desnivel con la altura del GPS
APP_TEST(hw_desnivel_con_gps)
{
    // subida lenta: 0,8 m por punto durante 1 h (antes, truncado a 0 en cada paso)
    app->start_recording();
    Position p = app->position();
    double a0 = p.alt;
    for (int k = 1; k <= 360; k++) {
        p.lat += 0.0001;
        p.alt = (int32_t)std::lround(a0 + k * 0.8);
        app->add_track_point({p.lat, p.lon, (float)(a0 + k * 0.8), app->status().epoch + (uint32_t)k * 10});
    }
    int32_t gain = app->track().gain_m, dist = app->track().distance_m;
    std::printf("  subida de 288 m: desnivel %d m, distancia %d m (4 km)\n", (int)gain, (int)dist);
    CHECK(gain >= 288 - (int)CLIMB_STEP_M - 1 && gain <= 289);
    CHECK(dist >= 3990 && dist <= 4020);
    app->discard_track();
    // plano con ruido de ±3 m (sin barómetro): casi nada
    app->start_recording();
    p = app->position();
    for (int k = 1; k <= 360; k++) {
        p.lat += 0.0001;
        float noise = (float)((k * 7919) % 7) - 3.0f;
        app->add_track_point({p.lat, p.lon, (float)p.alt + noise, app->status().epoch + (uint32_t)k * 10});
    }
    std::printf("  plano con ruido de ±3 m: desnivel %d m\n", (int)app->track().gain_m);
    CHECK(app->track().gain_m <= 10);
}
