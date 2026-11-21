// Pruebas de situaciones: la App completa con LVGL y los datos de ejemplo del simulador.
#include "tfield/format.h"
#include "tfield/gpx.h"
#include "demo.h"
#include "harness.h"

#include <algorithm>

using namespace tfield;
using th::app;
using th::rec;

static bool valid_utf8(const std::string &s)
{
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        if (!n || i + n > s.size())
            return false;
        for (size_t k = 1; k < n; k++)
            if (((unsigned char)s[i + k] & 0xC0) != 0x80)
                return false;
        i += n;
    }
    return true;
}

static const TeamNode *node(uint32_t num)
{
    for (auto &n : app->nodes())
        if (n.num == num)
            return &n;
    return nullptr;
}

static lv_obj_t *last_in_group(lv_group_t *g) { return lv_group_get_obj_by_index(g, lv_group_get_obj_count(g) - 1); }

// ------------------------------------------------------------ navegación
APP_TEST(navegacion_esc)
{
    app->show(ScreenId::Team);
    app->show_node(2);
    app->open_direct_chat(2);
    CHECK(app->current() == ScreenId::Messages);
    CHECK_EQ(app->messages_screen.current, std::string("geo2"));
    th::key(LV_KEY_ESC);
    CHECK(app->current() == ScreenId::Team);
    CHECK_EQ(app->team_screen.detail_num(), (uint32_t)2);
    th::key(LV_KEY_ESC);
    CHECK(app->current() == ScreenId::Team);
    CHECK_EQ(app->team_screen.detail_num(), (uint32_t)0);
    th::key(LV_KEY_ESC);
    CHECK(app->current() == ScreenId::Home);
    th::key(LV_KEY_ESC); // en el inicio no pasa nada
    CHECK(app->current() == ScreenId::Home);

    // muchas vistas seguidas: el historial se acota y siempre se vuelve al inicio
    for (int i = 0; i < 30; i++) {
        app->show_node(i % 2 ? 2 : 3);
        app->show(ScreenId::Map);
        app->open_direct_chat(4);
        app->show_settings_section(i % SettingsScreen::S_COUNT);
    }
    for (int i = 0; i < 40; i++)
        th::key(LV_KEY_ESC);
    CHECK(app->current() == ScreenId::Home);

    // Ajustes › Mensajes › Respuestas rápidas › Esc › Esc › Esc
    app->show(ScreenId::Settings);
    app->show_settings_section(SettingsScreen::S_MESSAGES);
    app->show(ScreenId::QuickReplies);
    th::key(LV_KEY_ESC);
    CHECK(app->current() == ScreenId::Settings);
    CHECK_EQ(app->settings_screen.section(), (int)SettingsScreen::S_MESSAGES);
    th::key(LV_KEY_ESC);
    CHECK_EQ(app->settings_screen.section(), -1);
    th::key(LV_KEY_ESC);
    CHECK(app->current() == ScreenId::Home);
    // el teclado siempre maneja el grupo de la pantalla visible
    CHECK(lv_group_get_focused(app->active_group()) != nullptr);
}

