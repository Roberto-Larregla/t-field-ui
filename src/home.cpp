// Pantalla de inicio "Mosaico": cabecera con hora y barra de luz del día, y fichas de color
// que a la vez son los accesos (1 Mensajes, 2 Equipo, 3 Mapa, 4 Ajustes) más el SOS.
//
//   0 ┌ 14:32  vie 20 nov · 5 h 54 de luz        GEO1 ┐
//     │        06:27 ▬▬▬▬▬▬|▬▬▬▬ 20:26     ▭ 82% GPS 9 │
//  35 ├ 1 Mensajes ─────────────┬ 2 Equipo ─────────┤
//     │ chats (3 a la vista)     │ equipo (3 a la    │
//     │                          │ vista)            │
// 160 ├ Responder a …  ──────────┼ 3 Mapa ─┬ 4 Ajus ─┤
//     │ [resp. 1]   [resp. 2]    │ (dibujo)├─────────┤
//     │ [resp. 3]   [resp. 4]    │         │   SOS   │
// 238 └──────────────────────────┴─────────┴─────────┘
#include "tfield/app.h"
#include "tfield/i18n.h"
#include "tfield/format.h"
#include "tfield/ui.h"

#include <cstdio>
#include <cstdlib>

LV_IMAGE_DECLARE(tfield_picto_mail);
LV_IMAGE_DECLARE(tfield_picto_users);
LV_IMAGE_DECLARE(tfield_picto_map);
LV_IMAGE_DECLARE(tfield_picto_settings);

namespace tfield {

using namespace ui;

static constexpr int32_t TOP_H = 33;   // cabecera (incluye línea)
static constexpr int32_t GAP = 3;      // separación entre fichas
static constexpr int32_t ROW1_Y = 35, ROW1_H = 122;
static constexpr int32_t ROW2_Y = ROW1_Y + ROW1_H + GAP, ROW2_H = 78;
static constexpr int32_t LEFT_W = 196;                // Mensajes y respuestas
static constexpr int32_t RIGHT_X = GAP + LEFT_W + GAP; // Equipo, Mapa, Ajustes, SOS
static constexpr int32_t RIGHT_W = SCREEN_W - GAP - RIGHT_X;
static constexpr int32_t MAP_W = 62;
static constexpr int32_t SIDE_X = RIGHT_X + MAP_W + GAP, SIDE_W = SCREEN_W - GAP - SIDE_X;
static constexpr int32_t BAND_H = 18; // franja de color con número, dibujo y nombre
static constexpr int32_t ROW_H = 34;  // fila de chat o de equipo (2 renglones)
static constexpr int32_t BAR_X = 103, BAR_W = 78; // barra de luz del día

static const lv_image_dsc_t *picto(int i)
{
    static const lv_image_dsc_t *p[] = {&tfield_picto_mail, &tfield_picto_users, &tfield_picto_map,
                                        &tfield_picto_settings};
    return p[i];
}

// Dibujo A8: toma el color de image_recolor (sirve igual en modo claro y oscuro)
static lv_obj_t *picto_img(lv_obj_t *parent, int i, int32_t x, int32_t y, uint32_t color)
{
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, picto(i));
    lv_obj_set_pos(img, x, y);
    lv_obj_set_style_image_recolor(img, c(color), 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
}

// Cuadrito blanco con el número del acceso (la tecla que lo abre)
static void num_square(lv_obj_t *parent, int32_t x, int32_t y, const char *num, uint32_t color)
{
    lv_obj_t *sq = box(parent, x, y, 13, 13, col::SURF);
    lv_obj_set_style_radius(sq, 2, 0);
    lv_obj_center(label(sq, &tfield_font_12b, color, num));
}

// Destinos de las fichas, en el orden de sus números
static const ScreenId TARGETS[4] = {ScreenId::Messages, ScreenId::Team, ScreenId::Map, ScreenId::Settings};

// Ficha con franja de color arriba (la franja es el acceso; el cuerpo, una lista)
lv_obj_t *HomeScreen::tile(int i, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, const char *num,
                           const char *title, lv_obj_t **right)
{
    lv_obj_t *t = box(screen, x, y, w, h, color);
    lv_obj_set_style_radius(t, 4, 0);
    lv_obj_t *band = button(t, 0, 0, w, BAND_H, color, 0, nullptr);
    lv_obj_set_style_radius(band, 4, 0);
    lv_obj_set_style_bg_color(band, c(color == col::BLUE ? col::BLUE_DK : 0x2A2070), LV_STATE_PRESSED);
    num_square(band, 3, 2, num, color);
    picto_img(band, i, 19, 2, col::SURF);
    lv_obj_t *l = label(band, &tfield_font_12b, col::SURF, title);
    lv_obj_set_pos(l, 36, 1);
    if (right) {
        *right = label(band, &tfield_font_12b, col::SURF, "");
        ui::align(*right, LV_ALIGN_TOP_RIGHT, -5, 1);
    }
    lv_obj_set_user_data(band, (void *)(intptr_t)i);
    lv_obj_add_event_cb(band, on_tab, LV_EVENT_CLICKED, this);
    tabs_[i] = band;
    lv_obj_t *body = box(t, 1, BAND_H, w - 2, h - BAND_H - 1, col::SURF);
    lv_obj_set_style_radius(body, 3, 0);
    return body;
}

void HomeScreen::create(App *app)
{
    app_ = app;
    screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen);
    set_bg(screen, col::BG);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    group = lv_group_create();

