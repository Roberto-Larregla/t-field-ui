// Pantalla de equipo: lista de nodos (mi equipo / toda la red) y ficha de cada nodo.
#include "tfield/app.h"
#include "tfield/i18n.h"
#include "tfield/format.h"
#include "tfield/ui.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace tfield {

using namespace ui;

static constexpr int32_t HEAD_H = 28;
static constexpr int32_t SUM_H = 22;
static constexpr int32_t ROW_H = 34;

enum DetailButton { B_MSG, B_GOTO, B_MAP, B_POS, B_ROUTE, B_TEAM };

void TeamScreen::create(App *app)
{
    app_ = app;
    screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen);
    set_bg(screen, col::SURF);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    group = lv_group_create();

    lv_obj_t *h = header_bar(screen, app, group, tr("Equipo"));
    // Selector Mi equipo | Toda la red
    static const char *seg_text[2] = {"Mi equipo", "Toda la red"};
    for (int i = 0; i < 2; i++) {
        seg_[i] = button(h, 166 + i * 76, 5, 74, 18, col::SURF, col::PURPLE, group);
        lv_obj_set_style_radius(seg_[i], 2, 0);
        seg_lbl_[i] = label(seg_[i], &tfield_font_12b, col::PURPLE, tr(seg_text[i]));
        lv_obj_center(seg_lbl_[i]);
        lv_obj_set_user_data(seg_[i], (void *)(intptr_t)i);
        lv_obj_add_event_cb(
            seg_[i], [](lv_event_t *e) {
                auto *s = static_cast<TeamScreen *>(lv_event_get_user_data(e));
                s->set_filter((intptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e)) == 1);
            },
            LV_EVENT_CLICKED, this);
    }

    lv_obj_t *sum = box(screen, 0, HEAD_H, SCREEN_W, SUM_H, col::BG);
    border(sum, col::LINE2, 1, LV_BORDER_SIDE_BOTTOM);
    summary_ = label(sum, &tfield_font_12, col::INK2, "");
    lv_obj_set_pos(summary_, 6, 4);
    lv_obj_set_size(summary_, SCREEN_W - 118, 14);
    lv_label_set_long_mode(summary_, LV_LABEL_LONG_MODE_DOTS);
    // pedir la posición a todo el equipo de una vez
    ask_all_ = button(sum, SCREEN_W - 108, 2, 104, 18, col::SURF, col::PURPLE, group);
    lv_obj_set_style_radius(ask_all_, 2, 0);
    lv_obj_set_style_bg_color(ask_all_, c(col::PURPLE), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(ask_all_, c(col::LINE), LV_STATE_DISABLED);
    ask_all_lbl_ = label(ask_all_, &tfield_font_12b, col::PURPLE, tr("Pedir posiciones"));
    lv_obj_center(ask_all_lbl_);
    lv_obj_add_event_cb(
        ask_all_, [](lv_event_t *e) {
            auto *s = static_cast<TeamScreen *>(lv_event_get_user_data(e));
            if (s->app_->request_team_positions() > 0) {
                // un pedido por minuto como máximo: cada uno ocupa la radio una vez por nodo
                lv_obj_add_state(s->ask_all_, LV_STATE_DISABLED);
                set_color(s->ask_all_lbl_, col::MUTED);
                if (s->ask_timer_)
                    lv_timer_delete(s->ask_timer_);
                s->ask_timer_ = lv_timer_create(
                    [](lv_timer_t *t) {
                        auto *s = static_cast<TeamScreen *>(lv_timer_get_user_data(t));
                        s->ask_timer_ = nullptr;
                        lv_obj_remove_state(s->ask_all_, LV_STATE_DISABLED);
                        set_color(s->ask_all_lbl_, col::PURPLE);
                    },
                    60000, s);
                lv_timer_set_repeat_count(s->ask_timer_, 1);
            }
        },
        LV_EVENT_CLICKED, this);

    list_ = box(screen, 0, HEAD_H + SUM_H, SCREEN_W, SCREEN_H - HEAD_H - SUM_H, col::SURF);
    lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_ACTIVE);

    // --- Ficha (encima de la lista)
    detail_group_ = lv_group_create();
    detail_ = box(screen, 0, HEAD_H, SCREEN_W, SCREEN_H - HEAD_H, col::SURF);
    lv_obj_t *tb = box(detail_, 0, 0, SCREEN_W, 22, col::BG);
    border(tb, col::LINE, 1, LV_BORDER_SIDE_BOTTOM);
    d_dot_ = box(tb, 6, 7, 9, 9, col::PURPLE);
    lv_obj_set_style_radius(d_dot_, LV_RADIUS_CIRCLE, 0);
    d_name_ = label(tb, &tfield_font_14b, col::INK, "");
    lv_obj_set_pos(d_name_, 20, 3);
    d_role_ = label(tb, &tfield_font_12, col::INK2, "");
    ui::align(d_role_, LV_ALIGN_TOP_RIGHT, -6, 4);

    // 4 filas de dos datos: etiqueta pequeña + valor
    static const char *labels[8] = {"Distancia y rumbo", "Desnivel", "Posici\xC3\xB3n UTM", "",
                                    "Bater\xC3\xAD" "a", "Se\xC3\xB1" "al (SNR \xC2\xB7 RSSI)",  "Escuchado",  "Equipo"};
    for (int i = 0; i < 8; i++) {
        int r = i / 2, col_i = i % 2;
        int32_t x = col_i ? 176 : 6, y = 25 + r * 30;
        if (i == 3)
            continue; // la posición ocupa toda la fila
        lv_obj_t *l = label(detail_, &tfield_font_12, col::INK2, tr(labels[i]));
        lv_obj_set_pos(l, x, y);
        if (i == 2)
            d_pos_lbl_ = l;
        d_val_[i] = label(detail_, &tfield_font_14b, col::INK, "");
        lv_obj_set_pos(d_val_[i], x, y + 12);
        lv_obj_set_size(d_val_[i], (i == 2) ? SCREEN_W - 12 : (col_i ? SCREEN_W - 182 : 166), 17);
        lv_label_set_long_mode(d_val_[i], LV_LABEL_LONG_MODE_DOTS);
    }
    d_status_ = label(detail_, &tfield_font_12b, col::INK2, "");
    lv_obj_set_pos(d_status_, 6, 145);
    lv_obj_set_size(d_status_, SCREEN_W - 12, 14);
    lv_label_set_long_mode(d_status_, LV_LABEL_LONG_MODE_DOTS);

    static const char *btn_text[6] = {"Mensaje", "Ir hacia", "Ver en mapa", "Pedir posici\xC3\xB3n", "Ruta de se\xC3\xB1" "al", "En mi equipo"};
    for (int i = 0; i < 6; i++) {
        int32_t x = 6 + (i % 3) * 104, y = 162 + (i / 3) * 26;
        d_btn_[i] = button(detail_, x, y, 100, 22, col::SURF, col::INK, detail_group_);
        lv_obj_set_style_bg_color(d_btn_[i], c(col::BG), LV_STATE_PRESSED);
        d_btn_lbl_[i] = label(d_btn_[i], &tfield_font_12b, col::INK, tr(btn_text[i]));
        lv_obj_center(d_btn_lbl_[i]);
        lv_obj_set_user_data(d_btn_[i], (void *)(intptr_t)i);
        lv_obj_add_event_cb(
            d_btn_[i], [](lv_event_t *e) {
                auto *s = static_cast<TeamScreen *>(lv_event_get_user_data(e));
                int b = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e));
                uint32_t num = s->detail_num_;
                switch (b) {
                case B_MSG: s->app_->open_direct_chat(num); break;
                case B_GOTO: s->app_->navigate_to_node(num); break;
                case B_MAP: s->app_->show_node_on_map(num); break;
                case B_POS: s->app_->request_position(num); break;
                case B_ROUTE: s->app_->traceroute(num); break;
                case B_TEAM: {
                    const TeamNode *n = s->find(num);
                    if (n)
                        s->app_->set_in_team(num, !n->in_team);
                    break;
                }
                }
            },
            LV_EVENT_CLICKED, this);
    }
    // "Mensaje" es la acción principal
    set_bg(d_btn_[B_MSG], col::BLUE);
    set_border_color(d_btn_[B_MSG], col::BLUE);
    lv_obj_set_style_bg_color(d_btn_[B_MSG], c(col::BLUE_DK), LV_STATE_PRESSED);
    set_color(d_btn_lbl_[B_MSG], col::SURF);
    show(detail_, false);
    set_filter(false);
}

