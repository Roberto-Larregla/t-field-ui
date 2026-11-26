// Simulador sin pantalla: dibuja cada vista y la guarda como imagen RGB565 cruda.
// Uso: sim_png <carpeta_salida>
#include "tfield/app.h"
#include "tfield/i18n.h"
#include "demo.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static uint16_t fb[tfield::SCREEN_W * tfield::SCREEN_H];
static uint16_t draw_buf[tfield::SCREEN_W * tfield::SCREEN_H];

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    int32_t w = lv_area_get_width(a);
    for (int32_t y = a->y1; y <= a->y2; y++)
        std::memcpy(&fb[y * tfield::SCREEN_W + a->x1], px + (y - a->y1) * w * 2, w * 2);
    lv_display_flush_ready(d);
}

static void render(const std::string &path)
{
    lv_obj_invalidate(lv_screen_active());
    for (int i = 0; i < 30; i++) {
        lv_tick_inc(20);
        lv_timer_handler();
    }
    lv_refr_now(nullptr);
    FILE *f = std::fopen(path.c_str(), "wb");
    std::fwrite(fb, 2, sizeof(fb) / 2, f);
    std::fclose(f);
    std::printf("%s\n", path.c_str());
}

static void click(lv_obj_t *o)
{
    if (o)
        lv_obj_send_event(o, LV_EVENT_CLICKED, nullptr);
}

// texto de un objeto y sus hijos (para buscar botones por lo que dicen, no por su posición)
static bool has_text(lv_obj_t *o, const char *txt, bool exact)
{
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *t = lv_label_get_text(o);
        if (exact ? std::strcmp(t, txt) == 0 : std::strstr(t, txt) != nullptr)
            return true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++)
        if (has_text(lv_obj_get_child(o, (int32_t)i), txt, exact))
            return true;
    return false;
}

// exact: el rótulo dice eso y nada más ("GEO2", no "GEO2, VEH1, BASE")
static lv_obj_t *by_text(lv_group_t *g, const char *txt, bool exact = false)
{
    for (uint32_t i = 0; i < lv_group_get_obj_count(g); i++) {
        lv_obj_t *o = lv_group_get_obj_by_index(g, i);
        if (has_text(o, txt, exact))
            return o;
    }
    std::printf("  (no encontré \"%s\")\n", txt);
    return nullptr;
}

