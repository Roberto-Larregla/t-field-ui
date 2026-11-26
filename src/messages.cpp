// Pantalla de mensajes: lista de chats, conversación y redacción.
#include "tfield/app.h"
#include "tfield/i18n.h"
#include "tfield/format.h"
#include "tfield/ui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

// pictogramas (fonts/tfield_pictos.c, C): fuera del namespace
LV_IMAGE_DECLARE(tfield_picto_mail);
LV_IMAGE_DECLARE(tfield_picto_pencil);

namespace tfield {

using namespace ui;

// Mensajes "con pestañas" (como el inicio Mosaico):
//
//   0 ┌ Esc 1 ✉ Mensajes                82% · 14:32 ┐  franja azul
//  22 ├ (GEO2) (Terreno 1) (BASE) (LongFast) …     ┤  un chat por pestaña (se desliza)
//  48 ├ directo · a 640 m NE · ahora      [Borrar] ┤
//  66 │ conversación a todo el ancho               │
// 185 ├ [✎] [OK, recibido] [Voy en camino] [En…  ┤  respuestas rápidas (se deslizan)
// 211 ├ [Escribe…                ] 0/200 [Enviar] ┤
// 240 └────────────────────────────────────────────┘
static constexpr int32_t HEAD_H = 28; // cabecera blanca de las otras pantallas (header_bar)
static constexpr int32_t BAND_H = 22;
static constexpr int32_t TABS_Y = BAND_H, TABS_H = 26;
static constexpr int32_t META_Y = TABS_Y + TABS_H, META_H = 18;
static constexpr int32_t CONV_Y = META_Y + META_H;
static constexpr int32_t QUICK_Y = 185, QUICK_H = 26;
static constexpr int32_t COMPOSE_Y = QUICK_Y + QUICK_H;
static constexpr int32_t CONV_W = SCREEN_W;
static constexpr int32_t BUBBLE_MAX_W = 262; // texto; la burbuja suma 12 de margen

static lv_obj_t *picto(lv_obj_t *parent, const lv_image_dsc_t *src, uint32_t color)
{
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, src);
    lv_obj_set_style_image_recolor(img, c(color), 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
}

lv_obj_t *header_bar(lv_obj_t *scr, App *app, lv_group_t *g, const char *title)
{
    lv_obj_t *h = box(scr, 0, 0, SCREEN_W, HEAD_H, col::SURF);
    border(h, col::INK, 1, LV_BORDER_SIDE_BOTTOM);
    lv_obj_t *esc = button(h, 6, 5, 28, 18, col::SURF, col::INK, g);
    lv_obj_set_style_radius(esc, 2, 0);
    lv_obj_set_style_bg_color(esc, c(col::BG), LV_STATE_PRESSED);
    lv_obj_t *el = label(esc, &tfield_font_12b, col::INK, "Esc");
    lv_obj_center(el);
    lv_obj_add_event_cb(
        esc, [](lv_event_t *e) { static_cast<App *>(lv_event_get_user_data(e))->back(); }, LV_EVENT_CLICKED, app);
    lv_obj_t *t = label(h, &tfield_font_14b, col::INK, title);
    lv_obj_set_pos(t, 42, 5);
    return h;
}

void MessagesScreen::create(App *app)
{
    app_ = app;
    screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen);
    set_bg(screen, col::SURF);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    group = lv_group_create();

