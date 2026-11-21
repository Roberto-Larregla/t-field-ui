// Pantalla de mensajes: lista de chats, conversación y redacción.
#include "tfield/app.h"
#include "tfield/i18n.h"
#include "tfield/format.h"
#include "tfield/ui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

namespace tfield {

using namespace ui;

static constexpr int32_t HEAD_H = 28;
static constexpr int32_t LIST_W = 104;
static constexpr int32_t LIST_ROW_H = 34;
static constexpr int32_t CONV_X = LIST_W + 1;
static constexpr int32_t CONV_W = SCREEN_W - CONV_X;
static constexpr int32_t CONV_HDR_H = 20;
static constexpr int32_t COMPOSE_H = 30;
static constexpr int32_t BUBBLE_MAX_W = 168;

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

    lv_obj_t *h = header_bar(screen, app, group, tr("Mensajes"));
    esc_ = lv_obj_get_child(h, 0);
    time_ = label(h, &tfield_font_12b, col::INK, "");
    ui::align(time_, LV_ALIGN_RIGHT_MID, -6, 0);
    pct_ = label(h, &tfield_font_12, col::INK2, "");

    // Lista de chats
    lv_obj_t *left = box(screen, 0, HEAD_H, LIST_W, SCREEN_H - HEAD_H, col::SURF);
    border(left, col::LINE, 1, LV_BORDER_SIDE_RIGHT);
    list_ = box(left, 0, 0, LIST_W - 1, SCREEN_H - HEAD_H - 30, col::SURF);
    lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *qr = button(left, 4, SCREEN_H - HEAD_H - 27, LIST_W - 9, 22, col::SURF, col::BLUE, nullptr);
    lv_obj_set_style_bg_color(qr, c(col::BLUE), LV_STATE_PRESSED);
    lv_obj_t *ql = label(qr, &tfield_font_12b, col::BLUE, tr("Resp. r\xC3\xA1pidas"));
    lv_obj_set_style_text_color(ql, c(col::SURF), LV_STATE_PRESSED);
    lv_obj_center(ql);
    lv_obj_add_event_cb(
        qr, [](lv_event_t *e) { static_cast<App *>(lv_event_get_user_data(e))->show(ScreenId::QuickReplies); },
        LV_EVENT_CLICKED, app);

    // Conversación
    lv_obj_t *ch = box(screen, CONV_X, HEAD_H, CONV_W, CONV_HDR_H, col::BG);
    border(ch, col::LINE, 1, LV_BORDER_SIDE_BOTTOM);
    conv_title_ = label(ch, &tfield_font_12b, col::INK, "");
    lv_obj_set_pos(conv_title_, 6, 3);
    lv_label_set_long_mode(conv_title_, LV_LABEL_LONG_MODE_DOTS);
    fitted_title_.clear();
    conv_meta_ = label(ch, &tfield_font_12, col::INK2, "");
    ui::align(conv_meta_, LV_ALIGN_TOP_RIGHT, -66, 3);
    // Borrar el chat: la primera pulsación pregunta, la segunda borra
    del_btn_ = button(ch, CONV_W - 60, 2, 56, 16, col::BG, col::LINE, nullptr);
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

    int32_t conv_y = HEAD_H + CONV_HDR_H, conv_h = SCREEN_H - conv_y - COMPOSE_H;
    conv_ = box(screen, CONV_X, conv_y, CONV_W, conv_h, col::SURF);
    lv_obj_add_flag(conv_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(conv_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_flex_flow(conv_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(conv_, 5, 0);
    lv_obj_set_style_pad_row(conv_, 5, 0);

    // Redacción
    lv_obj_t *comp = box(screen, CONV_X, SCREEN_H - COMPOSE_H, CONV_W, COMPOSE_H, col::SURF);
    border(comp, col::INK, 1, LV_BORDER_SIDE_TOP);
    input_ = lv_textarea_create(comp);
    lv_obj_set_pos(input_, 4, 4);
    lv_obj_set_size(input_, 122, 22);
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
    lv_obj_set_pos(counter_, 127, 8);
    lv_obj_t *send = button(comp, 171, 4, 40, 22, col::BLUE, 0, nullptr);
    lv_obj_set_style_bg_color(send, c(col::BLUE_DK), LV_STATE_PRESSED);
    lv_obj_t *sl = label(send, &tfield_font_12b, col::SURF, tr("Enviar"));
    lv_obj_center(sl);
    lv_obj_add_event_cb(send, on_send, LV_EVENT_CLICKED, this);

    send_ = send;
    qr_ = qr;
    rebuild_focus();
    update_counter();
}

// Orden de foco: Esc → chats → botones de la conversación → texto → Enviar → Respuestas rápidas
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
        r.row = button(list_, 0, 0, LIST_W - 1, LIST_ROW_H, col::SURF, 0, nullptr);
        lv_obj_set_style_radius(r.row, 0, 0);
        border(r.row, col::LINE2, 1, LV_BORDER_SIDE_BOTTOM);
        r.name = label(r.row, &tfield_font_14b, col::INK, "");
        lv_obj_set_pos(r.name, 5, 2);
        lv_obj_set_size(r.name, LIST_W - 32, 16);
        lv_label_set_long_mode(r.name, LV_LABEL_LONG_MODE_DOTS);
        r.badge = box(r.row, LIST_W - 1 - 4 - 18, 4, 18, 14, col::BLUE);
        lv_obj_set_style_radius(r.badge, 7, 0);
        r.badge_lbl = label(r.badge, &tfield_font_12b, col::SURF, "");
        lv_obj_center(r.badge_lbl);
        r.meta = label(r.row, &tfield_font_12, col::INK2, "");
        lv_obj_set_pos(r.meta, 5, 18);
        lv_obj_set_size(r.meta, LIST_W - 11, 14);
        lv_label_set_long_mode(r.meta, LV_LABEL_LONG_MODE_DOTS);
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
        set_border_color(r.row, on ? col::BLUE : col::LINE2);
        set_text(r.name, ch.name);
        set_color(r.name, on ? col::SURF : col::INK);
        std::string meta = ch.preview.empty() ? (ch.is_channel ? tr("Canal") : tr("Directo")) : ch.preview;
        set_text(r.meta, meta);
        set_color(r.meta, on ? col::SURF : col::INK2);
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
    set_text(conv_title_, chat ? chat->name : "");
    set_text(conv_meta_, chat ? (chat->is_channel ? tr("canal cifrado") : tr("mensaje directo")) : "");
    show(del_btn_, chat != nullptr);
    if (chat && chat->name != fitted_title_) { // un nombre largo no pisa "mensaje directo" ni "Borrar"
        fitted_title_ = chat->name;
        lv_obj_set_width(conv_title_, LV_SIZE_CONTENT);
        lv_obj_refr_size(conv_title_);
        lv_obj_refr_size(conv_meta_);
        int32_t avail = CONV_W - 6 - 66, tw = lv_obj_get_width(conv_title_), mw = lv_obj_get_width(conv_meta_);
        show(conv_meta_, tw + 8 + mw <= avail);
        if (tw > avail)
            lv_obj_set_width(conv_title_, avail);
    }

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