// ------------------------------------------------------------------- SOS
APP_TEST(sos_destinos_y_confirmaciones)
{
    app->trigger_sos();
    CHECK(app->sos_active());
    CHECK_EQ(rec.sos.size(), (size_t)1);
    CHECK(rec.sos[0].rfind("1|SOS GEO1 19H", 0) == 0);
    CHECK(rec.sos[0].find("|longfast,terreno|2,3,4") != std::string::npos); // RPT1 (repetidor) no va
    CHECK(th::visible_text("SOS ACTIVO"));
    CHECK(th::visible_text("esperando confirmaci"));
    app->sos_ack(2);
    app->sos_ack(4);
    app->sos_ack(99); // un nodo que no estaba: se ignora
    CHECK(th::visible_text("Lo recibieron: GEO2 y BASE"));
    CHECK(th::visible_text("Esperando: VEH1"));
    th::tick(2 * 60 * 1000 + 100);
    CHECK_EQ(rec.sos.size(), (size_t)2);
    CHECK(rec.sos[1].find("|longfast,terreno|3") != std::string::npos); // reenvío directo solo a quien falta
    // Esc cierra la pantalla roja, pero el SOS sigue activo
    th::key(LV_KEY_ESC);
    CHECK(app->sos_active());
    CHECK(!th::visible_text("SOS ACTIVO"));
    CHECK(th::visible_text("2 de 3 lo recibieron"));
    th::tap(100, 8); // franja roja: vuelve a la pantalla del SOS
    CHECK(th::visible_text("SOS ACTIVO"));
    th::hold(206, 212, 2400); // mantener para cancelar
    CHECK(!app->sos_active());
    CHECK(rec.sos.back().rfind("0|SOS cancelado GEO1", 0) == 0);
    CHECK(rec.sos.back().find("|2,3,4") != std::string::npos);
    CHECK(!th::visible_text("SOS activo"));
}

APP_TEST(sos_sin_destinos)
{
    Settings s = app->settings();
    s.sos_direct = false;
    s.sos_public = false;
    s.sos_team_ch = false;
    app->update_settings(s);
    app->trigger_sos();
    CHECK(rec.sos.back().find("|longfast|") != std::string::npos); // nunca sin destino
    app->cancel_sos();
    // sin canales ni equipo conocidos (recién encendido): canal principal
    app->set_chats({});
    app->set_team({});
    app->trigger_sos();
    CHECK(rec.sos.back().find("|@primary|") != std::string::npos);
    CHECK(th::visible_text("canal principal"));
    app->cancel_sos();
}

APP_TEST(sos_con_teclado)
{
    // en la T-Deck el teclado no informa "tecla mantenida": Enter 3 veces equivale a mantener
    lv_obj_t *sos = last_in_group(app->home.group);
    lv_group_focus_obj(sos);
    th::key(LV_KEY_ENTER);
    th::key(LV_KEY_ENTER);
    CHECK(!app->sos_active());
    th::key(LV_KEY_ENTER);
    CHECK(app->sos_active());
    // un toque corto en la pantalla no activa nada (solo mantener)
    app->cancel_sos();
    th::tap(300, 226);
    th::tap(300, 226);
    th::tap(300, 226);
    CHECK(!app->sos_active());
    // Enter lentos (más de 2 s entre ellos) no cuentan
    lv_group_focus_obj(last_in_group(app->home.group));
    th::key(LV_KEY_ENTER);
    th::tick(2500);
    th::key(LV_KEY_ENTER);
    th::tick(2500);
    th::key(LV_KEY_ENTER);
    CHECK(!app->sos_active());
}

