// Borrar chats: uno desde la conversación, todos desde Ajustes, y los antiguos solos
#include "demo.h"
#include "harness.h"

#include <algorithm>

using namespace tfield;
using th::app;
using th::rec;

static bool has_chat(const std::string &id)
{
    for (auto &c : app->chats())
        if (c.id == id)
            return true;
    return false;
}

static lv_obj_t *find_text(lv_group_t *g, const char *txt)
{
    for (uint32_t i = 0; i < lv_group_get_obj_count(g); i++) {
        lv_obj_t *o = lv_group_get_obj_by_index(g, i);
        for (uint32_t k = 0; k < lv_obj_get_child_count(o); k++) {
            lv_obj_t *c = lv_obj_get_child(o, (int32_t)k);
            if (lv_obj_check_type(c, &lv_label_class) && std::string(lv_label_get_text(c)) == txt)
                return o;
        }
    }
    return nullptr;
}

APP_TEST(borrar_un_chat)
{
    // directo: desaparece, con doble confirmación
    app->open_direct_chat(2); // GEO2
    std::string id = app->messages_screen.current;
    CHECK(!app->messages(id).empty());
    lv_obj_t *del = find_text(app->messages_screen.group, "Borrar");
    CHECK(del != nullptr);
    if (!del)
        return;
    lv_obj_send_event(del, LV_EVENT_CLICKED, nullptr);
    th::tick(30);
    CHECK(has_chat(id)); // la primera pulsación solo pregunta
    CHECK(th::visible_text("\xC2\xBF" "Borrar?"));
    lv_obj_send_event(del, LV_EVENT_CLICKED, nullptr);
    th::tick(30);
    CHECK(!has_chat(id));
    CHECK(app->messages(id).empty());
    CHECK(!rec.cleared.empty() && rec.cleared.back() == id + "|1");
    CHECK(app->current() == ScreenId::Messages);
    // volver a escribirle: Equipo › ficha › Mensaje
    app->show_node(2);
    app->open_direct_chat(2);
    CHECK(app->current() == ScreenId::Messages);
    CHECK(has_chat(app->messages_screen.current));
    app->send(app->messages_screen.current, "Sigo en la quebrada");
    CHECK(!rec.sent.empty());
    // o cuando el otro escribe
    std::string again = app->messages_screen.current;
    app->delete_chat(again);
    Message m;
    m.from = "GEO2";
    m.time = "15:00";
    m.text = "Ya voy";
    app->add_message("node:2", m);
    CHECK(has_chat("node:2"));

    // canal: queda en la lista, vacío
    std::string ch;
    for (auto &c : app->chats())
        if (c.is_channel)
            ch = c.id;
    CHECK(!ch.empty());
    app->delete_chat(ch);
    CHECK(has_chat(ch));
    CHECK(app->messages(ch).empty());
    CHECK(rec.cleared.back() == ch + "|0");
}

APP_TEST(borrar_todos_los_chats)
{
    app->show_settings_section(SettingsScreen::S_MESSAGES);
    lv_obj_t *row = nullptr;
    lv_group_t *g = app->settings_screen.group;
    for (uint32_t i = 0; i < lv_group_get_obj_count(g) && !row; i++) {
        std::vector<std::string> t;
        lv_obj_t *o = lv_group_get_obj_by_index(g, i);
        for (uint32_t k = 0; k < lv_obj_get_child_count(o); k++) {
            lv_obj_t *c = lv_obj_get_child(o, (int32_t)k);
            if (lv_obj_check_type(c, &lv_label_class) && std::string(lv_label_get_text(c)) == "Borrar todos los chats")
                row = o;
        }
    }
    CHECK(row != nullptr);
    if (!row)
        return;
    lv_obj_send_event(row, LV_EVENT_CLICKED, nullptr); // pregunta
    th::tick(30);
    size_t before = app->chats().size();
    CHECK(before > 0);
    lv_obj_send_event(row, LV_EVENT_CLICKED, nullptr); // confirma
    th::tick(30);
    size_t msgs = 0;
    for (auto &c : app->chats()) {
        CHECK(c.is_channel); // solo quedan los canales
        msgs += app->messages(c.id).size();
    }
    CHECK_EQ(msgs, (size_t)0);
}