    // --- Franja azul: Esc, 1 Mensajes, batería y hora
    lv_obj_t *h = box(screen, 0, 0, SCREEN_W, BAND_H, col::BLUE);
    esc_ = button(h, 4, 3, 28, 16, col::BLUE, col::SURF, nullptr);
    lv_obj_set_style_radius(esc_, 2, 0);
    lv_obj_set_style_bg_color(esc_, c(col::BLUE_DK), LV_STATE_PRESSED);
    lv_obj_set_style_outline_color(esc_, c(col::SURF), LV_STATE_FOCUS_KEY);
    lv_obj_center(label(esc_, &tfield_font_12b, col::SURF, "Esc"));
    lv_obj_add_event_cb(
        esc_, [](lv_event_t *e) { static_cast<App *>(lv_event_get_user_data(e))->back(); }, LV_EVENT_CLICKED, app);
    lv_obj_t *sq = box(h, 37, 4, 14, 14, col::SURF);
    lv_obj_set_style_radius(sq, 2, 0);
    lv_obj_center(label(sq, &tfield_font_12b, col::BLUE, "1"));
    lv_obj_set_pos(picto(h, &tfield_picto_mail, col::SURF), 55, 4);
    lv_obj_t *t = label(h, &tfield_font_14b, col::SURF, tr("Mensajes"));
    lv_obj_set_pos(t, 73, 3);
    time_ = label(h, &tfield_font_12b, col::SURF, "");
    ui::align(time_, LV_ALIGN_RIGHT_MID, -6, 0);
    pct_ = label(h, &tfield_font_12, col::SURF, "");