// ------------------------------------------------------------ canal privado
APP_TEST(canal_privado)
{
    std::string fp0 = key_fingerprint(app->settings().channel.key);
    app->remove_channel();
    CHECK(!app->settings().channel.on);
    bool has = false;
    for (auto &c : app->chats())
        has |= c.id == "terreno";
    CHECK(!has);
    app->create_channel();
    CHECK(app->settings().channel.on);
    CHECK_EQ(app->settings().channel.name, std::string("Equipo"));
    CHECK(!rec.channel_sets.empty() && rec.channel_sets.back() == "Equipo");
    app->rename_channel("Muy largo con \xC3\xB1" "and\xC3\xBA");
    CHECK(app->settings().channel.name.size() <= 11);
    CHECK(valid_utf8(app->settings().channel.name));
    std::string before = app->settings().channel.name;
    app->rename_channel("   ");
    CHECK_EQ(app->settings().channel.name, before);
    // Q7XA al equipo: sin clave pública, no se le invita
    app->set_in_team(11, true);
    rec.invites.clear();
    int n = app->invite_team();
    CHECK_EQ(n, 3);
    CHECK(std::find(rec.invites.begin(), rec.invites.end(), 11u) == rec.invites.end());
    CHECK(std::find(rec.invites.begin(), rec.invites.end(), 5u) == rec.invites.end()); // repetidor
    app->channel_joined(2);
    CHECK(app->settings().member(2) && app->settings().member(2)->state == ChannelMember::Joined);
    CHECK_EQ(app->pending_invites(), 2);
    app->show_settings_section(SettingsScreen::S_CHANNEL);
    CHECK(th::visible_text("sin clave p"));
    CHECK(th::visible_text("unido"));
    std::string fp1 = key_fingerprint(app->settings().channel.key);
    app->rekey_channel();
    CHECK(key_fingerprint(app->settings().channel.key) != fp1);
    CHECK_EQ(app->pending_invites(), 3);
    // invitación recibida: reemplaza el canal
    ChannelKey k{};
    k[0] = 9;
    app->receive_channel_invite(3, "Cumbre", k);
    uint32_t inv = 0;
    for (auto &m : app->messages("geo2"))
        (void)m;
    std::string vchat;
    for (auto &c : app->chats())
        if (c.name == "VEH1")
            vchat = c.id;
    CHECK(!vchat.empty());
    for (auto &m : app->messages(vchat))
        if (m.invite_id)
            inv = m.invite_id;
    CHECK(inv != 0);
    CHECK(app->accept_invite(inv));
    CHECK(!app->accept_invite(inv)); // dos veces no
    CHECK_EQ(app->settings().channel.name, std::string("Cumbre"));
    CHECK(app->settings().channel.key == k);
    CHECK(app->settings().member(3) && app->settings().member(3)->state == ChannelMember::Joined);
    int channels_named = 0;
    for (auto &c : app->chats())
        channels_named += c.name == before ? 1 : 0;
    CHECK_EQ(channels_named, 0); // el canal anterior se fue
    // quitar el canal con un SOS activo no deja destinos rotos
    app->trigger_sos();
    app->remove_channel();
    th::tick(2 * 60 * 1000 + 100);
    CHECK(rec.sos.back().find("canal:Cumbre") == std::string::npos);
    app->cancel_sos();
    (void)fp0;
}

// ---------------------------------------------------------------- puntos
APP_TEST(puntos_recibidos)
{
    size_t n0 = app->waypoints().size();
    Waypoint w;
    w.name = "Mirador";
    w.from = "MNT4";
    w.lat = -33.62;
    w.lon = -70.34;
    app->receive_waypoint(w, "longfast"); // otros: preguntar
    CHECK_EQ(app->waypoints().size(), n0);
    uint32_t pid = app->messages("longfast").back().pending_wp;
    CHECK(pid != 0);
    CHECK(app->accept_waypoint(pid) != 0);
    CHECK_EQ(app->waypoints().size(), n0 + 1);
    CHECK_EQ(app->accept_waypoint(pid), (uint32_t)0);
    CHECK(app->messages("longfast").back().waypoint_id != 0);
    // ignorar
    Settings s = app->settings();
    s.pts_others = PointPolicy::Ignore;
    app->update_settings(s);
    app->receive_waypoint(w, "longfast");
    CHECK_EQ(app->waypoints().size(), n0 + 1);
    // del equipo: se guarda solo
    w.from = "GEO2";
    app->receive_waypoint(w, "geo2");
    CHECK_EQ(app->waypoints().size(), n0 + 2);
    // de un nodo sin chat todavía: el chat se crea y el aviso se ve
    s.pts_others = PointPolicy::Ask;
    app->update_settings(s);
    w.from = "NUEVO";
    app->receive_waypoint(w, "node:77");
    bool found = false;
    for (auto &c : app->chats())
        found |= c.id == "node:77";
    CHECK(found);
    CHECK_EQ(app->messages("node:77").size(), (size_t)1);
}

