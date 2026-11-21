// Pantalla de inicio: estado, equipo, chats, respuestas rápidas y pestañas.
#include "tfield/app.h"
#include "tfield/i18n.h"
#include "tfield/format.h"
#include "tfield/ui.h"

#include <cstdio>

namespace tfield {

using namespace ui;

static constexpr int32_t TOP_H = 33;   // franja de estado (incluye línea)
static constexpr int32_t MID_Y = 34;
static constexpr int32_t MID_H = 105;
static constexpr int32_t TEAM_W = 172;
static constexpr int32_t HDR_H = 16;
static constexpr int32_t ROW_H = 29;
static constexpr int32_t QUICK_Y = 139;
static constexpr int32_t QUICK_H = 74;
static constexpr int32_t NAV_Y = 213;

struct TabDef {
    const char *num;
    const char *label;
    uint32_t color;
    ScreenId target;
};
static const TabDef TABS[4] = {{"1", "Mensajes", col::BLUE, ScreenId::Messages},
                               {"2", "Mapa", col::GREEN, ScreenId::Map},
                               {"3", "Equipo", col::PURPLE, ScreenId::Team},
                               {"4", "Ajustes", col::SLATE, ScreenId::Settings}}; // se traducen al dibujar

void HomeScreen::create(App *app)
{
    app_ = app;
    screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen);
    set_bg(screen, col::BG);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    group = lv_group_create();

    // --- Franja de estado
    lv_obj_t *top = box(screen, 0, 0, SCREEN_W, TOP_H, col::SURF);
    border(top, col::INK, 1, LV_BORDER_SIDE_BOTTOM);
    clock_ = label(top, &tfield_font_24b, col::INK, "--:--");
    lv_obj_set_pos(clock_, 6, 2);
    day_ = label(top, &tfield_font_12, col::INK2, "");
    lv_obj_set_pos(day_, 70, 3);
    month_ = label(top, &tfield_font_12, col::INK2, "");
    lv_obj_set_pos(month_, 70, 16);
    box(top, 108, 6, 1, 20, col::LINE);
    sun_dot_ = box(top, 116, 6, 8, 8, col::AMBER);
    lv_obj_set_style_radius(sun_dot_, LV_RADIUS_CIRCLE, 0);
    daylight_ = label(top, &tfield_font_14b, col::INK, "");
    lv_obj_set_pos(daylight_, 128, 1);
    sunset_ = label(top, &tfield_font_12, col::INK2, "");
    lv_obj_set_pos(sunset_, 116, 17);
    name_ = label(top, &tfield_font_12b, col::INK, "");
    ui::align(name_, LV_ALIGN_TOP_RIGHT, -6, 3);
    gps_ = label(top, &tfield_font_12b, col::INK, "");
    ui::align(gps_, LV_ALIGN_TOP_RIGHT, -6, 16);
    bat_pct_ = label(top, &tfield_font_12b, col::INK, "");
    bat_ = battery(top, 0, 0);