APP_TEST(borrado_automatico_por_dias)
{
    Settings s = app->settings();
    uint32_t now = app->status().epoch;
    Message m;
    m.from = "VEH1";
    m.time = "10:00";
    m.text = "Mensaje de hace dos d\xC3\xAD" "as";
    m.epoch = now - 2 * 86400;
    app->add_message("node:3", m); // VEH1: un chat directo con solo eso
    m.text = "Mensaje de hoy";
    m.epoch = now - 3600;
    app->add_message("node:7", m);
    std::string old_id = "node:3";
    for (auto &c : app->chats())
        if (c.name == "VEH1")
            old_id = c.id;
    size_t veh_before = app->messages(old_id).size();
    CHECK(veh_before >= 1);
    s.msg_keep_days = 1;
    app->update_settings(s);
    th::tick(30);
    bool found_old = false;
    for (auto &c : app->chats())
        for (auto &x : app->messages(c.id))
            found_old |= x.text == "Mensaje de hace dos d\xC3\xAD" "as";
    CHECK(!found_old);
    bool found_new = false;
    for (auto &c : app->chats())
        for (auto &x : app->messages(c.id))
            found_new |= x.text == "Mensaje de hoy";
    CHECK(found_new);
    // los de hora desconocida (epoch 0, como los de ejemplo) no se tocan
    size_t unknown = 0;
    for (auto &c : app->chats())
        for (auto &x : app->messages(c.id))
            unknown += x.epoch == 0;
    CHECK(unknown > 0);
}

APP_TEST(pestana_con_nombre_largo)
{
    std::vector<Chat> chats = app->chats();
    Chat c;
    c.id = "node:99";
    c.name = "Brigada de muestreo quebrada norte";
    chats.push_back(c);
    app->set_chats(chats);
    app->open_chat("node:99");
    app->show(ScreenId::Messages);
    th::tick(30);
    // la pestaña no crece sin límite (el nombre se corta con "...")
    lv_obj_t *tab = nullptr;
    lv_group_t *g = app->messages_screen.group;
    for (uint32_t i = 0; i < lv_group_get_obj_count(g); i++) {
        lv_obj_t *o = lv_group_get_obj_by_index(g, i);
        for (uint32_t k = 0; k < lv_obj_get_child_count(o); k++) {
            lv_obj_t *l = lv_obj_get_child(o, (int32_t)k);
            if (lv_obj_check_type(l, &lv_label_class) && std::string(lv_label_get_text(l)).find("Brigada") == 0)
                tab = o;
        }
    }
    CHECK(tab != nullptr);
    if (tab)
        CHECK(lv_obj_get_width(tab) <= 150);
    // los datos del chat no llegan al botón Borrar
    lv_obj_t *del = find_text(app->messages_screen.group, "Borrar");
    CHECK(del != nullptr);
    if (!del)
        return;
    lv_area_t b;
    lv_obj_get_coords(del, &b);
    bool meta_ok = false;
    lv_obj_t *hdr = lv_obj_get_parent(del);
    for (uint32_t k = 0; k < lv_obj_get_child_count(hdr); k++) {
        lv_obj_t *l = lv_obj_get_child(hdr, (int32_t)k);
        if (lv_obj_check_type(l, &lv_label_class) && std::string(lv_label_get_text(l)) == "mensaje directo") {
            lv_area_t a;
            lv_obj_get_coords(l, &a);
            meta_ok = a.x2 < b.x1;
        }
    }
    CHECK(meta_ok); // nodo desconocido: sin distancia
    // un compañero con posición: a cuánto y hace cuánto se le escuchó
    app->show_node(2);
    app->open_direct_chat(2);
    th::tick(30);
    CHECK(th::visible_text("directo \xC2\xB7 a "));
}