// ------------------------------------------------------------- mensajes
APP_TEST(mensajes_limites)
{
    Message m;
    m.from = "GEO2";
    m.time = "14:40";
    for (int i = 0; i < 300; i++) {
        m.text = "mensaje " + std::to_string(i);
        app->add_message("geo2", m);
    }
    CHECK(app->messages("geo2").size() <= 100); // memoria acotada
    CHECK_EQ(app->messages("geo2").back().text, std::string("mensaje 299"));
    m.text = std::string(300, 'x');
    app->add_message("geo2", m);
    app->show(ScreenId::Messages);
    app->open_chat("geo2");
    app->messages_screen.show_chat("geo2");
    th::tick(100);
    CHECK(lv_obj_get_child_count(app->messages_screen.conversation()) <= 60); // no dibuja cientos de burbujas
    // mensaje a un chat desconocido: se crea
    app->add_message("node:88", m);
    bool found = false;
    for (auto &c : app->chats())
        found |= c.id == "node:88";
    CHECK(found);
    // límite en bytes (Meshtastic): 150 "ñ" son 300 bytes
    app->messages_screen.focus_input();
    std::string many;
    for (int i = 0; i < 150; i++)
        many += "\xC3\xB1";
    th::type(many);
    lv_obj_t *ta = th::focused();
    CHECK(ta && lv_obj_check_type(ta, &lv_textarea_class));
    if (ta)
        CHECK(std::string(lv_textarea_get_text(ta)).size() <= MSG_MAX_BYTES);
    CHECK(th::visible_text("/200"));
    // enviar con Enter
    th::key(LV_KEY_ENTER);
    CHECK(!rec.sent.empty());
    if (!rec.sent.empty())
        CHECK(rec.sent.back().size() <= std::string("geo2|").size() + MSG_MAX_BYTES);
    // vacío o solo espacios: no se envía
    size_t n = rec.sent.size();
    th::type("   ");
    th::key(LV_KEY_ENTER);
    CHECK_EQ(rec.sent.size(), n);
}

APP_TEST(mensajes_no_saltan)
{
    Message m;
    m.from = "GEO2";
    m.time = "14:40";
    for (int i = 0; i < 40; i++) {
        m.text = "linea " + std::to_string(i);
        app->add_message("geo2", m);
    }
    app->show(ScreenId::Messages);
    app->open_chat("geo2");
    app->messages_screen.show_chat("geo2");
    th::tick(50);
    lv_obj_t *conv = app->messages_screen.conversation();
    lv_obj_scroll_to_y(conv, 0, LV_ANIM_OFF); // leyendo lo de arriba
    lv_obj_t *first = lv_obj_get_child(conv, 0);
    // llega telemetría del equipo y un mensaje a otro chat: la conversación no se rehace ni salta
    app->set_team(demo::team(60));
    m.text = "otro chat";
    app->add_message("base", m);
    th::tick(50);
    CHECK(lv_obj_get_child(conv, 0) == first);
    CHECK_EQ((int)lv_obj_get_scroll_y(conv), 0);
}

// ------------------------------------------------- chats fuera del equipo
APP_TEST(chats_fuera_del_equipo_en_gris)
{
    for (auto &c : app->chats()) {
        if (c.id == "longfast")
            CHECK(app->chat_outside_team(c)); // canal público
        if (c.id == "geo2")
            CHECK(!app->chat_outside_team(c)); // compañero
    }
}