const TeamNode *TeamScreen::find(uint32_t num) const
{
    for (auto &n : app_->nodes())
        if (n.num == num)
            return &n;
    return nullptr;
}

void TeamScreen::set_filter(bool all)
{
    show_all_ = all;
    for (int i = 0; i < 2; i++) {
        bool on = (i == 1) == all;
        set_bg(seg_[i], on ? col::PURPLE : col::SURF);
        set_color(seg_lbl_[i], on ? col::SURF : col::PURPLE);
    }
    rebuild_list();
}

void TeamScreen::set_nodes(const std::vector<TeamNode> &)
{
    // con 250 nodos llegan paquetes seguido: sin la pantalla a la vista, se rehace al entrar
    if (lv_screen_active() != screen) {
        dirty_ = true;
        return;
    }
    rebuild_list();
    if (detail_num_)
        refresh_detail();
}

void TeamScreen::rebuild_list()
{
    dirty_ = false;
    const std::vector<TeamNode> &nodes_ = app_->nodes(); // sin copia
    // resumen del equipo
    int team = 0, live = 0;
    uint32_t last = UINT32_MAX;
    for (auto &n : nodes_) {
        if (!n.in_team)
            continue;
        team++;
        if (!n.stale)
            live++;
        last = std::min(last, n.heard_ago_s);
    }
    char buf[96];
    if (team == 0)
        snprintf(buf, sizeof(buf), tr("Sin equipo \xC2\xB7 m\xC3\xA1rcalos en Toda la red"));
    else if (show_all_)
        snprintf(buf, sizeof(buf), tr("%d de %d en contacto \xC2\xB7 %d m\xC3\xA1s en la red"), live, team, (int)nodes_.size() - team);
    else
        snprintf(buf, sizeof(buf), tr("%d de %d en contacto \xC2\xB7 \xC3\xBAltimo: %s"), live, team, format_ago(last).c_str());
    if (!note_.empty()) {
        set_text(summary_, note_);
        set_color(summary_, note_color_);
        set_font(summary_, &tfield_font_12b);
    } else {
        set_text(summary_, buf);
        set_color(summary_, col::INK2);
        set_font(summary_, &tfield_font_12);
    }
    if (team == 0)
        show(ask_all_, false);
    else
        show(ask_all_, true);

    std::vector<const TeamNode *> shown;
    for (auto &n : nodes_)
        if (show_all_ || n.in_team)
            shown.push_back(&n);
    // equipo primero; dentro de cada grupo, por distancia (sin posición al final)
    std::stable_sort(shown.begin(), shown.end(), [](const TeamNode *a, const TeamNode *b) {
        if (a->in_team != b->in_team)
            return a->in_team;
        uint32_t da = a->dist_m < 0 ? UINT32_MAX : (uint32_t)a->dist_m, db = b->dist_m < 0 ? UINT32_MAX : (uint32_t)b->dist_m;
        return da < db;
    });
    // "Toda la red" con 250 nodos serían ~3000 objetos: se muestran los 50 ajenos más cercanos
    size_t team_n = (size_t)team, cap = team_n + TEAM_LIST_OTHERS, total = shown.size();
    if (shown.size() > cap)
        shown.resize(cap);

    while (rows_.size() < shown.size()) {
        Row r{};
        r.row = button(list_, 0, 0, SCREEN_W, ROW_H, col::SURF, 0, group);
        lv_obj_set_style_radius(r.row, 0, 0);
        border(r.row, col::LINE2, 1, LV_BORDER_SIDE_BOTTOM);
        lv_obj_set_style_bg_color(r.row, c(col::BG), LV_STATE_PRESSED);
        r.dot = box(r.row, 7, 6, 9, 9, col::INK);
        lv_obj_set_style_radius(r.dot, LV_RADIUS_CIRCLE, 0);
        r.name = label(r.row, &tfield_font_14b, col::INK, "");
        lv_obj_set_pos(r.name, 22, 1);
        r.lname = label(r.row, &tfield_font_12, col::INK2, "");
        r.bars = signal_bars(r.row, SCREEN_W - 6 - 11, 4);
        r.heard = label(r.row, &tfield_font_12, col::INK2, "");
        r.dist = label(r.row, &tfield_font_12, col::INK, "");
        lv_obj_set_pos(r.dist, 22, 18);
        r.alt = label(r.row, &tfield_font_12, col::INK, "");
        lv_obj_set_pos(r.alt, 100, 18);
        r.bat = battery(r.row, 150, 22);
        r.pct = label(r.row, &tfield_font_12, col::INK, "");
        lv_obj_set_pos(r.pct, 166, 18);
        r.hops = label(r.row, &tfield_font_12, col::INK2, "");
        lv_obj_set_pos(r.hops, 210, 18);
        lv_obj_add_event_cb(
            r.row, [](lv_event_t *e) {
                auto *s = static_cast<TeamScreen *>(lv_event_get_user_data(e));
                lv_obj_t *row = (lv_obj_t *)lv_event_get_current_target(e);
                for (size_t i = 0; i < s->rows_.size(); i++)
                    if (s->rows_[i].row == row && i < s->row_nums_.size())
                        s->app_->show_node(s->row_nums_[i]); // queda en el historial: Esc vuelve a la lista
            },
            LV_EVENT_CLICKED, this);
        rows_.push_back(r);
    }
    while (rows_.size() > shown.size()) {
        lv_obj_delete(rows_.back().row);
        rows_.pop_back();
    }
    row_nums_.clear();
    for (size_t i = 0; i < shown.size(); i++) {
        const TeamNode &n = *shown[i];
        Row &r = rows_[i];
        row_nums_.push_back(n.num);
        bool fresh = n.heard_ago_s < 60;
        set_bg(r.row, n.stale ? col::AMBER_BG : col::SURF);
        // fuera del equipo: punto hueco gris
        set_bg(r.dot, n.in_team ? n.color : col::SURF);
        set_border_color(r.dot, n.in_team ? n.color : col::MUTED);
        lv_obj_set_style_border_width(r.dot, n.in_team ? 0 : 2, 0);
        set_text(r.name, n.name);
        set_text(r.lname, n.long_name + (n.in_team ? "" : tr(" \xC2\xB7 fuera del equipo")));
        ui::align_to(r.lname, r.name, LV_ALIGN_OUT_RIGHT_BOTTOM, 5, -1);
        set_text(r.heard, format_ago(n.heard_ago_s));
        set_font(r.heard, (fresh || n.stale) ? &tfield_font_12b : &tfield_font_12);
        set_color(r.heard, n.stale ? col::AMBER : (fresh ? col::GREEN : col::INK2));
        ui::align(r.heard, LV_ALIGN_TOP_RIGHT, -22, 2);
        signal_set(r.bars, n.stale ? 0 : n.signal, col::INK, n.stale ? 0xE3D3B5 : col::LINE);
        set_text(r.dist, n.dist_m >= 0 ? format_distance(n.dist_m) + " " + cardinal(n.bearing_deg) : tr("sin posici\xC3\xB3n"));
        set_text(r.alt, n.has_alt && n.dist_m >= 0 ? format_alt_diff(n.alt_diff_m) : "");
        battery_set(r.bat, n.battery, col::INK);
        char pb[12];
        if (n.battery > 100)
            snprintf(pb, sizeof(pb), "USB");
        else if (n.battery >= 0)
            snprintf(pb, sizeof(pb), "%d%%", n.battery);
        else
            snprintf(pb, sizeof(pb), "--");
        set_text(r.pct, pb);
        set_text(r.hops, format_hops(n.hops));
    }
    // aviso al final cuando la lista se acorta
    if (!more_) {
        more_ = label(list_, &tfield_font_12, col::INK2, "");
        lv_obj_set_width(more_, SCREEN_W - 12);
        lv_obj_set_style_pad_all(more_, 6, 0);
    }
    if (total > shown.size()) {
        char mb[96];
        snprintf(mb, sizeof(mb), tr("Se muestran los %u m\xC3\xA1s cercanos de %u fuera del equipo."), (unsigned)TEAM_LIST_OTHERS,
                 (unsigned)(total - team_n));
        set_text(more_, mb);
        show(more_, true);
        keep_on_top({more_});
    } else
        show(more_, false);
}