    // --- Cabecera: hora, fecha, luz del día, nodo propio
    lv_obj_t *top = box(screen, 0, 0, SCREEN_W, TOP_H, col::SURF);
    border(top, col::INK, 1, LV_BORDER_SIDE_BOTTOM);
    clock_ = label(top, &tfield_font_24b, col::INK, "--:--");
    lv_obj_set_pos(clock_, 6, 2);
    date_ = label(top, &tfield_font_12, col::INK2, "");
    lv_obj_set_pos(date_, 72, 2);
    daylight_ = label(top, &tfield_font_12b, col::AMBER, "");
    name_ = label(top, &tfield_font_12b, col::INK, "");
    ui::align(name_, LV_ALIGN_TOP_RIGHT, -6, 2);
    // barra: de la salida a la puesta del sol; gris lo que pasó, ámbar la luz que queda
    rise_ = label(top, &tfield_font_12, col::INK2, "");
    lv_obj_set_pos(rise_, 72, 16);
    bar_ = box(top, BAR_X, 21, BAR_W, 6, col::LINE);
    lv_obj_set_style_radius(bar_, 3, 0);
    lv_obj_set_style_clip_corner(bar_, true, 0);
    bar_left_ = box(bar_, 0, 0, 0, 6, col::AMBER);
    now_mark_ = box(top, BAR_X, 18, 2, 12, col::INK);
    set_ = label(top, &tfield_font_12, col::INK2, "");
    lv_obj_set_pos(set_, BAR_X + BAR_W + 3, 16);
    gps_ = label(top, &tfield_font_12b, col::INK, "");
    ui::align(gps_, LV_ALIGN_TOP_RIGHT, -6, 16);
    bat_pct_ = label(top, &tfield_font_12b, col::INK, "");
    bat_ = battery(top, 0, 0);