// ---------------------------------------------------------------- equipo
APP_TEST(equipo_se_guarda)
{
    // quién está en el equipo se guarda en los ajustes, no se pierde al reiniciar
    int saved = rec.settings_saved;
    app->set_in_team(9, true);
    th::tick(2100); // el guardado se agrupa (flash)
    CHECK(rec.settings_saved > saved);
    auto &t = app->settings().team;
    CHECK(std::find(t.begin(), t.end(), 9u) != t.end());
    // un nodo nuevo que aparece en la red no entra solo al equipo
    auto nodes = demo::team(0);
    TeamNode x;
    x.num = 77;
    x.name = "NUEV";
    x.in_team = true; // lo que traería el adaptador por defecto
    nodes.push_back(x);
    app->set_team(nodes);
    CHECK(node(77) && !node(77)->in_team);
    CHECK(node(9) && node(9)->in_team);
    // otra App con esos ajustes (como al reiniciar)
    App *b = new App();
    b->init();
    b->set_settings(app->settings());
    b->set_team(demo::team(0));
    bool nine = false, eleven = true;
    for (auto &n : b->nodes()) {
        if (n.num == 9)
            nine = n.in_team;
        if (n.num == 11)
            eleven = n.in_team;
    }
    CHECK(nine);
    CHECK(!eleven);
}

APP_TEST(equipo_nodo_desaparece)
{
    app->show(ScreenId::Team);
    app->show_node(3);
    CHECK_EQ(app->team_screen.detail_num(), (uint32_t)3);
    auto nodes = demo::team(0);
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [](const TeamNode &n) { return n.num == 3; }), nodes.end());
    app->set_team(nodes);
    CHECK_EQ(app->team_screen.detail_num(), (uint32_t)0);
    th::key(LV_KEY_ESC);
    th::key(LV_KEY_ESC);
    CHECK(app->current() == ScreenId::Home);
    app->set_team({});
    app->show(ScreenId::Team);
    CHECK(th::visible_text("Sin equipo"));
    app->show(ScreenId::Home);
    CHECK(th::visible_text("Sin equipo"));
    app->request_team_positions(); // sin equipo: no hace nada
    CHECK(rec.pos_requests.empty());
}

APP_TEST(pedir_posiciones)
{
    app->show(ScreenId::Team);
    CHECK_EQ(app->request_team_positions(), 3);
    CHECK(th::visible_text("Pidiendo su posici"));
    app->position_received(2);
    app->position_received(4);
    CHECK(th::visible_text("2 de 3"));
    app->position_received(3);
    CHECK(th::visible_text("Llegaron las 3"));
    th::tick(46000);
    CHECK(th::visible_text("en contacto"));
}

// ------------------------------------------------------------------ mapa
APP_TEST(mapa_sin_gps_y_rutas)
{
    Position p;
    p.valid = false;
    app->set_position(p);
    app->show(ScreenId::Map);
    th::tap(120, 226); // Marcar punto aquí
    CHECK(th::visible_text("Sin posici"));
    app->start_recording();
    app->stop_recording();
    app->show(ScreenId::Home);
    app->show(ScreenId::Map);
    th::tap(40, 226); // Grabar → graba; otra vez → detiene y pregunta
    th::tap(40, 226);
    CHECK(th::visible_text("Ruta detenida"));
    CHECK(th::visible_text("Desechar"));
    CHECK(!th::visible_text("Guardar")); // sin puntos no hay nada que guardar
    th::key(LV_KEY_ESC);                 // Esc: sigue grabando
    CHECK(app->track().recording);
    // nombres de archivo que no chocan
    Track t;
    t.pts = {{-33.64, -70.35, 1500, 1}, {-33.63, -70.34, 1510, 70}};
    app->set_track(t);
    std::string f1 = app->save_track("R1 Subida mirador", "");
    CHECK(f1 != "/tfield/rutas/R1_Subida_mirador.gpx"); // ya existía (ruta de ejemplo)
    app->set_track(t);
    std::string f2 = app->save_track("Otra", "");
    uint32_t id2 = app->saved_tracks().back().id;
    rec.deleted.clear();
    app->update_track(id2, "R1 Subida mirador", "");
    std::vector<std::string> files;
    for (auto &x : app->saved_tracks())
        files.push_back(x.file);
    std::sort(files.begin(), files.end());
    CHECK(std::adjacent_find(files.begin(), files.end()) == files.end()); // sin archivos repetidos
    for (auto &d : rec.deleted)
        CHECK(d != "/tfield/rutas/R1_Subida_mirador.gpx"); // no borra la ruta de otro
    (void)f2;
}