void TeamScreen::show_list()
{
    if (dirty_)
        rebuild_list();
    detail_num_ = 0;
    show(detail_, false);
    app_->refresh_input_group();
}

void TeamScreen::show_detail(uint32_t num)
{
    if (dirty_)
        rebuild_list();
    if (!find(num))
        return;
    if (status_num_ != num) {
        status_text_.clear();
        status_num_ = 0;
    }
    detail_num_ = num;
    refresh_detail();
    show(detail_, true);
    lv_obj_move_foreground(detail_);
    lv_group_focus_obj(d_btn_[B_MSG]);
    app_->refresh_input_group();
}

void TeamScreen::refresh_detail()
{
    const TeamNode *n = find(detail_num_);
    if (!n) {
        show_list();
        return;
    }
    char buf[96];
    set_bg(d_dot_, n->in_team ? n->color : col::MUTED);
    set_text(d_name_, n->long_name.empty() ? n->name : n->name + " \xC2\xB7 " + n->long_name);
    set_text(d_role_, tr(n->role.c_str())); // vocabulario cerrado del adaptador (cliente, repetidor…)
    ui::align(d_role_, LV_ALIGN_TOP_RIGHT, -6, 4);

    if (n->dist_m >= 0) {
        snprintf(buf, sizeof(buf), "%s %s %d\xC2\xB0", format_distance(n->dist_m).c_str(), cardinal(n->bearing_deg), n->bearing_deg);
        set_text(d_val_[0], buf);
        set_text(d_val_[1], n->has_alt ? format_alt_diff(n->alt_diff_m) : "--");
        set_text(d_val_[2], format_utm(to_utm(n->lat, n->lon)));
        set_text(d_pos_lbl_, tr("Posici\xC3\xB3n UTM \xC2\xB7 ") + ago_text(n->pos_age_s) +
                                 (n->acc_m ? " \xC2\xB7 \xC2\xB1" + std::to_string(n->acc_m) + tr(" m") : ""));
    } else {
        set_text(d_val_[0], tr("sin posici\xC3\xB3n"));
        set_text(d_val_[1], "--");
        set_text(d_val_[2], tr("Este nodo no ha enviado posici\xC3\xB3n"));
        set_text(d_pos_lbl_, tr("Posici\xC3\xB3n UTM"));
    }
    if (n->battery > 100)
        snprintf(buf, sizeof(buf), tr("USB \xC2\xB7 %.2f V"), n->voltage);
    else if (n->battery >= 0 && n->voltage > 0)
        snprintf(buf, sizeof(buf), tr("%d %% \xC2\xB7 %.2f V"), n->battery, n->voltage);
    else if (n->battery >= 0)
        snprintf(buf, sizeof(buf), "%d %%", n->battery);
    else
        snprintf(buf, sizeof(buf), "--");
    std::string bat = buf;
    for (auto &ch : bat) // coma decimal en español
        if (ch == '.' && !english())
            ch = ',';
    set_text(d_val_[4], bat);
    snprintf(buf, sizeof(buf), tr("%.1f dB \xC2\xB7 %s%d dBm"), n->snr, n->rssi < 0 ? "\xE2\x88\x92" : "", std::abs(n->rssi));
    std::string sig = buf;
    for (auto &ch : sig)
        if (ch == '.' && !english())
            ch = ',';
    set_text(d_val_[5], n->stale ? tr("sin se\xC3\xB1" "al reciente") : sig);
    std::string heard = ago_text(n->heard_ago_s);
    if (n->hops >= 0)
        heard += " \xC2\xB7 " + format_hops(n->hops);
    set_text(d_val_[6], heard);
    set_color(d_val_[6], n->stale ? col::AMBER : col::INK);
    set_text(d_val_[7], n->hw.empty() ? "--" : n->hw);

    // "En mi equipo": relleno morado si está dentro
    set_bg(d_btn_[B_TEAM], n->in_team ? col::PURPLE : col::SURF);
    set_border_color(d_btn_[B_TEAM], n->in_team ? col::PURPLE : col::INK);
    set_color(d_btn_lbl_[B_TEAM], n->in_team ? col::SURF : col::INK);
    set_text(d_btn_lbl_[B_TEAM], n->in_team ? tr("En mi equipo") : tr("Agregar a equipo"));
    lv_obj_center(d_btn_lbl_[B_TEAM]);
    bool pos = n->dist_m >= 0;
    for (int b : {B_GOTO, B_MAP}) {
        if (pos)
            lv_obj_remove_state(d_btn_[b], LV_STATE_DISABLED);
        else
            lv_obj_add_state(d_btn_[b], LV_STATE_DISABLED);
        set_color(d_btn_lbl_[b], pos ? col::INK : col::MUTED);
    }

    if (status_num_ == detail_num_ && !status_text_.empty()) {
        set_text(d_status_, status_text_);
        set_color(d_status_, status_color_);
    } else {
        set_text(d_status_, "");
    }
}

void TeamScreen::set_note(const std::string &text, uint32_t color)
{
    note_ = text;
    note_color_ = color;
    if (note_timer_)
        lv_timer_delete(note_timer_);
    // la nota se va sola a los 45 s y vuelve el resumen
    note_timer_ = lv_timer_create(
        [](lv_timer_t *t) {
            auto *s = static_cast<TeamScreen *>(lv_timer_get_user_data(t));
            s->note_.clear();
            s->note_timer_ = nullptr;
            s->rebuild_list();
        },
        45000, this);
    lv_timer_set_repeat_count(note_timer_, 1);
    rebuild_list();
}

void TeamScreen::set_status(uint32_t num, const std::string &text, uint32_t color)
{
    status_num_ = num;
    status_text_ = text;
    status_color_ = color;
    if (detail_num_ == num)
        refresh_detail();
}

void TeamScreen::release(std::vector<lv_group_t *> &groups)
{
    for (lv_timer_t **t : {&note_timer_, &ask_timer_})
        if (*t) {
            lv_timer_delete(*t);
            *t = nullptr;
        }
    groups.push_back(group);
    groups.push_back(detail_group_);
}

} // namespace tfield