APP_TEST(estado_del_mensaje_enviado)
{
    uint32_t id = 0;
    app->on_send_text = [&](const std::string &, const std::string &) { id = app->last_sent_id(); };
    app->open_chat("geo2");
    app->send("geo2", "Voy bajando");
    CHECK(id != 0);
    CHECK(app->messages("geo2").back().status == "Enviado");
    app->set_message_status(id, "Entregado");
    CHECK(app->messages("geo2").back().status == "Entregado");
    app->show(ScreenId::Messages);
    th::tick(30);
    CHECK(th::visible_text("Entregado"));
    app->set_message_status(id + 99, "No enviado"); // otro id: no toca nada
    CHECK(app->messages("geo2").back().status == "Entregado");
}

APP_TEST(puntos_se_recuperan_de_la_sd)
{
    Waypoint w = app->draft_point();
    w.name = "Toma <A&B>";
    w.note = "Muestra 3";
    app->add_waypoint(w);
    size_t n = app->waypoints().size();
    CHECK(n >= 1);
    CHECK(rec.fs.count(App::WAYPOINTS_FILE) == 1);
    app->set_waypoints({});
    CHECK(app->waypoints().empty());
    CHECK(app->load_waypoints());
    CHECK_EQ(app->waypoints().size(), n);
    bool found = false;
    for (auto &x : app->waypoints())
        found |= x.name == "Toma <A&B>" && x.note == "Muestra 3";
    CHECK(found);
}

APP_TEST(mensajes_pestanas_y_respuestas)
{
    app->open_chat("geo2");
    app->show(ScreenId::Messages);
    th::tick(30);
    lv_group_t *g = app->messages_screen.group;
    // tocar una respuesta rápida la envía al chat abierto
    lv_obj_t *chip = find_text(g, "OK, recibido");
    CHECK(chip != nullptr);
    if (!chip)
        return;
    size_t n = app->messages("geo2").size();
    lv_obj_send_event(chip, LV_EVENT_CLICKED, nullptr);
    th::tick(30);
    CHECK_EQ(app->messages("geo2").size(), n + 1);
    CHECK_EQ(app->messages("geo2").back().text, std::string("OK, recibido"));
    // mantenerla abre el editor en esa respuesta, y el clic al soltar no envía
    lv_obj_send_event(chip, LV_EVENT_LONG_PRESSED, nullptr);
    lv_obj_send_event(chip, LV_EVENT_CLICKED, nullptr);
    th::tick(30);
    CHECK(app->current() == ScreenId::QuickReplies);
    CHECK_EQ(app->messages("geo2").size(), n + 1);
    lv_obj_t *ta = th::focused();
    CHECK(ta && lv_obj_check_type(ta, &lv_textarea_class) &&
          std::string(lv_textarea_get_text(ta)) == "OK, recibido");
    th::key(LV_KEY_ESC);
    CHECK(app->current() == ScreenId::Messages);
    // el lápiz abre el editor
    lv_obj_t *pencil = nullptr;
    for (uint32_t i = 0; i < lv_group_get_obj_count(g); i++) {
        lv_obj_t *o = lv_group_get_obj_by_index(g, i);
        if (o == chip && i > 0)
            pencil = lv_group_get_obj_by_index(g, i - 1); // va justo antes de las respuestas
    }
    CHECK(pencil != nullptr);
    if (pencil)
        lv_obj_send_event(pencil, LV_EVENT_CLICKED, nullptr);
    th::tick(30);
    CHECK(app->current() == ScreenId::QuickReplies);
    th::key(LV_KEY_ESC);
    // ← → sobre las pestañas cambian de chat
    lv_obj_t *tab = find_text(g, "GEO2");
    CHECK(tab != nullptr);
    if (!tab)
        return;
    lv_group_focus_obj(tab);
    std::string first = app->messages_screen.current;
    th::key(LV_KEY_RIGHT);
    CHECK(app->messages_screen.current != first);
    th::key(LV_KEY_LEFT);
    CHECK_EQ(app->messages_screen.current, first);
}