APP_TEST(mapa_mostrar_ruta_la_encuadra)
{
    app->show(ScreenId::Map);
    // ruta lejos (Valle Nevado) y "Mostrar": el mapa la encuadra
    Track t;
    for (int i = 0; i < 20; i++)
        t.pts.push_back({-33.35 + i * 0.001, -70.25 + i * 0.001, 3000, (uint32_t)(1000 + i * 60)});
    app->set_track(t);
    app->save_track("Lejos", "");
    uint32_t id = app->saved_tracks().back().id;
    app->map_screen.show_route(id);
    double la, lo;
    app->map_screen.center(la, lo);
    CHECK_NEAR(la, -33.3405, 0.01);
    CHECK_NEAR(lo, -70.2405, 0.01);
}

// ------------------------------------------------------ respuestas rápidas
APP_TEST(respuestas_esc_sin_guardar)
{
    app->show(ScreenId::QuickReplies);
    lv_group_focus_obj(lv_group_get_obj_by_index(app->quick_screen.group, 1));
    th::type(" ya");
    th::key(LV_KEY_ESC);
    CHECK(app->current() == ScreenId::QuickReplies); // avisa antes de perder lo escrito
    CHECK(th::visible_text("Esc descarta"));
    th::key(LV_KEY_ESC);
    CHECK(app->current() == ScreenId::Home);
    CHECK_EQ(app->quick().text[0], std::string("OK, recibido"));
    // al guardar, el texto queda limitado a 25 caracteres (no bytes)
    app->show(ScreenId::QuickReplies);
    lv_group_focus_obj(lv_group_get_obj_by_index(app->quick_screen.group, 2));
    th::type(" \xC3\xB1\xC3\xB1\xC3\xB1\xC3\xB1\xC3\xB1\xC3\xB1\xC3\xB1\xC3\xB1\xC3\xB1\xC3\xB1\xC3\xB1\xC3\xB1\xC3\xB1");
    CHECK(utf8_length(lv_textarea_get_text(th::focused())) <= QUICK_MAX_CHARS);
}

// --------------------------------------------------------------- hora local
APP_TEST(hora_local)
{
    Status s = app->status();
    s.epoch = epoch_from_utc(2026, 11, 20, 12, 0, 40); // India: UTC+5:30, con segundos
    s.hour = 17;
    s.minute = 30;
    app->set_status(s);
    CHECK_EQ(app->local_hhmm(s.epoch), std::string("17:30"));
    s.epoch = epoch_from_utc(2026, 11, 20, 17, 32, 59); // Chile UTC−3
    s.hour = 14;
    s.minute = 32;
    app->set_status(s);
    CHECK_EQ(app->local_hhmm(s.epoch), std::string("14:32"));
    CHECK_EQ(app->local_date_time(epoch_from_utc(2026, 11, 21, 2, 10, 0)), std::string("20 nov 23:10"));
    // sin hora todavía: el reloj no inventa "00:00"
    s.epoch = 0;
    s.hour = 0;
    s.minute = 0;
    app->set_status(s);
    app->show(ScreenId::Home);
    CHECK(th::visible_text("--:--"));
}