    // --- Equipo
    lv_obj_t *team = box(screen, 0, MID_Y, TEAM_W, MID_H, col::SURF);
    border(team, col::INK, 1, LV_BORDER_SIDE_RIGHT);
    lv_obj_t *th = box(team, 0, 0, TEAM_W - 1, HDR_H, col::BG);
    team_title_ = label(th, &tfield_font_12b, col::INK, tr("Equipo"));
    lv_obj_set_pos(team_title_, 5, 1);
    lv_obj_t *sort = label(th, &tfield_font_12, col::INK2, tr("por distancia"));
    ui::align(sort, LV_ALIGN_TOP_RIGHT, -5, 1);
    team_list_ = box(team, 0, HDR_H, TEAM_W - 1, MID_H - HDR_H, col::SURF);
    lv_obj_add_flag(team_list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(team_list_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_flex_flow(team_list_, LV_FLEX_FLOW_COLUMN);
    // sin equipo todavía (primer encendido): cómo armarlo
    team_empty_ = label(team, &tfield_font_12, col::INK2, tr("Sin equipo todav\xC3\xAD" "a.\nM\xC3\xA1rcalo en Equipo \xE2\x80\xBA Toda la red \xE2\x80\xBA ficha del nodo."));
    lv_obj_set_pos(team_empty_, 6, HDR_H + 6);
    lv_obj_set_width(team_empty_, TEAM_W - 14);
    lv_label_set_long_mode(team_empty_, LV_LABEL_LONG_MODE_WRAP);
    show(team_empty_, false);

    // --- Mensajes
    int32_t cx = TEAM_W + 1, cw = SCREEN_W - cx;
    lv_obj_t *chats = box(screen, cx, MID_Y, cw, MID_H, col::SURF);
    lv_obj_t *ch = box(chats, 0, 0, cw, HDR_H, col::BG);
    lv_obj_t *ct = label(ch, &tfield_font_12b, col::INK, tr("Mensajes"));
    lv_obj_set_pos(ct, 5, 1);
    chat_badge_ = label(ch, &tfield_font_12b, col::BLUE, "");
    ui::align(chat_badge_, LV_ALIGN_TOP_RIGHT, -5, 1);
    chat_list_ = box(chats, 0, HDR_H, cw, MID_H - HDR_H, col::SURF);
    lv_obj_add_flag(chat_list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(chat_list_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_flex_flow(chat_list_, LV_FLEX_FLOW_COLUMN);

    // --- Respuestas rápidas
    lv_obj_t *q = box(screen, 0, QUICK_Y, SCREEN_W, QUICK_H, col::SURF);
    border(q, col::INK, 1, LV_BORDER_SIDE_TOP);
    quick_label_ = label(q, &tfield_font_12b, col::INK2, tr("Elige un chat para responder"));
    lv_obj_set_pos(quick_label_, 5, 3);
    lv_obj_set_size(quick_label_, SCREEN_W - 10, 14);
    lv_label_set_long_mode(quick_label_, LV_LABEL_LONG_MODE_DOTS);
    for (int i = 0; i < 4; i++) {
        int32_t x = (i % 2) ? 161 : 4, y = (i / 2) ? 46 : 19, w = (i % 2) ? 155 : 154;
        chips_[i] = button(q, x, y, w, 24, col::SURF, col::BLUE, nullptr);
        lv_obj_set_style_bg_color(chips_[i], c(col::BLUE), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(chips_[i], c(col::LINE), LV_STATE_DISABLED);
        chip_labels_[i] = label(chips_[i], &tfield_font_12b, col::BLUE, "");
        lv_obj_set_size(chip_labels_[i], w - 10, 14);
        lv_obj_set_style_text_align(chip_labels_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(chip_labels_[i], LV_LABEL_LONG_MODE_DOTS);
        lv_obj_center(chip_labels_[i]);
        lv_obj_set_style_text_color(chip_labels_[i], c(col::SURF), LV_STATE_PRESSED);
        lv_obj_add_event_cb(chips_[i], on_chip, LV_EVENT_CLICKED, this);
    }

    // --- Pestañas
    lv_obj_t *nav = box(screen, 0, NAV_Y, SCREEN_W, SCREEN_H - NAV_Y, col::SURF);
    border(nav, col::INK, 1, LV_BORDER_SIDE_TOP);
    // pestañas de ancho ajustado + SOS a la derecha
    static const int32_t TAB_X[4] = {0, 84, 144, 212}, TAB_W[4] = {84, 60, 68, 68};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *tab = button(nav, TAB_X[i], 1, TAB_W[i], SCREEN_H - NAV_Y - 1, col::SURF, 0, nullptr);
        lv_obj_set_style_radius(tab, 0, 0);
        lv_obj_set_style_bg_color(tab, c(col::BG), LV_STATE_PRESSED);
        {
            set_border_color(tab, col::LINE2);
            lv_obj_set_style_border_width(tab, 1, 0);
            lv_obj_set_style_border_side(tab, LV_BORDER_SIDE_RIGHT, 0);
        }
        lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(tab, 3, 0);
        lv_obj_t *sq = box(tab, 0, 0, 14, 14, TABS[i].color);
        lv_obj_set_style_radius(sq, 2, 0);
        lv_obj_t *n = label(sq, &tfield_font_12b, col::SURF, TABS[i].num);
        lv_obj_center(n);
        label(tab, &tfield_font_12b, col::INK, tr(TABS[i].label));
        lv_obj_set_user_data(tab, (void *)(intptr_t)i);
        lv_obj_add_event_cb(tab, on_tab, LV_EVENT_CLICKED, this);
        tabs_[i] = tab;
    }
    sos_ = hold_button(nav, 282, 3, 35, SCREEN_H - NAV_Y - 5, col::RED, 0x7A1712, col::SURF, "SOS", 2000,
                       [](void *u) { static_cast<App *>(u)->trigger_sos(); }, app, nullptr);
    lv_obj_set_style_radius(sos_, 3, 0);
    rebuild_focus();
    refresh_chips();
}

void HomeScreen::set_status(const Status &s)
{
    char buf[32];
    if (s.epoch) // sin hora (aún sin GPS ni red): no inventar "00:00"
        snprintf(buf, sizeof(buf), "%02u:%02u", s.hour, s.minute);
    else
        snprintf(buf, sizeof(buf), "--:--");
    set_text(clock_, buf);
    set_text(day_, s.day);
    set_text(month_, s.month);
    if (s.daylight_min > 0) {
        set_text(daylight_, format_daylight(s.daylight_min) + tr(" de luz"));
        set_text(sunset_, tr("Puesta ") + s.sunset);
        set_bg(sun_dot_, col::AMBER);
    } else if (s.daylight_min == 0) { // de noche: cuándo vuelve la luz
        set_text(daylight_, tr("Noche"));
        set_text(sunset_, s.sunrise.empty() ? std::string() : tr("Sale el sol ") + s.sunrise);
        set_bg(sun_dot_, col::INK2);
    } else {
        set_text(daylight_, tr("Sin posición"));
        set_text(sunset_, "");
    }
    set_text(name_, s.short_name);
    snprintf(buf, sizeof(buf), s.gps_fix ? "GPS %u" : "GPS --", s.sats);
    set_text(gps_, buf);
    if (s.battery > 100)
        set_text(bat_pct_, "USB");
    else if (s.battery >= 0) {
        snprintf(buf, sizeof(buf), "%d%%", s.battery);
        set_text(bat_pct_, buf);
    } else
        set_text(bat_pct_, "--");
    ui::align_to(bat_pct_, gps_, LV_ALIGN_OUT_LEFT_MID, -6, 0);
    ui::align_to(bat_, bat_pct_, LV_ALIGN_OUT_LEFT_MID, -3, 0);
    battery_set(bat_, s.battery, col::INK);
}

void HomeScreen::set_team(const std::vector<TeamNode> &team)
{
    char buf[32];
    snprintf(buf, sizeof(buf), tr("Equipo \xC2\xB7 %u"), (unsigned)team.size());
    set_text(team_title_, buf);

    // Crear o eliminar filas solo si cambia la cantidad.
    while (team_rows_.size() < team.size()) {
        TeamRow r{};
        r.row = button(team_list_, 0, 0, TEAM_W - 1, ROW_H, col::SURF, 0, nullptr);
        lv_obj_set_style_radius(r.row, 0, 0);
        border(r.row, col::LINE2, 1, LV_BORDER_SIDE_BOTTOM);
        lv_obj_set_style_bg_color(r.row, c(col::BG), LV_STATE_PRESSED);
        lv_obj_add_event_cb(
            r.row, [](lv_event_t *e) { // abre la ficha del nodo
                auto *self = static_cast<HomeScreen *>(lv_event_get_user_data(e));
                uint32_t num = (uint32_t)(uintptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e));
                self->app_->show_node(num);
            },
            LV_EVENT_CLICKED, this);
        r.dot = box(r.row, 5, 5, 8, 8, col::INK);
        lv_obj_set_style_radius(r.dot, LV_RADIUS_CIRCLE, 0);
        r.name = label(r.row, &tfield_font_14b, col::INK, "");
        lv_obj_set_pos(r.name, 17, 0);
        // línea 1: nombre · hace cuánto · señal
        r.bars = signal_bars(r.row, TEAM_W - 1 - 5 - 11, 3);
        r.heard = label(r.row, &tfield_font_12, col::INK2, "");
        ui::align(r.heard, LV_ALIGN_TOP_RIGHT, -21, 1);
        // línea 2: distancia y rumbo · altura · batería
        r.dist = label(r.row, &tfield_font_12, col::INK, "");
        lv_obj_set_pos(r.dist, 17, 14);
        r.alt = label(r.row, &tfield_font_12, col::INK, "");
        lv_obj_set_pos(r.alt, 79, 14);
        r.bat = battery(r.row, 122, 18);
        r.pct = label(r.row, &tfield_font_12, col::INK, "");
        lv_obj_set_pos(r.pct, 138, 14);
        team_rows_.push_back(r);
    }
    while (team_rows_.size() > team.size()) {
        lv_obj_delete(team_rows_.back().row);
        team_rows_.pop_back();
    }
    if (team.empty())
        show(team_empty_, true);
    else
        show(team_empty_, false);

    for (size_t i = 0; i < team.size(); i++) {
        const TeamNode &n = team[i];
        TeamRow &r = team_rows_[i];
        bool fresh = n.heard_ago_s < 60;
        lv_obj_set_user_data(r.row, (void *)(uintptr_t)n.num);
        set_bg(r.row, n.stale ? col::AMBER_BG : col::SURF);
        set_bg(r.dot, n.color);
        set_text(r.name, n.name);
        set_text(r.heard, format_ago(n.heard_ago_s));
        set_font(r.heard, (fresh || n.stale) ? &tfield_font_12b : &tfield_font_12);
        set_color(r.heard, n.stale ? col::AMBER : (fresh ? col::GREEN : col::INK2));
        if (n.dist_m >= 0)
            set_text(r.dist, format_distance(n.dist_m) + " " + cardinal(n.bearing_deg));
        else
            set_text(r.dist, tr("sin posición"));
        set_text(r.alt, n.has_alt && n.dist_m >= 0 ? format_alt_diff(n.alt_diff_m) : "");
        battery_set(r.bat, n.battery, col::INK);
        if (n.battery > 100)
            set_text(r.pct, "USB");
        else if (n.battery >= 0) {
            snprintf(buf, sizeof(buf), "%d%%", n.battery);
            set_text(r.pct, buf);
        } else
            set_text(r.pct, "--");
        signal_set(r.bars, n.stale ? 0 : n.signal, col::INK, n.stale ? 0xE3D3B5 : col::LINE);
    }
    rebuild_focus();
}

void HomeScreen::set_chats(const std::vector<Chat> &chats)
{
    // con el teclado sobre un chat, el foco sigue a ese chat aunque cambie de lugar
    std::string focus_id;
    lv_obj_t *f = lv_group_get_focused(group);
    for (size_t i = 0; i < chat_rows_.size() && i < chats_.size(); i++)
        if (chat_rows_[i].row == f)
            focus_id = chats_[i].id;
    // recordar el chat elegido por id
    std::string sel_id = (selected_ >= 0 && selected_ < (int)chats_.size()) ? chats_[selected_].id : "";
    chats_ = chats;
    selected_ = -1;
    for (size_t i = 0; i < chats_.size(); i++)
        if (chats_[i].id == sel_id)
            selected_ = (int)i;

    unsigned unread = 0;
    for (auto &ch : chats_)
        unread += ch.unread ? 1 : 0;
    char buf[24];
    if (unread)
        snprintf(buf, sizeof(buf), unread == 1 ? tr("1 nuevo") : tr("%u nuevos"), unread);
    else
        buf[0] = 0;
    set_text(chat_badge_, buf);

    int32_t cw = SCREEN_W - TEAM_W - 1;
    while (chat_rows_.size() < chats_.size()) {
        ChatRow r{};
        r.row = button(chat_list_, 0, 0, cw, ROW_H, col::SURF, 0, nullptr);
        lv_obj_set_style_radius(r.row, 0, 0);
        border(r.row, col::LINE2, 1, LV_BORDER_SIDE_BOTTOM);
        r.dot = box(r.row, 4, 5, 5, 5, col::BLUE);
        lv_obj_set_style_radius(r.dot, LV_RADIUS_CIRCLE, 0);
        r.name = label(r.row, &tfield_font_12b, col::INK, "");
        lv_obj_set_pos(r.name, 12, 1);
        r.tag = label(r.row, &tfield_font_12, col::INK2, "");
        r.time = label(r.row, &tfield_font_12, col::INK2, "");
        ui::align(r.time, LV_ALIGN_TOP_RIGHT, -5, 1);
        r.preview = label(r.row, &tfield_font_12, col::INK, "");
        lv_obj_set_pos(r.preview, 5, 14);
        lv_obj_set_size(r.preview, cw - 10, 14);
        lv_label_set_long_mode(r.preview, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_user_data(r.row, (void *)(intptr_t)chat_rows_.size());
        lv_obj_add_event_cb(r.row, on_chat, LV_EVENT_CLICKED, this);
        chat_rows_.push_back(r);
    }
    while (chat_rows_.size() > chats_.size()) {
        lv_obj_delete(chat_rows_.back().row);
        chat_rows_.pop_back();
    }
    rebuild_focus();
    for (size_t i = 0; i < chats_.size(); i++) {
        const Chat &ch = chats_[i];
        ChatRow &r = chat_rows_[i];
        set_text(r.name, ch.name);
        set_text(r.tag, ch.is_channel ? tr("canal") : tr("directo"));
        ui::align_to(r.tag, r.name, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, 0);
        set_text(r.time, ch.time);
        set_text(r.preview, ch.preview);
        restyle_chat_row((int)i);
    }
    if (!focus_id.empty())
        for (size_t i = 0; i < chats_.size(); i++)
            if (chats_[i].id == focus_id && lv_group_get_focused(group) != chat_rows_[i].row)
                lv_group_focus_obj(chat_rows_[i].row);
    refresh_chips();
}

// Orden de foco con teclado/trackball: chats → respuestas → pestañas → SOS → equipo.
// (el equipo al final: lo más usado es responder; ↑ desde el primer chat llega al equipo)
void HomeScreen::rebuild_focus()
{
    std::vector<lv_obj_t *> objs;
    for (auto &r : chat_rows_)
        objs.push_back(r.row);
    for (auto *chip : chips_)
        objs.push_back(chip);
    for (auto *tab : tabs_)
        objs.push_back(tab);
    for (auto &r : team_rows_)
        objs.push_back(r.row);
    objs.push_back(sos_); // el último: con ↑ desde el primer chat se llega directo
    regroup(group, objs);
}

void HomeScreen::restyle_chat_row(int i)
{
    ChatRow &r = chat_rows_[i];
    const Chat &ch = chats_[i];
    bool on = i == selected_;
    uint32_t fg = on ? col::SURF : col::INK, fg2 = on ? col::SURF : col::INK2;
    set_bg(r.row, on ? col::BLUE : col::SURF);
    set_border_color(r.row, on ? col::BLUE : col::LINE2);
    set_color(r.name, fg);
    set_color(r.tag, fg2);
    set_color(r.time, fg2);
    set_color(r.preview, fg);
    set_font(r.preview, ch.unread ? &tfield_font_12b : &tfield_font_12);
    set_bg(r.dot, on ? col::SURF : col::BLUE);
    if (ch.unread)
        show(r.dot, true);
    else
        show(r.dot, false);
}

void HomeScreen::set_quick(const QuickReplies &q)
{
    quick_ = q;
    for (int i = 0; i < 4; i++)
        set_text(chip_labels_[i], q.text[i].empty() ? tr("(vacía)") : q.text[i]);
    refresh_chips();
}

void HomeScreen::select_chat(int index)
{
    int prev = selected_;
    selected_ = (index == selected_) ? -1 : index;
    sent_shown_ = false;
    if (prev >= 0 && prev < (int)chat_rows_.size())
        restyle_chat_row(prev);
    if (selected_ >= 0)
        restyle_chat_row(selected_);
    refresh_chips();
}

void HomeScreen::refresh_chips()
{
    bool on = selected_ >= 0 && selected_ < (int)chats_.size();
    for (int i = 0; i < 4; i++) {
        bool enabled = on && !quick_.text[i].empty();
        if (enabled)
            lv_obj_remove_state(chips_[i], LV_STATE_DISABLED);
        else
            lv_obj_add_state(chips_[i], LV_STATE_DISABLED);
        set_color(chip_labels_[i], enabled ? col::BLUE : col::MUTED);
    }
    if (sent_shown_)
        return;
    if (on) {
        const Chat &ch = chats_[selected_];
        set_text(quick_label_, tr("Responder a ") + ch.name + (ch.is_channel ? tr(" (canal):") : ":"));
        set_color(quick_label_, col::BLUE);
    } else {
        set_text(quick_label_, tr("Elige un chat para responder"));
        set_color(quick_label_, col::INK2);
    }
}

void HomeScreen::clear_sent()
{
    sent_shown_ = false;
    refresh_chips();
}

std::string HomeScreen::selected_id() const
{
    return (selected_ >= 0 && selected_ < (int)chats_.size()) ? chats_[selected_].id : "";
}

void HomeScreen::notify_sent(const std::string &to, const std::string &text)
{
    sent_shown_ = true;
    set_text(quick_label_, tr("Enviado a ") + to + ": \xC2\xAB" + text + "\xC2\xBB");
    set_color(quick_label_, col::GREEN);
}

void HomeScreen::on_chat(lv_event_t *e)
{
    auto *self = static_cast<HomeScreen *>(lv_event_get_user_data(e));
    lv_obj_t *row = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
    self->select_chat((int)(intptr_t)lv_obj_get_user_data(row));
}

void HomeScreen::on_chip(lv_event_t *e)
{
    auto *self = static_cast<HomeScreen *>(lv_event_get_user_data(e));
    lv_obj_t *chip = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
    if (self->selected_ < 0 || lv_obj_has_state(chip, LV_STATE_DISABLED))
        return;
    for (int i = 0; i < 4; i++) {
        if (self->chips_[i] == chip) {
            Chat ch = self->chats_[self->selected_];
            std::string text = self->quick_.text[i];
            self->app_->send(ch.id, text);
            self->notify_sent(ch.name, text);
        }
    }
}

void HomeScreen::on_tab(lv_event_t *e)
{
    auto *self = static_cast<HomeScreen *>(lv_event_get_user_data(e));
    lv_obj_t *tab = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
    int i = (int)(intptr_t)lv_obj_get_user_data(tab);
    self->app_->show(TABS[i].target);
}

} // namespace tfield