    // --- 1 Mensajes
    lv_obj_t *msg = tile(0, GAP, ROW1_Y, LEFT_W, ROW1_H, col::BLUE, "1", tr("Mensajes"), &chat_badge_);
    chat_list_ = box(msg, 0, 0, LEFT_W - 2, ROW1_H - BAND_H - 1, col::SURF);
    lv_obj_set_style_bg_opa(chat_list_, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(chat_list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(chat_list_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_flex_flow(chat_list_, LV_FLEX_FLOW_COLUMN);

    // --- 2 Equipo
    lv_obj_t *team = tile(1, RIGHT_X, ROW1_Y, RIGHT_W, ROW1_H, col::PURPLE, "2", tr("Equipo"), &team_count_);
    team_list_ = box(team, 0, 0, RIGHT_W - 2, ROW1_H - BAND_H - 1, col::SURF);
    lv_obj_set_style_bg_opa(team_list_, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(team_list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(team_list_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_flex_flow(team_list_, LV_FLEX_FLOW_COLUMN);
    // sin equipo todavía (primer encendido): cómo armarlo
    team_empty_ = label(team, &tfield_font_12, col::INK2, tr("Sin equipo todav\xC3\xAD" "a.\nM\xC3\xA1rcalo en Equipo \xE2\x80\xBA Toda la red \xE2\x80\xBA ficha del nodo."));
    lv_obj_set_pos(team_empty_, 5, 4);
    lv_obj_set_width(team_empty_, RIGHT_W - 12);
    lv_label_set_long_mode(team_empty_, LV_LABEL_LONG_MODE_WRAP);
    show(team_empty_, false);

    // --- Respuestas rápidas (máximo QUICK_MAX_CHARS: caben completas en el botón)
    lv_obj_t *q = box(screen, GAP, ROW2_Y, LEFT_W, ROW2_H, col::SURF);
    lv_obj_set_style_radius(q, 4, 0);
    border(q, col::BLUE, 1);
    quick_label_ = label(q, &tfield_font_12b, col::INK2, tr("Elige un chat para responder"));
    lv_obj_set_pos(quick_label_, 5, 2);
    lv_obj_set_size(quick_label_, LEFT_W - 10, 14);
    lv_label_set_long_mode(quick_label_, LV_LABEL_LONG_MODE_DOTS);
    for (int i = 0; i < 4; i++) {
        int32_t x = (i % 2) ? 98 : 2, y = (i / 2) ? 47 : 18, w = 94;
        chips_[i] = button(q, x, y, w, 26, col::SURF, col::BLUE, nullptr);
        lv_obj_set_style_bg_color(chips_[i], c(col::BLUE), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(chips_[i], c(col::LINE), LV_STATE_DISABLED);
        chip_labels_[i] = label(chips_[i], &tfield_font_12b, col::BLUE, "");
        lv_obj_set_size(chip_labels_[i], w - 4, 14);
        lv_obj_set_style_text_align(chip_labels_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(chip_labels_[i], LV_LABEL_LONG_MODE_DOTS);
        lv_obj_center(chip_labels_[i]);
        lv_obj_set_style_text_color(chip_labels_[i], c(col::SURF), LV_STATE_PRESSED);
        lv_obj_add_event_cb(chips_[i], on_chip, LV_EVENT_CLICKED, this);
    }

    // --- 3 Mapa: ficha entera; avisa si hay una ruta grabándose
    lv_obj_t *map = button(screen, RIGHT_X, ROW2_Y, MAP_W, ROW2_H, col::GREEN, 0, nullptr);
    lv_obj_set_style_radius(map, 4, 0);
    lv_obj_set_style_bg_color(map, c(0x1F4D24), LV_STATE_PRESSED);
    num_square(map, 4, 4, "3", col::GREEN);
    picto_img(map, 2, (MAP_W - 28) / 2, 13, col::SURF);
    lv_obj_t *ml = label(map, &tfield_font_12b, col::SURF, tr("Mapa"));
    ui::align(ml, LV_ALIGN_TOP_MID, 0, 42);
    rec_label_ = label(map, &tfield_font_12, col::SURF, tr("Grabando"));
    ui::align(rec_label_, LV_ALIGN_TOP_MID, 0, 58);
    rec_dot_ = box(map, MAP_W - 13, 4, 9, 9, col::RED);
    lv_obj_set_style_radius(rec_dot_, LV_RADIUS_CIRCLE, 0);
    border(rec_dot_, col::SURF, 2);
    show(rec_label_, false);
    show(rec_dot_, false);
    lv_obj_set_user_data(map, (void *)(intptr_t)2);
    lv_obj_add_event_cb(map, on_tab, LV_EVENT_CLICKED, this);
    tabs_[2] = map;

    // --- 4 Ajustes
    int32_t side_h = (ROW2_H - GAP) / 2;
    lv_obj_t *set = button(screen, SIDE_X, ROW2_Y, SIDE_W, side_h, col::SLATE, 0, nullptr);
    lv_obj_set_style_radius(set, 4, 0);
    lv_obj_set_style_bg_color(set, c(0x2A353D), LV_STATE_PRESSED);
    num_square(set, 4, 3, "4", col::SLATE);
    picto_img(set, 3, SIDE_W - 21, 2, col::SURF);
    lv_obj_t *sl = label(set, &tfield_font_12b, col::SURF, tr("Ajustes"));
    ui::align(sl, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_user_data(set, (void *)(intptr_t)3);
    lv_obj_add_event_cb(set, on_tab, LV_EVENT_CLICKED, this);
    tabs_[3] = set;

    // --- SOS: mantener 2 s
    sos_ = hold_button(screen, SIDE_X, ROW2_Y + side_h + GAP, SIDE_W, ROW2_H - side_h - GAP, col::RED, 0x7A1712,
                       col::SURF, "SOS", 2000, [](void *u) { static_cast<App *>(u)->trigger_sos(); }, app, nullptr);
    lv_obj_set_style_radius(sos_, 4, 0);
    rebuild_focus();
    refresh_chips();
}

// "HH:MM" → minutos desde medianoche (-1 si no viene)
static int hhmm_min(const std::string &s)
{
    if (s.size() < 5 || s[2] != ':')
        return -1;
    return std::atoi(s.substr(0, 2).c_str()) * 60 + std::atoi(s.substr(3, 2).c_str());
}

void HomeScreen::set_status(const Status &s)
{
    char buf[32];
    if (s.epoch) // sin hora (aún sin GPS ni red): no inventar "00:00"
        snprintf(buf, sizeof(buf), "%02u:%02u", s.hour, s.minute);
    else
        snprintf(buf, sizeof(buf), "--:--");
    set_text(clock_, buf);
    set_text(date_, s.day.empty() ? std::string() : s.day + " " + s.month);

    // luz del día: texto y barra (gris lo que pasó desde la salida del sol, ámbar lo que queda)
    int rise = hhmm_min(s.sunrise), set = hhmm_min(s.sunset);
    bool sun = s.daylight_min >= 0 && s.epoch && rise >= 0 && set > rise;
    if (s.daylight_min > 0) {
        set_text(daylight_, format_daylight(s.daylight_min) + tr(" de luz"));
        set_color(daylight_, col::AMBER);
    } else if (s.daylight_min == 0) {
        set_text(daylight_, tr("Noche"));
        set_color(daylight_, col::INK2);
    } else {
        set_text(daylight_, tr("Sin posici\xC3\xB3n"));
        set_color(daylight_, col::INK2);
    }
    ui::align_to(daylight_, date_, LV_ALIGN_OUT_RIGHT_MID, s.day.empty() ? 0 : 6, 0);
    show(rise_, sun);
    show(set_, sun);
    show(bar_, sun);
    show(now_mark_, sun && s.daylight_min > 0);
    if (sun) {
        set_text(rise_, s.sunrise);
        set_text(set_, s.sunset);
        int32_t left = s.daylight_min > 0 ? LV_CLAMP(0, s.daylight_min * BAR_W / (set - rise), BAR_W) : 0;
        if (lv_obj_get_x(bar_left_) != BAR_W - left || lv_obj_get_width(bar_left_) != left) {
            lv_obj_set_pos(bar_left_, BAR_W - left, 0);
            lv_obj_set_width(bar_left_, left);
        }
        lv_obj_set_pos(now_mark_, BAR_X + LV_CLAMP(0, BAR_W - left - 1, BAR_W - 2), 18);
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

void HomeScreen::set_recording(bool on)
{
    show(rec_label_, on);
    show(rec_dot_, on);
}

void HomeScreen::set_team(const std::vector<TeamNode> &team)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", (unsigned)team.size());
    set_text(team_count_, team.empty() ? "" : buf);

    // Crear o eliminar filas solo si cambia la cantidad.
    int32_t rw = RIGHT_W - 2;
    while (team_rows_.size() < team.size()) {
        TeamRow r{};
        r.row = button(team_list_, 0, 0, rw, ROW_H, col::SURF, 0, nullptr);
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
        // renglón 1: color · nombre ……… señal · batería %
        r.dot = box(r.row, 4, 5, 7, 7, col::INK);
        lv_obj_set_style_radius(r.dot, LV_RADIUS_CIRCLE, 0);
        r.name = label(r.row, &tfield_font_12b, col::INK, "");
        lv_obj_set_pos(r.name, 14, 1);
        lv_obj_set_size(r.name, 34, 14);
        lv_label_set_long_mode(r.name, LV_LABEL_LONG_MODE_CLIP);
        r.pct = label(r.row, &tfield_font_12, col::INK, "");
        r.bat = battery(r.row, 0, 0);
        r.bars = signal_bars(r.row, 0, 0);
        // renglón 2: distancia y rumbo · desnivel
        r.dist = label(r.row, &tfield_font_12b, col::INK, "");
        lv_obj_set_pos(r.dist, 4, 17);
        r.alt = label(r.row, &tfield_font_12, col::INK, "");
        r.heard = nullptr;
        team_rows_.push_back(r);
    }
    while (team_rows_.size() > team.size()) {
        lv_obj_delete(team_rows_.back().row);
        team_rows_.pop_back();
    }
    show(team_empty_, team.empty());

    for (size_t i = 0; i < team.size(); i++) {
        const TeamNode &n = team[i];
        TeamRow &r = team_rows_[i];
        lv_obj_set_user_data(r.row, (void *)(uintptr_t)n.num);
        set_bg(r.row, n.stale ? col::AMBER_BG : col::SURF); // sin noticias hace rato: ámbar
        set_bg(r.dot, n.color);
        set_text(r.name, n.name);
        set_color(r.name, n.stale ? col::AMBER : col::INK);
        if (n.battery > 100)
            set_text(r.pct, "USB");
        else if (n.battery >= 0) {
            snprintf(buf, sizeof(buf), "%d%%", n.battery);
            set_text(r.pct, buf);
        } else
            set_text(r.pct, "--");
        ui::align(r.pct, LV_ALIGN_TOP_RIGHT, -4, 1);
        ui::align_to(r.bat, r.pct, LV_ALIGN_OUT_LEFT_MID, -2, 1);
        ui::align_to(r.bars, r.bat, LV_ALIGN_OUT_LEFT_MID, -4, -1);
        battery_set(r.bat, n.battery, col::INK);
        signal_set(r.bars, n.stale ? 0 : n.signal, col::INK, n.stale ? 0xE3D3B5 : col::LINE);
        if (n.dist_m >= 0)
            set_text(r.dist, format_distance(n.dist_m) + " " + cardinal(n.bearing_deg));
        else
            set_text(r.dist, tr("sin posici\xC3\xB3n"));
        set_text(r.alt, n.has_alt && n.dist_m >= 0 ? format_alt_diff(n.alt_diff_m) : "");
        ui::align_to(r.alt, r.dist, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
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

    int32_t cw = LEFT_W - 2;
    while (chat_rows_.size() < chats_.size()) {
        ChatRow r{};
        r.row = button(chat_list_, 0, 0, cw, ROW_H, col::SURF, 0, nullptr);
        lv_obj_set_style_radius(r.row, 0, 0);
        border(r.row, col::LINE2, 1, LV_BORDER_SIDE_BOTTOM);
        r.dot = box(r.row, 5, 6, 6, 6, col::BLUE);
        lv_obj_set_style_radius(r.dot, LV_RADIUS_CIRCLE, 0);
        r.name = label(r.row, &tfield_font_12b, col::INK, "");
        lv_obj_set_pos(r.name, 14, 2);
        r.tag = label(r.row, &tfield_font_12, col::INK2, "");
        r.time = label(r.row, &tfield_font_12, col::INK2, "");
        ui::align(r.time, LV_ALIGN_TOP_RIGHT, -5, 2);
        r.preview = label(r.row, &tfield_font_12, col::INK, "");
        lv_obj_set_pos(r.preview, 5, 17);
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

// Orden de foco con teclado/trackball: chats → respuestas → fichas 1-4 → equipo → SOS.
// (lo más usado es responder; ↑ desde el primer chat llega al SOS, el último)
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
    objs.push_back(sos_);
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
    show(r.dot, ch.unread);
}

void HomeScreen::set_quick(const QuickReplies &q)
{
    quick_ = q;
    for (int i = 0; i < 4; i++)
        set_text(chip_labels_[i], q.text[i].empty() ? tr("(vac\xC3\xAD" "a)") : q.text[i]);
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
    self->app_->show(TARGETS[i]);
}

} // namespace tfield