int main(int argc, char **argv)
{
    std::string out = argc > 1 ? argv[1] : ".";
    lv_init();
    lv_display_t *d = lv_display_create(tfield::SCREEN_W, tfield::SCREEN_H);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, draw_buf, nullptr, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush_cb);

    static tfield::App app;
    app.init();
    demo::attach_files(app);
    demo::load(app, 14, 32);
    // sim_png <carpeta> [es|en] [claro|oscuro]
    if (argc > 2 || argc > 3) {
        tfield::Settings s = app.settings();
        s.lang = argc > 2 && std::string(argv[2]) == "en" ? 1 : 0;
        s.theme = argc > 3 && std::string(argv[3]) == "oscuro" ? 1 : 0;
        app.set_settings(s);
    }

    render(out + "/inicio.raw");

    // Inicio con el chat GEO2 elegido y una respuesta enviada
    lv_obj_t *chat_list = nullptr;
    (void)chat_list;
    // primer chat = GEO2: se elige simulando el clic sobre su fila
    lv_obj_t *row = lv_group_get_obj_by_index(app.home.group, 0);
    click(row);
    render(out + "/inicio_chat.raw");

    app.open_chat("geo2");
    app.show(tfield::ScreenId::Messages);
    render(out + "/mensajes.raw");
    click(by_text(app.messages_screen.group, tfield::tr("Borrar"), true)); // la primera pulsación pregunta
    render(out + "/mensajes_borrar.raw");
    app.messages_screen.focus_input();

    app.show(tfield::ScreenId::QuickReplies);
    render(out + "/respuestas.raw");

    app.show(tfield::ScreenId::Home);
    app.receive_waypoint(demo::shared_from_geo2(), "geo2");
    render(out + "/inicio_compartido.raw");
    app.show(tfield::ScreenId::Map);
    render(out + "/mapa.raw");
    // Mapa: elegir con ‹ ›, el panel Nombres y el mapa con los nombres del equipo
    lv_group_focus_obj(app.map_screen.view);
    if (!app.handle_key(LV_KEY_RIGHT)) // ‹ › sobre el mapa: el marcador más cercano al centro
        lv_group_send_data(app.map_screen.group, LV_KEY_RIGHT);
    render(out + "/mapa_elegido.raw");
    app.handle_key(LV_KEY_ESC);
    click(app.map_screen.layers_button());
    render(out + "/mapa_nombres.raw");
    { // la etiqueta de "Mi equipo" (fila: nombre, cuenta, ojo, etiqueta)
        lv_obj_t *row = by_text(app.map_screen.group, tfield::tr("Mi equipo"), true);
        if (row && lv_obj_get_child_count(row) > 3)
            click(lv_obj_get_child(row, 3));
    }
    app.handle_key(LV_KEY_ESC);
    render(out + "/mapa_con_nombres.raw");
    {
        tfield::Settings st = app.settings(); // lo que sigue, sin nombres (como viene)
        st.map_names = 0;
        st.map_hidden = 0;
        app.update_settings(st);
        app.show(tfield::ScreenId::Map);
    }
    // Rutas -> R1 -> volver al inicio
    click(by_text(app.map_screen.group, "Rutas"));
    render(out + "/mapa_rutas.raw");
    click(by_text(app.map_screen.group, "Subida mirador")); // R1
    render(out + "/mapa_editar_ruta.raw");
    app.handle_key(LV_KEY_ESC);
    // editar el punto recibido
    click(by_text(app.map_screen.group, "Puntos"));
    render(out + "/mapa_puntos.raw");
    app.handle_key(LV_KEY_ESC);
    // SOS
    // compartir: Marcar punto -> Compartir -> selector
    click(by_text(app.map_screen.group, "Marcar"));
    click(by_text(app.map_screen.active_group(), "Compartir"));
    click(by_text(app.map_screen.active_group(), "GEO2", true)); // marcar GEO2
    render(out + "/compartir.raw");
    app.handle_key(LV_KEY_ESC);
    app.open_chat("geo2");
    app.show(tfield::ScreenId::Messages);
    render(out + "/mensaje_punto.raw");
    app.show_waypoint(app.waypoints()[4].id);
    render(out + "/mapa_punto_recibido.raw");
    app.show(tfield::ScreenId::Home);
    app.show(tfield::ScreenId::Team);
    render(out + "/equipo.raw");
    click(lv_group_get_obj_by_index(app.team_screen.group, 2)); // Toda la red
    render(out + "/equipo_red.raw");
    { // más abajo en la lista (la bola sobre una ficha lejana): "5–8 de 6"
        lv_group_t *g = app.team_screen.group;
        lv_group_focus_obj(lv_group_get_obj_by_index(g, lv_group_get_obj_count(g) - 1));
        render(out + "/equipo_red_abajo.raw");
    }
    app.show_node(2);
    app.traceroute_result(2, "GEO1 \xE2\x80\xBA GEO2, directo \xC2\xB7 SNR 6,5 dB");
    render(out + "/equipo_ficha.raw");
    app.show_node_on_map(2);
    render(out + "/mapa_rastro.raw");
    app.show(tfield::ScreenId::Home);
    app.trigger_sos();
    render(out + "/sos.raw");
    app.sos_ack(2);
    app.sos_ack(4);
    render(out + "/sos_confirmado.raw");
    app.cancel_sos();

    // Ajustes: cuadrícula y cada sección (las largas, también desplazadas hacia abajo)
    app.show(tfield::ScreenId::Home);
    app.show(tfield::ScreenId::Settings);
    render(out + "/ajustes.raw");
    { // un modo en uso (Campamento no toca el tema: el resto de las capturas sigue igual)
        tfield::Settings before = app.settings();
        click(lv_group_get_obj_by_index(app.settings_screen.group, 4));
        for (int i = 0; i < 3; i++) {
            lv_tick_inc(10);
            lv_timer_handler();
        }
        render(out + "/ajustes_modo.raw");
        app.show_settings_section(tfield::SettingsScreen::S_MODE0 + 3); // página del modo (en uso)
        render(out + "/ajustes_modo_campamento.raw");
        app.back();
        app.update_settings(before);
        app.show_settings_section(tfield::SettingsScreen::S_MODE0 + 0); // página de un modo que no está en uso
        render(out + "/ajustes_modo_terreno.raw");
        app.back();
    }
    const char *names[] = {"pantalla", "gps", "mensajes", "mapa", "canal", "sos", "nodo", "radio"};
    for (int i = 0; i < tfield::SettingsScreen::S_COUNT; i++) {
        app.show_settings_section(i);
        render(out + "/ajustes_" + names[i] + ".raw");
        if (i == tfield::SettingsScreen::S_CHANNEL) { // la última fila enfocada: la página se desplaza hasta ella
            lv_group_t *g = app.settings_screen.group;
            lv_obj_t *last = lv_group_get_obj_by_index(g, lv_group_get_obj_count(g) - 1);
            lv_group_focus_obj(last);
            render(out + "/ajustes_" + names[i] + "_b.raw");
        }
        app.back();
    }
    // Radio con un cambio pendiente y la confirmación armada
    app.show_settings_section(tfield::SettingsScreen::S_RADIO);
    {
        lv_group_t *g = app.settings_screen.group;
        lv_obj_t *preset = lv_group_get_obj_by_index(g, 2);
        click(preset);
        lv_obj_t *apply = lv_group_get_obj_by_index(g, 4);
        lv_group_focus_obj(apply);
        click(apply);
        for (int i = 0; i < 3; i++) {
            lv_tick_inc(10);
            lv_timer_handler();
        }
        render(out + "/ajustes_radio_confirmar.raw");
    }
    app.back();
    // Equipo: pedir posiciones a todos
    app.show(tfield::ScreenId::Home);
    app.show(tfield::ScreenId::Team);
    app.request_team_positions();
    app.position_received(2);
    render(out + "/equipo_pedido.raw");
    // Mensajes: punto de alguien fuera del equipo, esperando "Guardar punto"
    {
        tfield::Waypoint w;
        w.name = "Mirador norte";
        w.from = "MNT4";
        w.time = "14:40";
        w.lat = -33.62;
        w.lon = -70.34;
        app.receive_waypoint(w, "longfast");
        tfield::Message m;
        m.from = "Q7XA";
        m.from_color = 0x6E7B85;
        m.time = "14:41";
        m.text = "\xC2\xBF" "Alguien cerca del embalse?";
        app.add_message("longfast", m);
        app.show(tfield::ScreenId::Home);
        app.show(tfield::ScreenId::Messages);
        app.open_chat("longfast");
        app.messages_screen.show_chat("longfast");
        render(out + "/mensajes_punto_ajeno.raw");
        app.accept_waypoint(1);
        render(out + "/mensajes_punto_guardado.raw");
    }
    // Invitación recibida de GEO2 a otro canal (lado de quien recibe)
    {
        tfield::ChannelKey k{};
        k[0] = 7;
        app.receive_channel_invite(2, "Cumbre", k);
        app.show(tfield::ScreenId::Home);
        app.show(tfield::ScreenId::Messages);
        app.open_chat("geo2");
        app.messages_screen.show_chat("geo2");
        render(out + "/invitacion.raw");
        app.accept_invite(1);
        render(out + "/invitacion_aceptada.raw");
        app.show_settings_section(tfield::SettingsScreen::S_CHANNEL);
        render(out + "/invitacion_canal.raw");
    }
    // Compartir con "Todo mi equipo"
    app.show(tfield::ScreenId::Home);
    app.show(tfield::ScreenId::Map);
    click(by_text(app.map_screen.group, "Marcar"));
    click(by_text(app.map_screen.active_group(), "Compartir"));
    click(by_text(app.map_screen.active_group(), "Todo mi equipo"));
    render(out + "/compartir_equipo.raw");
    app.handle_key(LV_KEY_ESC);

    // Inicio: de noche, sin ruta en curso, y sin posición (sin barra de luz)
    app.show(tfield::ScreenId::Home);
    app.stop_recording();
    app.set_status(demo::status(21, 40));
    render(out + "/inicio_noche.raw");
    tfield::Status st = demo::status(14, 32);
    st.daylight_min = -1;
    st.gps_fix = false;
    st.sats = 0;
    app.set_status(st);
    render(out + "/inicio_sin_posicion.raw");

    return 0;
}