    // --- Pestañas: un chat cada una, se deslizan a lo ancho
    lv_obj_t *tabs = box(screen, 0, TABS_Y, SCREEN_W, TABS_H, col::BG);
    border(tabs, col::LINE, 1, LV_BORDER_SIDE_BOTTOM);
    list_ = box(tabs, 0, 0, SCREEN_W, TABS_H - 1, col::BG);
    lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list_, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(list_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(list_, 4, 0);
    lv_obj_set_style_pad_column(list_, 4, 0);

    // --- Datos del chat abierto y Borrar
    lv_obj_t *ch = box(screen, 0, META_Y, CONV_W, META_H, col::SURF);
    border(ch, col::LINE2, 1, LV_BORDER_SIDE_BOTTOM);
    conv_meta_ = label(ch, &tfield_font_12, col::INK2, "");
    lv_obj_set_pos(conv_meta_, 6, 2);
    lv_obj_set_size(conv_meta_, CONV_W - 72, 14);
    lv_label_set_long_mode(conv_meta_, LV_LABEL_LONG_MODE_DOTS);
    // Borrar el chat: la primera pulsación pregunta, la segunda borra
    del_btn_ = button(ch, CONV_W - 60, 1, 56, 15, col::BG, col::LINE, nullptr);
    lv_obj_set_style_radius(del_btn_, 3, 0);
    del_lbl_ = label(del_btn_, &tfield_font_12b, col::INK2, tr("Borrar"));
    lv_obj_center(del_lbl_);
    lv_obj_add_event_cb(
        del_btn_, [](lv_event_t *e) {
            auto *s = static_cast<MessagesScreen *>(lv_event_get_user_data(e));
            if (s->current.empty())
                return;
            if (!s->del_armed_ || lv_tick_elaps(s->del_armed_at_) > 5000) {
                s->del_armed_ = true;
                s->del_armed_at_ = lv_tick_get();
                set_bg(s->del_btn_, col::RED);
                set_border_color(s->del_btn_, col::RED);
                set_color(s->del_lbl_, col::SURF);
                set_text(s->del_lbl_, tr("\xC2\xBF" "Borrar?"));
                lv_obj_center(s->del_lbl_);
                return;
            }
            s->disarm_delete();
            lv_async_call([](void *p) { // la lista se rehace: fuera del evento del botón
                auto *s = static_cast<MessagesScreen *>(p);
                s->app_->delete_chat(s->current);
                s->rebuild_focus();
                s->focus_input();
            }, s);
        },
        LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(
        del_btn_, [](lv_event_t *e) { static_cast<MessagesScreen *>(lv_event_get_user_data(e))->disarm_delete(); },
        LV_EVENT_DEFOCUSED, this);

    conv_ = box(screen, 0, CONV_Y, CONV_W, QUICK_Y - CONV_Y, col::SURF);
    lv_obj_add_flag(conv_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(conv_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_flex_flow(conv_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(conv_, 5, 0);
    lv_obj_set_style_pad_row(conv_, 5, 0);

    // --- Respuestas rápidas: el lápiz (fijo) las edita; tocar una la envía al chat abierto,
    // mantenerla abre el editor en esa
    lv_obj_t *qs = box(screen, 0, QUICK_Y, SCREEN_W, QUICK_H, col::BG);
    border(qs, col::LINE, 1, LV_BORDER_SIDE_TOP);
    qr_ = button(qs, 4, 3, 25, 20, col::SLATE, 0, nullptr);
    lv_obj_set_style_bg_color(qr_, c(0x2A353D), LV_STATE_PRESSED);
    lv_obj_center(picto(qr_, &tfield_picto_pencil, col::SURF));
    lv_obj_add_event_cb(
        qr_, [](lv_event_t *e) { static_cast<App *>(lv_event_get_user_data(e))->edit_quick(-1); }, LV_EVENT_CLICKED,
        app);
    box(qs, 32, 4, 1, 18, col::LINE);
    lv_obj_t *chips = box(qs, 35, 0, SCREEN_W - 35, QUICK_H - 1, col::BG);
    lv_obj_add_flag(chips, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(chips, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(chips, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chips, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_right(chips, 4, 0);
    lv_obj_set_style_pad_column(chips, 3, 0);
    for (int i = 0; i < 4; i++) {
        chips_[i] = button(chips, 0, 0, 94, 20, col::SURF, col::BLUE, nullptr);
        lv_obj_set_style_bg_color(chips_[i], c(col::BLUE), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(chips_[i], c(col::LINE), LV_STATE_DISABLED);
        chip_labels_[i] = label(chips_[i], &tfield_font_12b, col::BLUE, "");
        lv_obj_set_size(chip_labels_[i], 90, 14);
        lv_obj_set_style_text_align(chip_labels_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(chip_labels_[i], LV_LABEL_LONG_MODE_DOTS);
        lv_obj_center(chip_labels_[i]);
        lv_obj_set_style_text_color(chip_labels_[i], c(col::SURF), LV_STATE_PRESSED);
        lv_obj_set_user_data(chips_[i], (void *)(intptr_t)i);
        lv_obj_add_event_cb(chips_[i], on_chip, LV_EVENT_ALL, this);
    }

    // --- Redacción
    lv_obj_t *comp = box(screen, 0, COMPOSE_Y, CONV_W, SCREEN_H - COMPOSE_Y, col::SURF);
    border(comp, col::INK, 1, LV_BORDER_SIDE_TOP);
    input_ = lv_textarea_create(comp);
    lv_obj_set_pos(input_, 4, 3);
    lv_obj_set_size(input_, 208, 23);
    lv_textarea_set_one_line(input_, true);
    limit_bytes(input_, MSG_MAX_BYTES); // Meshtastic cuenta bytes: "ñ" usa 2
    lv_textarea_set_placeholder_text(input_, tr("Escribe…"));
    set_font(input_, &tfield_font_14);
    set_color(input_, col::INK);
    lv_obj_set_style_text_color(input_, c(col::MUTED), LV_PART_TEXTAREA_PLACEHOLDER);
    set_bg(input_, col::SURF);
    set_border_color(input_, col::LINE);
    lv_obj_set_style_border_color(input_, c(col::BLUE), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(input_, 1, 0);
    lv_obj_set_style_radius(input_, 3, 0);
    lv_obj_set_style_pad_hor(input_, 4, 0);
    lv_obj_set_style_pad_ver(input_, 2, 0);
    lv_obj_set_style_shadow_width(input_, 0, 0);
    lv_obj_set_style_outline_width(input_, 0, LV_STATE_FOCUS_KEY);
    lv_obj_add_event_cb(input_, on_text, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(input_, on_send, LV_EVENT_READY, this);
    counter_ = label(comp, &tfield_font_12, col::INK2, "0/200");
    lv_obj_set_width(counter_, 40);
    lv_obj_set_style_text_align(counter_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(counter_, 214, 8);
    lv_obj_t *send = button(comp, 260, 3, 56, 23, col::BLUE, 0, nullptr);
    lv_obj_set_style_bg_color(send, c(col::BLUE_DK), LV_STATE_PRESSED);
    lv_obj_t *sl = label(send, &tfield_font_12b, col::SURF, tr("Enviar"));
    lv_obj_center(sl);
    lv_obj_add_event_cb(send, on_send, LV_EVENT_CLICKED, this);

    send_ = send;
    rebuild_focus();
    update_counter();
}

// Orden de foco: Esc → pestañas → Borrar → conversación → texto → Enviar → lápiz → respuestas
void MessagesScreen::disarm_delete()
{
    if (!del_armed_)
        return;
    del_armed_ = false;
    set_bg(del_btn_, col::BG);
    set_border_color(del_btn_, col::LINE);
    set_color(del_lbl_, col::INK2);
    set_text(del_lbl_, tr("Borrar"));
    lv_obj_center(del_lbl_);
}

void MessagesScreen::rebuild_focus()
{
    std::vector<lv_obj_t *> objs{esc_};
    for (auto &r : rows_)
        objs.push_back(r.row);
    if (!current.empty())
        objs.push_back(del_btn_);
    if (more_btn_)
        objs.push_back(more_btn_);
    for (auto &b : bubble_btns_)
        objs.push_back(b.second);
    objs.push_back(input_);
    objs.push_back(send_);
    objs.push_back(qr_);
    for (auto *chip : chips_)
        objs.push_back(chip);
    regroup(group, objs);
}

std::string MessagesScreen::draft() const { return input_ ? lv_textarea_get_text(input_) : ""; }

void MessagesScreen::set_draft(const std::string &text)
{
    if (input_) {
        lv_textarea_set_text(input_, text.c_str());
        update_counter();
    }
}

void MessagesScreen::focus_input()
{
    disarm_delete(); // volver a la pantalla no deja el "¿Borrar?" armado
    lv_group_focus_obj(input_);
}

void MessagesScreen::set_status(const Status &s)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:%02u", s.hour, s.minute);
    set_text(time_, buf);
    if (s.battery > 100)
        set_text(pct_, "USB");
    else if (s.battery >= 0) {
        snprintf(buf, sizeof(buf), "%d%%", s.battery);
        set_text(pct_, buf);
    }
    ui::align_to(pct_, time_, LV_ALIGN_OUT_LEFT_MID, -6, 0);
}

void MessagesScreen::set_chats(const std::vector<Chat> &chats)
{
    chats_ = chats;
    while (rows_.size() < chats_.size()) {
        Row r{};
        r.row = button(list_, 0, 0, 10, 19, col::SURF, col::LINE, nullptr);
        lv_obj_set_size(r.row, LV_SIZE_CONTENT, 19);
        lv_obj_set_style_radius(r.row, 10, 0);
        lv_obj_set_style_pad_hor(r.row, 7, 0);
        lv_obj_set_style_pad_column(r.row, 4, 0);
        lv_obj_set_flex_flow(r.row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r.row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        r.meta = box(r.row, 0, 0, 6, 6, col::BLUE); // punto de color del chat
        lv_obj_set_style_radius(r.meta, LV_RADIUS_CIRCLE, 0);
        r.name = label(r.row, &tfield_font_12b, col::INK, "");
        lv_obj_set_style_max_width(r.name, 120, 0); // un nombre largo no se come la fila
        lv_label_set_long_mode(r.name, LV_LABEL_LONG_MODE_DOTS);
        r.badge = box(r.row, 0, 0, 14, 14, col::BLUE);
        lv_obj_set_size(r.badge, LV_SIZE_CONTENT, 14);
        lv_obj_set_style_min_width(r.badge, 14, 0);
        lv_obj_set_style_pad_hor(r.badge, 3, 0);
        lv_obj_set_style_radius(r.badge, 7, 0);
        r.badge_lbl = label(r.badge, &tfield_font_12b, col::SURF, "");
        lv_obj_center(r.badge_lbl);
        lv_obj_set_user_data(r.row, (void *)(intptr_t)rows_.size());
        lv_obj_add_event_cb(r.row, on_row, LV_EVENT_CLICKED, this);
        rows_.push_back(r);
    }
    while (rows_.size() > chats_.size()) {
        lv_obj_delete(rows_.back().row);
        rows_.pop_back();
    }
    rebuild_focus();
    if (current.empty() && !chats_.empty())
        current = chats_[0].id;
    restyle_rows();
    refresh_conversation(false); // solo si llegó algo a este chat
}

void MessagesScreen::restyle_rows()
{
    char buf[8];
    for (size_t i = 0; i < rows_.size(); i++) {
        Row &r = rows_[i];
        const Chat &ch = chats_[i];
        bool on = ch.id == current;
        set_bg(r.row, on ? col::BLUE : col::SURF);
        set_border_color(r.row, on ? col::BLUE : col::LINE);
        set_text(r.name, ch.name);
        set_color(r.name, on ? col::SURF : col::INK);
        // color del chat: el del compañero; canales azules; fuera del equipo, gris
        uint32_t dc = ch.color ? ch.color : (app_->chat_outside_team(ch) ? col::MUTED : col::BLUE);
        set_bg(r.meta, on ? col::SURF : dc);
        if (ch.unread) {
            snprintf(buf, sizeof(buf), "%u", ch.unread);
            set_text(r.badge_lbl, buf);
            // fuera del equipo: insignia gris
            uint32_t bc = app_->chat_outside_team(ch) ? col::MUTED : col::BLUE;
            set_bg(r.badge, on ? col::SURF : bc);
            set_color(r.badge_lbl, on ? bc : col::SURF);
            show(r.badge, true);
        } else
            show(r.badge, false);
    }
}

void MessagesScreen::show_chat(const std::string &id)
{
    current = id;
    restyle_rows();
    refresh_conversation();
    for (size_t i = 0; i < rows_.size() && i < chats_.size(); i++)
        if (chats_[i].id == id) // la pestaña del chat abierto, a la vista
            lv_obj_scroll_to_view(rows_[i].row, LV_ANIM_OFF);
}

bool MessagesScreen::on_tab() const
{
    lv_obj_t *f = lv_group_get_focused(group);
    for (auto &r : rows_)
        if (r.row == f)
            return true;
    return false;
}

// Chat anterior o siguiente (← → sobre las pestañas)
void MessagesScreen::step_chat(int dir)
{
    if (chats_.empty())
        return;
    int i = 0;
    for (size_t k = 0; k < chats_.size(); k++)
        if (chats_[k].id == current)
            i = (int)k;
    i = (i + dir + (int)chats_.size()) % (int)chats_.size();
    app_->open_chat(chats_[i].id);
    lv_group_focus_obj(rows_[i].row);
}

// "directo · a 640 m NE · ahora" / "canal cifrado"
void MessagesScreen::refresh_meta()
{
    const Chat *chat = nullptr;
    for (auto &ch : chats_)
        if (ch.id == current)
            chat = &ch;
    std::string meta;
    if (chat && chat->is_channel)
        meta = tr("canal cifrado");
    else if (chat) {
        meta = tr("mensaje directo");
        if (const TeamNode *n = app_->chat_node(*chat)) {
            if (n->dist_m >= 0)
                meta = std::string(tr("directo")) + " \xC2\xB7 " + tr("a ") + format_distance(n->dist_m) + " " +
                       cardinal(n->bearing_deg);
            meta += " \xC2\xB7 " + format_ago(n->heard_ago_s);
        }
    }
    set_text(conv_meta_, meta);
}

void MessagesScreen::refresh_chips()
{
    const QuickReplies &q = app_->quick();
    for (int i = 0; i < 4; i++) {
        bool on = !current.empty() && !q.text[i].empty();
        set_text(chip_labels_[i], q.text[i].empty() ? tr("(vac\xC3\xAD" "a)") : q.text[i]);
        if (on)
            lv_obj_remove_state(chips_[i], LV_STATE_DISABLED);
        else
            lv_obj_add_state(chips_[i], LV_STATE_DISABLED);
        set_color(chip_labels_[i], on ? col::BLUE : col::MUTED);
    }
}

void MessagesScreen::on_chip(lv_event_t *e)
{
    auto *self = static_cast<MessagesScreen *>(lv_event_get_user_data(e));
    lv_obj_t *chip = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
    int i = (int)(intptr_t)lv_obj_get_user_data(chip);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_LONG_PRESSED) { // mantener: editar esta respuesta
        self->long_pressed_ = true;
        self->app_->edit_quick(i);
        return;
    }
    if (code != LV_EVENT_CLICKED)
        return;
    if (self->long_pressed_) { // el clic que sigue a mantener no envía
        self->long_pressed_ = false;
        return;
    }
    const std::string &text = self->app_->quick().text[i];
    if (self->current.empty() || text.empty() || lv_obj_has_state(chip, LV_STATE_DISABLED))
        return;
    self->app_->send(self->current, text);
}

size_t MessagesScreen::bubble_count() const
{
    uint32_t n = lv_obj_get_child_count(conv_);
    return more_btn_ ? n - 1 : n;
}

// Quita las burbujas más antiguas hasta dejar "keep" (la de arriba es la más vieja)
void MessagesScreen::trim_bubbles(size_t keep)
{
    int32_t removed_h = 0;
    while (bubble_count() > keep) {
        lv_obj_t *old = lv_obj_get_child(conv_, more_btn_ ? 1 : 0);
        removed_h += lv_obj_get_height(old) + lv_obj_get_style_pad_row(conv_, LV_PART_MAIN);
        bubble_btns_.erase(std::remove_if(bubble_btns_.begin(), bubble_btns_.end(),
                                          [old](const std::pair<lv_obj_t *, lv_obj_t *> &p) { return p.first == old; }),
                           bubble_btns_.end());
        lv_obj_delete(old);
    }
    if (removed_h) { // lo que se estaba leyendo no salta hacia arriba
        lv_obj_update_layout(conv_);
        lv_obj_scroll_by_bounded(conv_, 0, removed_h, LV_ANIM_OFF);
    }
}

void MessagesScreen::refresh_conversation(bool force)
{
    const Chat *chat = nullptr;
    for (auto &ch : chats_)
        if (ch.id == current)
            chat = &ch;
    refresh_meta();
    show(del_btn_, chat != nullptr);
    refresh_chips();

    // rehacer las burbujas cuesta (en la T-Deck, ~10 ms cada una): solo si cambió el chat o
    // llegó/cambió un mensaje, y si solo llegaron mensajes nuevos, se agregan al final
    uint32_t rev = chat ? app_->messages_rev(current) : 0, added = chat ? app_->messages_added(current) : 0;
    bool same = shown_chat_ == (chat ? current : std::string());
    if (!force && same && shown_rev_ == rev)
        return;
    const std::vector<Message> *all = chat ? &app_->messages(current) : nullptr;
    uint32_t fresh = added - shown_added_;
    if (!force && same && chat && fresh > 0 && rev - shown_rev_ == fresh && fresh <= CHAT_SHOW_MESSAGES &&
        fresh <= all->size()) {
        bool at_bottom = lv_obj_get_scroll_bottom(conv_) <= 8;
        shown_rev_ = rev;
        shown_added_ = added;
        for (size_t i = all->size() - fresh; i < all->size(); i++)
            add_bubble((*all)[i]);
        // las de arriba que se descartaron de la memoria, o que pasan del máximo a la vista
        trim_bubbles(std::min(show_n_, all->size()));
        if (more_btn_ && bubble_count() >= all->size()) {
            lv_obj_delete(more_btn_);
            more_btn_ = nullptr;
        }
        rebuild_focus();
        lv_obj_update_layout(conv_);
        if (at_bottom)
            lv_obj_scroll_to_y(conv_, LV_COORD_MAX, LV_ANIM_OFF);
        return;
    }
    if (!same) {
        show_n_ = CHAT_SHOW_MESSAGES;
        disarm_delete();
    }
    shown_chat_ = chat ? current : std::string();
    shown_rev_ = rev;
    shown_added_ = added;
    lv_obj_clean(conv_);
    more_btn_ = nullptr;
    bubble_btns_.clear();
    if (!chat) {
        rebuild_focus();
        return;
    }
    size_t from = all->size() > show_n_ ? all->size() - show_n_ : 0; // los últimos
    if (from > 0) {
        more_btn_ = button(conv_, 0, 0, CONV_W - 10, 22, col::SURF, col::BLUE, nullptr);
        lv_obj_set_style_radius(more_btn_, 3, 0);
        char mb[48];
        snprintf(mb, sizeof(mb), tr("Mensajes anteriores (%u)"), (unsigned)from);
        lv_obj_center(label(more_btn_, &tfield_font_12b, col::BLUE, mb));
        lv_obj_add_event_cb(
            more_btn_, [](lv_event_t *e) {
                auto *self = static_cast<MessagesScreen *>(lv_event_get_user_data(e));
                lv_async_call([](void *p) { // se rehace la lista: fuera del evento del botón
                    auto *self = static_cast<MessagesScreen *>(p);
                    self->show_n_ += CHAT_SHOW_MESSAGES;
                    self->refresh_conversation(true);
                    lv_obj_scroll_to_y(self->conv_, 0, LV_ANIM_OFF);
                    if (self->more_btn_)
                        lv_group_focus_obj(self->more_btn_);
                }, self);
            },
            LV_EVENT_CLICKED, this);
    }
    for (size_t mi = from; mi < all->size(); mi++)
        add_bubble((*all)[mi]);
    rebuild_focus();
    lv_obj_update_layout(conv_);
    lv_obj_scroll_to_y(conv_, LV_COORD_MAX, LV_ANIM_OFF);
}

void MessagesScreen::add_bubble(const Message &m)
{
    lv_obj_t *line = box(conv_, 0, 0, CONV_W - 10, 10, col::SURF);
    lv_obj_set_height(line, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(line, LV_OPA_TRANSP, 0);

    lv_obj_t *b = box(line, 0, 0, 10, 10, m.mine ? col::BLUE : col::SURF);
    lv_obj_set_size(b, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_pad_hor(b, 6, 0);
    lv_obj_set_style_pad_ver(b, 4, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(b, 2, 0);
    if (!m.mine)
        border(b, col::LINE, 1);

    if (!m.mine) {
        lv_obj_t *meta = box(b, 0, 0, 10, 10, col::SURF);
        lv_obj_set_size(meta, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(meta, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(meta, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(meta, 5, 0);
        lv_obj_t *tag = box(meta, 0, 0, 10, 10, m.from_color ? m.from_color : col::INK);
        lv_obj_set_size(tag, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(tag, 2, 0);
        lv_obj_set_style_pad_hor(tag, 3, 0);
        label(tag, &tfield_font_12b, col::SURF, m.from.c_str());
        label(meta, &tfield_font_12, col::INK2, m.time.c_str());
    }

    lv_obj_t *txt = label(b, &tfield_font_14, m.mine ? col::SURF : col::INK, m.text.c_str());
    lv_point_t sz;
    lv_text_get_size(&sz, m.text.c_str(), &tfield_font_14, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (sz.x > BUBBLE_MAX_W) {
        lv_obj_set_width(txt, BUBBLE_MAX_W);
        lv_label_set_long_mode(txt, LV_LABEL_LONG_MODE_WRAP);
    }

    // acciones dentro de la burbuja: abrir un punto, guardarlo o unirse a un canal
    const char *act = m.waypoint_id ? tr("Ver en el mapa") : m.pending_wp ? tr("Guardar punto") : m.invite_id ? tr("Unirse al canal") : nullptr;
    if (act) {
        lv_obj_t *btn = button(b, 0, 0, 10, 20, m.mine ? col::BLUE : col::SURF, m.mine ? col::SURF : col::BLUE, nullptr);
        bubble_btns_.push_back({line, btn});
        lv_obj_set_size(btn, LV_SIZE_CONTENT, 20);
        lv_obj_set_style_pad_hor(btn, 8, 0);
        lv_obj_set_style_radius(btn, 3, 0);
        lv_obj_set_style_bg_color(btn, c(m.mine ? col::BLUE_DK : col::BLUE_BG), LV_STATE_PRESSED);
        bool strong = !m.waypoint_id; // decisiones pendientes: botón lleno
        if (strong && !m.mine)
            set_bg(btn, col::BLUE);
        lv_obj_t *bl = label(btn, &tfield_font_12b, (m.mine || strong) ? col::SURF : col::BLUE, act);
        lv_obj_center(bl);
        // tipo en los 2 bits altos: 0 punto, 1 punto pendiente, 2 invitación
        uint32_t kind = m.waypoint_id ? 0u : m.pending_wp ? 1u : 2u;
        uint32_t id = m.waypoint_id ? m.waypoint_id : m.pending_wp ? m.pending_wp : m.invite_id;
        lv_obj_set_user_data(btn, (void *)(uintptr_t)((kind << 30) | (id & 0x3FFFFFFF)));
        lv_obj_add_event_cb(
            btn, [](lv_event_t *e) {
                auto *self = static_cast<MessagesScreen *>(lv_event_get_user_data(e));
                uint32_t v = (uint32_t)(uintptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e));
                uint32_t kind = v >> 30, id = v & 0x3FFFFFFF;
                if (kind == 0)
                    self->app_->show_waypoint(id);
                else if (kind == 1)
                    lv_async_call([](void *p) { // la conversación se rehace: no dentro del evento
                        auto *pr = static_cast<std::pair<MessagesScreen *, uint32_t> *>(p);
                        pr->first->app_->accept_waypoint(pr->second);
                        delete pr;
                    }, new std::pair<MessagesScreen *, uint32_t>(self, id));
                else
                    lv_async_call([](void *p) {
                        auto *pr = static_cast<std::pair<MessagesScreen *, uint32_t> *>(p);
                        pr->first->app_->accept_invite(pr->second);
                        delete pr;
                    }, new std::pair<MessagesScreen *, uint32_t>(self, id));
            },
            LV_EVENT_CLICKED, this);
    }

    if (m.mine) {
        // estado: vocabulario cerrado (Enviado, Entregado…), se traduce al dibujar
        std::string st = m.status.empty() ? m.time : std::string(tr(m.status.c_str())) + " " + m.time;
        label(b, &tfield_font_12, col::SURF, st.c_str());
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        ui::align(b, LV_ALIGN_TOP_RIGHT, 0, 0);
    }
}

void MessagesScreen::update_counter()
{
    char buf[16];
    size_t n = std::strlen(lv_textarea_get_text(input_)); // bytes: lo que cuenta Meshtastic
    snprintf(buf, sizeof(buf), "%u/%u", (unsigned)n, (unsigned)MSG_MAX_BYTES);
    set_text(counter_, buf);
    set_color(counter_, n >= MSG_MAX_BYTES ? col::AMBER : col::INK2);
}

void MessagesScreen::on_row(lv_event_t *e)
{
    auto *self = static_cast<MessagesScreen *>(lv_event_get_user_data(e));
    int i = (int)(intptr_t)lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e)));
    if (i >= 0 && i < (int)self->chats_.size())
        self->app_->open_chat(self->chats_[i].id);
}

void MessagesScreen::on_text(lv_event_t *e)
{
    static_cast<MessagesScreen *>(lv_event_get_user_data(e))->update_counter();
}

void MessagesScreen::on_send(lv_event_t *e)
{
    auto *self = static_cast<MessagesScreen *>(lv_event_get_user_data(e));
    std::string text = lv_textarea_get_text(self->input_);
    while (!text.empty() && text.back() == ' ')
        text.pop_back();
    if (text.empty() || self->current.empty())
        return;
    lv_textarea_set_text(self->input_, "");
    self->app_->send(self->current, text);
}

} // namespace tfield