// ------------------------------------------------------------------ ajustes
APP_TEST(ajustes_todas_las_filas)
{
    for (int sec = 0; sec < SettingsScreen::S_COUNT; sec++) {
        app->show(ScreenId::Home);
        app->show(ScreenId::Settings);
        app->show_settings_section(sec);
        // el grupo se vuelve a pedir cada vez: Tema e Idioma rehacen la interfaz (grupos nuevos)
        auto g = [] { return app->settings_screen.group; };
        for (uint32_t i = 1; i < lv_group_get_obj_count(g()); i++) {
            lv_obj_t *o = lv_group_get_obj_by_index(g(), i);
            if (!o || lv_obj_check_type(o, &lv_textarea_class))
                continue;
            for (int k = 0; k < 3; k++) {
                if (i >= lv_group_get_obj_count(g()))
                    break;
                o = lv_group_get_obj_by_index(g(), i);
                lv_group_focus_obj(o);
                lv_obj_send_event(o, LV_EVENT_CLICKED, nullptr);
                th::tick(30);
                th::key(LV_KEY_RIGHT);
                th::key(LV_KEY_LEFT);
            }
        }
        for (int k = 0; k < 6 && app->current() != ScreenId::Home; k++)
            th::key(LV_KEY_ESC);
        CHECK(app->current() == ScreenId::Home);
    }
    // el SOS nunca queda sin destinos
    const Settings &s = app->settings();
    int targets = (s.sos_direct ? 1 : 0) + (s.sos_public ? 1 : 0) + (s.sos_team_ch && s.channel.on ? 1 : 0);
    CHECK(targets >= 1);
}

APP_TEST(ajustes_radio_confirmar)
{
    app->show(ScreenId::Settings);
    app->show_settings_section(SettingsScreen::S_RADIO);
    lv_group_t *g = app->settings_screen.group;
    lv_obj_t *preset = lv_group_get_obj_by_index(g, 2);
    lv_obj_send_event(preset, LV_EVENT_CLICKED, nullptr);
    th::tick(20);
    CHECK_EQ((int)app->settings().preset, 0); // pendiente, no aplicado
    lv_obj_t *apply = last_in_group(g);
    lv_group_focus_obj(apply);
    lv_obj_send_event(apply, LV_EVENT_CLICKED, nullptr);
    th::tick(20);
    CHECK_EQ((int)app->settings().preset, 0); // primera pulsación: pide confirmar
    CHECK(th::visible_text("Pulsa de nuevo"));
    lv_obj_send_event(apply, LV_EVENT_CLICKED, nullptr);
    th::tick(20);
    CHECK_EQ((int)app->settings().preset, 1);
    // salir con un cambio pendiente lo descarta
    app->show_settings_section(SettingsScreen::S_RADIO);
    lv_obj_send_event(lv_group_get_obj_by_index(app->settings_screen.group, 3), LV_EVENT_CLICKED, nullptr);
    th::key(LV_KEY_ESC);
    CHECK_EQ((int)app->settings().hops, 3);
}

APP_TEST(inicio_foco_estable)
{
    // con el teclado sobre el chat de BASE, la llegada de datos no mueve el foco a otro lado
    lv_group_t *g = app->home.group;
    auto row_name = [](lv_obj_t *o) -> std::string {
        if (!o || lv_obj_get_child_count(o) < 2 || !lv_obj_check_type(lv_obj_get_child(o, 1), &lv_label_class))
            return "";
        return lv_label_get_text(lv_obj_get_child(o, 1));
    };
    lv_obj_t *base = nullptr;
    for (uint32_t i = 0; i < lv_group_get_obj_count(g); i++) {
        lv_obj_t *o = lv_group_get_obj_by_index(g, i);
        if (lv_obj_get_width(o) == 147 && row_name(o) == "BASE")
            base = o;
    }
    CHECK(base != nullptr);
    if (!base)
        return;
    lv_group_focus_obj(base);
    app->set_team(demo::team(120));
    Message m;
    m.from = "BASE";
    m.time = "14:41";
    m.text = "hola";
    app->add_message("base", m); // BASE sube al primer lugar
    m.from = "Q7XA";
    app->add_message("longfast", m);
    th::tick(30);
    CHECK_EQ(row_name(lv_group_get_focused(g)), std::string("BASE"));
}
