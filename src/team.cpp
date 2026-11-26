// Pantalla de equipo: lista de nodos (mi equipo / toda la red) y ficha de cada nodo.
#include "tfield/app.h"
#include "tfield/i18n.h"
#include "tfield/format.h"
#include "tfield/ui.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <tuple>

namespace tfield {

using namespace ui;

// Equipo "Fichas": una ficha por compañero, de a dos por fila; con más de 4, la lista se
// desliza hacia abajo y un indicador dice cuáles se ven ("5–8 de 12").
//
//   0 ┌ Esc 2 👥 Equipo              [Pedir posiciones] ┐  franja morada
//  22 ├ (Mi equipo 4) (Toda la red 6)   3 de 4 en contacto ┤
//  46 ├ ┌ GEO2 Geología 2  ahora ┐ ┌ VEH1 Camioneta 38 min ┐ ┤
//     │ │ 640 m            ↗ NE  │ │ 1,1 km            ← O  │ │
//     │ │ desnivel −4 m          │ │ desnivel +80 m         │ │
//     │ │ ▬▬▬▬▬▬▬▬▬▬▬▬     64%  │ │ ▬▬           18%       │ │
//     │ │ ▂▄▆█ directo           │ │ ▂ 1 salto              │ │
//     │ └────────────────────────┘ └────────────────────────┘ │
static constexpr int32_t BAND_H = 22;
static constexpr int32_t PILLS_Y = BAND_H, PILLS_H = 24;
static constexpr int32_t GRID_Y = PILLS_Y + PILLS_H;
static constexpr int32_t TILE_W = 156, TILE_H = 93, TILE_GAP = 3;

enum DetailButton { B_MSG, B_MAP, B_GOTO, B_POS, B_ROUTE, B_TEAM };

} // namespace tfield

// pictogramas (fonts/tfield_pictos.c, C): fuera del namespace
LV_IMAGE_DECLARE(tfield_picto_users);
LV_IMAGE_DECLARE(tfield_picto_arrow);
LV_IMAGE_DECLARE(tfield_picto_arrow_big);
LV_IMAGE_DECLARE(tfield_picto_mail16);
LV_IMAGE_DECLARE(tfield_picto_map16);
LV_IMAGE_DECLARE(tfield_picto_nav);
LV_IMAGE_DECLARE(tfield_picto_locate);
LV_IMAGE_DECLARE(tfield_picto_route);
LV_IMAGE_DECLARE(tfield_picto_user_plus);
LV_IMAGE_DECLARE(tfield_picto_user_minus);

namespace tfield {

static lv_obj_t *picto(lv_obj_t *parent, const lv_image_dsc_t *src, uint32_t color)
{
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, src);
    lv_obj_set_style_image_recolor(img, c(color), 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
}

// Con 250 nodos llega telemetría seguido: cambiar un estilo (aunque quede igual) redibuja,
// y en la T-Deck cada redibujo es SPI y batería. Estos solo tocan lo que cambió.
static void set_w(lv_obj_t *o, int32_t w)
{
    if (lv_obj_get_style_width(o, LV_PART_MAIN) != w)
        lv_obj_set_width(o, w);
}

static void set_recolor(lv_obj_t *img, uint32_t color)
{
    lv_color_t want = c(color), now = lv_obj_get_style_image_recolor(img, LV_PART_MAIN);
    if (now.red != want.red || now.green != want.green || now.blue != want.blue)
        lv_obj_set_style_image_recolor(img, want, 0);
}

// texto en negrita; si no cabe en "w", en la fuente normal (que es más angosta)
static void set_fit(lv_obj_t *l, const std::string &txt, const lv_font_t *big, const lv_font_t *small, int32_t w)
{
    lv_point_t sz;
    lv_text_get_size(&sz, txt.c_str(), big, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    set_font(l, sz.x <= w ? big : small);
    set_text(l, txt);
}

// rumbo: la flecha (hacia arriba en el dibujo) girada; solo si cambia (girar redibuja)
static void set_bearing(lv_obj_t *img, int deg)
{
    int32_t r = (int32_t)(((deg % 360) + 360) % 360) * 10;
    if (lv_image_get_rotation(img) != r)
        lv_image_set_rotation(img, r);
}

// "640 m" / "1,1 km": el número grande y la unidad chica (la fuente grande solo trae cifras)
static void split_distance(const std::string &d, std::string &num, std::string &unit)
{
    size_t sp = d.rfind(' ');
    num = sp == std::string::npos ? d : d.substr(0, sp);
    unit = sp == std::string::npos ? "" : d.substr(sp);
}

static lv_obj_t *band_esc(lv_obj_t *band, App *app, lv_group_t *g, uint32_t press)
{
    lv_obj_t *esc = button(band, 4, 3, 28, 16, press, col::SURF, g);
    lv_obj_set_style_bg_opa(esc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(esc, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(esc, c(press), LV_STATE_PRESSED);
    lv_obj_set_style_radius(esc, 2, 0);
    lv_obj_set_style_outline_color(esc, c(col::SURF), LV_STATE_FOCUS_KEY);
    lv_obj_center(label(esc, &tfield_font_12b, col::SURF, "Esc"));
    lv_obj_add_event_cb(
        esc, [](lv_event_t *e) { static_cast<App *>(lv_event_get_user_data(e))->back(); }, LV_EVENT_CLICKED, app);
    return esc;
}

void TeamScreen::create(App *app)
{
    app_ = app;
    screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen);
    set_bg(screen, col::BG);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    group = lv_group_create();

    // --- Franja morada: Esc, 2 Equipo, Pedir posiciones
    lv_obj_t *h = box(screen, 0, 0, SCREEN_W, BAND_H, col::PURPLE);
    band_esc(h, app, group, 0x2A2070);
    lv_obj_t *sq = box(h, 37, 4, 14, 14, col::SURF);
    lv_obj_set_style_radius(sq, 2, 0);
    lv_obj_center(label(sq, &tfield_font_12b, col::PURPLE, "2"));
    lv_obj_set_pos(picto(h, &tfield_picto_users, col::SURF), 55, 4);
    lv_obj_set_pos(label(h, &tfield_font_14b, col::SURF, tr("Equipo")), 73, 3);
    // pedir la posición a todo el equipo de una vez
    ask_all_ = button(h, SCREEN_W - 118, 3, 114, 16, col::PURPLE, col::SURF, nullptr);
    lv_obj_set_style_radius(ask_all_, 3, 0);
    lv_obj_set_style_bg_color(ask_all_, c(0x2A2070), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(ask_all_, c(col::MUTED), LV_STATE_DISABLED);
    lv_obj_set_style_outline_color(ask_all_, c(col::SURF), LV_STATE_FOCUS_KEY);
    ask_all_lbl_ = label(ask_all_, &tfield_font_12b, col::SURF, tr("Pedir posiciones"));
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
                        set_color(s->ask_all_lbl_, col::SURF);
                    },
                    60000, s);
                lv_timer_set_repeat_count(s->ask_timer_, 1);
            }
        },
        LV_EVENT_CLICKED, this);

    // --- Pestañas: Mi equipo | Toda la red, y el resumen
    lv_obj_t *pr = box(screen, 0, PILLS_Y, SCREEN_W, PILLS_H, col::BG);
    border(pr, col::LINE, 1, LV_BORDER_SIDE_BOTTOM);
    for (int i = 0; i < 2; i++) {
        seg_[i] = button(pr, 4 + i * 98, 3, 94, 18, col::SURF, col::LINE, group);
        lv_obj_set_style_radius(seg_[i], 9, 0);
        seg_lbl_[i] = label(seg_[i], &tfield_font_12b, col::INK, "");
        lv_obj_center(seg_lbl_[i]);
        lv_obj_set_user_data(seg_[i], (void *)(intptr_t)i);
        lv_obj_add_event_cb(
            seg_[i], [](lv_event_t *e) {
                auto *s = static_cast<TeamScreen *>(lv_event_get_user_data(e));
                s->set_filter((intptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e)) == 1);
            },
            LV_EVENT_CLICKED, this);
    }
    lv_group_add_obj(group, ask_all_); // orden: Esc, pestañas, Pedir posiciones, fichas
    summary_ = label(pr, &tfield_font_12, col::INK2, "");
    lv_obj_set_pos(summary_, 200, 5);
    lv_obj_set_size(summary_, SCREEN_W - 204, 14);
    lv_obj_set_style_text_align(summary_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(summary_, LV_LABEL_LONG_MODE_DOTS);

    // aviso temporal (pedido de posiciones…): franja bajo las pestañas, a todo el ancho
    note_bar_ = label(screen, &tfield_font_12b, col::INK, "");
    lv_obj_set_pos(note_bar_, 0, GRID_Y);
    lv_obj_set_size(note_bar_, SCREEN_W, 18);
    set_bg(note_bar_, col::SURF);
    lv_obj_set_style_bg_opa(note_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(note_bar_, 6, 0);
    lv_obj_set_style_pad_ver(note_bar_, 2, 0);
    border(note_bar_, col::LINE, 1, LV_BORDER_SIDE_BOTTOM);
    lv_label_set_long_mode(note_bar_, LV_LABEL_LONG_MODE_DOTS);
    show(note_bar_, false);

    // --- Fichas, de a dos; se desliza hacia abajo
    list_ = box(screen, 0, GRID_Y, SCREEN_W, SCREEN_H - GRID_Y, col::BG);
    lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list_, LV_DIR_VER);
    lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(list_, TILE_GAP, 0);
    lv_obj_set_style_pad_column(list_, SCREEN_W - 2 * TILE_GAP - 2 * TILE_W, 0);
    lv_obj_set_style_pad_row(list_, TILE_GAP, 0);
    lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_add_event_cb(
        list_, [](lv_event_t *e) { static_cast<TeamScreen *>(lv_event_get_user_data(e))->update_pager(); },
        LV_EVENT_SCROLL, this);
    // cuáles se ven: "5–8 de 12"
    pager_ = label(screen, &tfield_font_12b, col::SURF, "");
    set_bg(pager_, col::INK);
    lv_obj_set_style_bg_opa(pager_, LV_OPA_80, 0);
    lv_obj_set_style_pad_hor(pager_, 6, 0);
    lv_obj_set_style_pad_ver(pager_, 1, 0);
    lv_obj_set_style_radius(pager_, 8, 0);
    show(pager_, false);

    // --- Ficha (encima de todo)
    detail_group_ = lv_group_create();
    detail_ = box(screen, 0, 0, SCREEN_W, SCREEN_H, col::BG);
    d_band_ = box(detail_, 0, 0, SCREEN_W, BAND_H, col::PURPLE);
    d_esc_ = band_esc(d_band_, app, detail_group_, 0x2A2070);
    d_name_ = label(d_band_, &tfield_font_14b, col::SURF, "");
    lv_obj_set_pos(d_name_, 38, 3);
    lv_obj_set_size(d_name_, 180, 16);
    lv_label_set_long_mode(d_name_, LV_LABEL_LONG_MODE_DOTS);
    d_role_ = label(d_band_, &tfield_font_12, col::SURF, "");
    lv_obj_set_size(d_role_, 96, 14);
    lv_obj_set_style_text_align(d_role_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(d_role_, LV_LABEL_LONG_MODE_DOTS);
    ui::align(d_role_, LV_ALIGN_TOP_RIGHT, -6, 4);

    // distancia y rumbo, en grande
    d_big_ = box(detail_, 3, 25, 104, 72, col::SURF);
    lv_obj_set_style_radius(d_big_, 4, 0);
    border(d_big_, col::PURPLE, 2);
    d_arrow_ = picto(d_big_, &tfield_picto_arrow_big, col::PURPLE);
    ui::align(d_arrow_, LV_ALIGN_TOP_MID, 0, 1);
    d_dist_ = label(d_big_, &tfield_font_24b, col::INK, "");
    d_dist_unit_ = label(d_big_, &tfield_font_14b, col::INK, "");
    d_bear_ = label(d_big_, &tfield_font_12b, col::INK, "");
    // cuatro recuadros: etiqueta chica (con un dato extra) y valor
    static const int32_t TX[4] = {110, 215, 110, 215}, TY[4] = {25, 25, 62, 62};
    for (int i = 0; i < 4; i++) {
        d_tile_[i] = box(detail_, TX[i], TY[i], 102, 35, col::SURF);
        lv_obj_set_style_radius(d_tile_[i], 4, 0);
        border(d_tile_[i], col::LINE, 1);
        d_tlbl_[i] = label(d_tile_[i], &tfield_font_12, col::INK2, "");
        lv_obj_set_pos(d_tlbl_[i], 6, 2);
        lv_obj_set_size(d_tlbl_[i], 92, 14);
        lv_label_set_long_mode(d_tlbl_[i], LV_LABEL_LONG_MODE_DOTS);
        d_tval_[i] = label(d_tile_[i], &tfield_font_14b, col::INK, "");
        lv_obj_set_pos(d_tval_[i], 6, 15);
        lv_obj_set_size(d_tval_[i], 92, 17);
        lv_label_set_long_mode(d_tval_[i], LV_LABEL_LONG_MODE_DOTS);
    }
    // posición UTM y equipo
    lv_obj_t *pos = box(detail_, 3, 100, 314, 32, col::SURF);
    lv_obj_set_style_radius(pos, 4, 0);
    border(pos, col::LINE, 1);
    d_pos_lbl_ = label(pos, &tfield_font_12, col::INK2, "");
    lv_obj_set_pos(d_pos_lbl_, 6, 1);
    lv_obj_set_size(d_pos_lbl_, 220, 14);
    lv_label_set_long_mode(d_pos_lbl_, LV_LABEL_LONG_MODE_DOTS);
    d_hw_ = label(pos, &tfield_font_12, col::INK2, "");
    ui::align(d_hw_, LV_ALIGN_TOP_RIGHT, -6, 1);
    d_pos_ = label(pos, &tfield_font_14b, col::INK, "");
    lv_obj_set_pos(d_pos_, 6, 14);
    lv_obj_set_size(d_pos_, 300, 17);
    lv_label_set_long_mode(d_pos_, LV_LABEL_LONG_MODE_DOTS);
    // estado de lo pedido (posición, ruta de señal)
    d_status_ = label(detail_, &tfield_font_12b, col::INK2, "");
    lv_obj_set_pos(d_status_, 6, 134);
    lv_obj_set_size(d_status_, SCREEN_W - 12, 14);
    lv_label_set_long_mode(d_status_, LV_LABEL_LONG_MODE_DOTS);

    // acciones: botones grandes con pictograma
    static const char *btn_text[6] = {"Mensaje", "Ver en mapa", "Ir hacia", "Pedir posici\xC3\xB3n", "Ruta de se\xC3\xB1" "al", "Agregar a equipo"};
    const lv_image_dsc_t *btn_icon[6] = {&tfield_picto_mail16, &tfield_picto_map16, &tfield_picto_nav,
                                         &tfield_picto_locate, &tfield_picto_route, &tfield_picto_user_plus};
    for (int i = 0; i < 6; i++) {
        int32_t x = 3 + (i % 3) * 106, y = 150 + (i / 3) * 45;
        d_btn_[i] = button(detail_, x, y, 102, 42, col::SURF, col::INK, detail_group_);
        lv_obj_set_style_radius(d_btn_[i], 4, 0);
        lv_obj_set_style_bg_color(d_btn_[i], c(col::BG), LV_STATE_PRESSED);
        d_btn_img_[i] = picto(d_btn_[i], btn_icon[i], col::INK);
        ui::align(d_btn_img_[i], LV_ALIGN_TOP_MID, 0, 4);
        d_btn_lbl_[i] = label(d_btn_[i], &tfield_font_12b, col::INK, "");
        set_fit(d_btn_lbl_[i], tr(btn_text[i]), &tfield_font_12b, &tfield_font_12, 96);
        ui::align(d_btn_lbl_[i], LV_ALIGN_TOP_MID, 0, 22);
        lv_obj_set_user_data(d_btn_[i], (void *)(intptr_t)i);
        lv_obj_add_event_cb(
            d_btn_[i], [](lv_event_t *e) {
                auto *s = static_cast<TeamScreen *>(lv_event_get_user_data(e));
                int b = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e));
                uint32_t num = s->detail_num_;
                if (lv_obj_has_state(s->d_btn_[b], LV_STATE_DISABLED))
                    return;
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
    // las dos principales, llenas en el color de su pantalla
    for (auto [b, color, press] : {std::tuple<int, uint32_t, uint32_t>{B_MSG, col::BLUE, col::BLUE_DK},
                                   std::tuple<int, uint32_t, uint32_t>{B_MAP, col::GREEN, 0x1F4D24}}) {
        set_bg(d_btn_[b], color);
        set_border_color(d_btn_[b], color);
        lv_obj_set_style_bg_color(d_btn_[b], c(press), LV_STATE_PRESSED);
        set_color(d_btn_lbl_[b], col::SURF);
        lv_obj_set_style_image_recolor(d_btn_img_[b], c(col::SURF), 0);
    }
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
        set_border_color(seg_[i], on ? col::PURPLE : col::LINE);
        set_color(seg_lbl_[i], on ? col::SURF : col::INK);
    }
    rebuild_list();
    lv_obj_scroll_to_y(list_, 0, LV_ANIM_OFF);
    update_pager();
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
    else
        snprintf(buf, sizeof(buf), tr("%d de %d en contacto"), live, team);
    set_text(summary_, buf);
    // el aviso, en su franja; las fichas bajan mientras está
    bool noted = !note_.empty();
    if (noted) {
        set_text(note_bar_, note_);
        set_color(note_bar_, note_color_);
    }
    show(note_bar_, noted);
    int32_t gy = GRID_Y + (noted ? 18 : 0);
    if (lv_obj_get_y(list_) != gy) {
        lv_obj_set_y(list_, gy);
        lv_obj_set_height(list_, SCREEN_H - gy);
    }
    (void)last;
    show(ask_all_, team > 0);
    snprintf(buf, sizeof(buf), tr("Mi equipo %d"), team);
    set_text(seg_lbl_[0], buf);
    snprintf(buf, sizeof(buf), tr("Toda la red %d"), (int)nodes_.size());
    set_text(seg_lbl_[1], buf);

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
    // "Toda la red" con 250 nodos serían miles de objetos: los 50 ajenos más cercanos
    size_t team_n = (size_t)team, cap = team_n + TEAM_LIST_OTHERS, total = shown.size();
    if (shown.size() > cap)
        shown.resize(cap);

    while (rows_.size() < shown.size()) {
        Row r{};
        r.row = button(list_, 0, 0, TILE_W, TILE_H, col::SURF, col::INK, group);
        lv_obj_set_style_radius(r.row, 4, 0);
        lv_obj_set_style_clip_corner(r.row, false, 0);
        lv_obj_set_style_bg_color(r.row, c(col::BG), LV_STATE_PRESSED);
        // franja: nombre, nombre largo, hace cuánto
        r.band = box(r.row, 0, 0, TILE_W - 2, 17, col::PURPLE);
        r.name = label(r.band, &tfield_font_12b, col::SURF, "");
        lv_obj_set_pos(r.name, 4, 1);
        r.lname = label(r.band, &tfield_font_12, col::SURF, "");
        lv_label_set_long_mode(r.lname, LV_LABEL_LONG_MODE_CLIP);
        r.heard = label(r.band, &tfield_font_12b, col::SURF, "");
        // distancia (grande) y rumbo (flecha y punto cardinal)
        r.dist = label(r.row, &tfield_font_24b, col::INK, "");
        lv_obj_set_pos(r.dist, 5, 19);
        r.unit = label(r.row, &tfield_font_14b, col::INK, "");
        r.arrow = picto(r.row, &tfield_picto_arrow, col::PURPLE);
        lv_obj_set_pos(r.arrow, TILE_W - 2 - 30, 18);
        r.card = label(r.row, &tfield_font_12b, col::INK, "");
        // desnivel, batería, señal
        r.alt = label(r.row, &tfield_font_12, col::INK, "");
        lv_obj_set_pos(r.alt, 5, 45);
        r.bat = box(r.row, 5, 63, 104, 9, col::LINE2);
        lv_obj_set_style_radius(r.bat, 2, 0);
        r.bat_fill = box(r.bat, 0, 0, 0, 9, col::INK);
        lv_obj_set_style_radius(r.bat_fill, 2, 0);
        r.pct = label(r.row, &tfield_font_12b, col::INK, "");
        r.bars = signal_bars(r.row, 5, 77);
        r.hops = label(r.row, &tfield_font_12, col::INK2, "");
        lv_obj_set_pos(r.hops, 20, 74);
        lv_obj_add_event_cb(
            r.row, [](lv_event_t *e) {
                auto *s = static_cast<TeamScreen *>(lv_event_get_user_data(e));
                lv_obj_t *row = (lv_obj_t *)lv_event_get_current_target(e);
                for (size_t i = 0; i < s->rows_.size(); i++)
                    if (s->rows_[i].row == row && i < s->row_nums_.size())
                        s->app_->show_node(s->row_nums_[i]); // queda en el historial: Esc vuelve a la lista
            },
            LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(
            r.row, [](lv_event_t *e) { static_cast<TeamScreen *>(lv_event_get_user_data(e))->update_pager(); },
            LV_EVENT_FOCUSED, this);
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
        // color: el del compañero; sin noticias, ámbar; fuera del equipo, gris
        uint32_t color = !n.in_team ? col::MUTED : (n.stale ? col::AMBER : n.color);
        set_bg(r.row, n.stale ? col::AMBER_BG : col::SURF);
        set_border_color(r.row, color);
        set_bg(r.band, color);
        set_text(r.name, n.name);
        set_text(r.heard, format_ago(n.heard_ago_s));
        ui::align(r.heard, LV_ALIGN_TOP_RIGHT, -4, 1);
        lv_obj_update_layout(r.band);
        // nombre largo entre el nombre y "hace cuánto", cortado si no cabe
        set_text(r.lname, n.long_name); // fuera del equipo: lo dice el gris
        int32_t lx = lv_obj_get_x(r.name) + lv_obj_get_width(r.name) + 4;
        lv_obj_set_pos(r.lname, lx, 1);
        int32_t room = lv_obj_get_x(r.heard) - lx - 4;
        set_w(r.lname, room > 0 ? room : 0);
        if (n.dist_m >= 0) {
            std::string num, unit;
            split_distance(format_distance(n.dist_m), num, unit);
            set_font(r.dist, &tfield_font_24b);
            set_text(r.dist, num);
            set_text(r.unit, unit);
            ui::align_to(r.unit, r.dist, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, -3);
            set_bearing(r.arrow, n.bearing_deg);
            set_recolor(r.arrow, color);
            show(r.arrow, true);
            set_text(r.card, cardinal(n.bearing_deg));
            lv_obj_update_layout(r.card);
            lv_obj_set_pos(r.card, TILE_W - 2 - 20 - lv_obj_get_width(r.card) / 2, 38);
            show(r.card, true);
            set_text(r.alt, n.has_alt ? tr("desnivel ") + format_alt_diff(n.alt_diff_m) : "");
        } else {
            set_font(r.dist, &tfield_font_14b);
            set_text(r.dist, tr("sin posici\xC3\xB3n"));
            set_text(r.unit, "");
            show(r.arrow, false);
            show(r.card, false);
            set_text(r.alt, "");
        }
        // batería: barra (roja bajo 20 %) y porcentaje
        int pct = n.battery > 100 ? 100 : n.battery;
        set_bg(r.bat_fill, pct >= 0 && pct < 20 ? col::RED : col::INK);
        set_w(r.bat_fill, pct < 0 ? 0 : 104 * pct / 100);
        char pb[12];
        if (n.battery > 100)
            snprintf(pb, sizeof(pb), "USB");
        else if (n.battery >= 0)
            snprintf(pb, sizeof(pb), "%d%%", n.battery);
        else
            snprintf(pb, sizeof(pb), "--");
        set_text(r.pct, pb);
        set_color(r.pct, pct >= 0 && pct < 20 ? col::RED : col::INK);
        ui::align(r.pct, LV_ALIGN_TOP_RIGHT, -5, 60);
        signal_set(r.bars, n.stale ? 0 : n.signal, col::INK, n.stale ? 0xE3D3B5 : col::LINE);
        set_text(r.hops, format_hops(n.hops));
    }
    // aviso al final cuando la lista se acorta
    if (!more_) {
        more_ = label(list_, &tfield_font_12, col::INK2, "");
        lv_obj_set_width(more_, SCREEN_W - 2 * TILE_GAP);
        lv_obj_set_style_pad_all(more_, 4, 0);
        lv_label_set_long_mode(more_, LV_LABEL_LONG_MODE_WRAP);
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
    update_pager();
}

// "5–8 de 12": qué fichas se ven (solo si hay más de las que caben)
void TeamScreen::update_pager()
{
    size_t n = rows_.size();
    if (n <= 4 || lv_obj_has_flag(list_, LV_OBJ_FLAG_HIDDEN)) {
        show(pager_, false);
        return;
    }
    int32_t sy = lv_obj_get_scroll_y(list_);
    int32_t row = (sy + (TILE_H + TILE_GAP) / 2) / (TILE_H + TILE_GAP);
    size_t first = std::min(n, (size_t)std::max(0, row) * 2 + 1);
    size_t last = std::min(n, first + 3);
    char buf[32];
    snprintf(buf, sizeof(buf), tr("%u\xE2\x80\x93%u de %u"), (unsigned)first, (unsigned)last, (unsigned)n);
    set_text(pager_, buf);
    show(pager_, true);
    ui::align(pager_, LV_ALIGN_BOTTOM_RIGHT, -8, -5);
    lv_obj_move_foreground(pager_);
}

void TeamScreen::show_list()
{
    if (dirty_)
        rebuild_list();
    detail_num_ = 0;
    show(detail_, false);
    update_pager();
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
    show(pager_, false);
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
    uint32_t color = !n->in_team ? col::MUTED : (n->stale ? col::AMBER : n->color);
    set_bg(d_band_, color);
    set_text(d_name_, n->long_name.empty() ? n->name : n->name + " \xC2\xB7 " + n->long_name);
    // papel (cliente, repetidor…: vocabulario cerrado del adaptador) y hace cuánto
    set_text(d_role_, tr(n->role.c_str()));
    set_border_color(d_big_, color);

    if (n->dist_m >= 0) {
        std::string num, unit;
        split_distance(format_distance(n->dist_m), num, unit);
        set_font(d_dist_, &tfield_font_24b);
        set_text(d_dist_, num);
        set_text(d_dist_unit_, unit);
        lv_obj_update_layout(d_dist_);
        lv_obj_update_layout(d_dist_unit_);
        int32_t w = lv_obj_get_width(d_dist_) + lv_obj_get_width(d_dist_unit_);
        lv_obj_set_pos(d_dist_, (104 - 4 - w) / 2, 31);
        ui::align_to(d_dist_unit_, d_dist_, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, -3);
        snprintf(buf, sizeof(buf), "%s %d\xC2\xB0", cardinal(n->bearing_deg), n->bearing_deg);
        set_text(d_bear_, buf);
        ui::align(d_bear_, LV_ALIGN_TOP_MID, 0, 52);
        set_bearing(d_arrow_, n->bearing_deg);
        set_recolor(d_arrow_, color);
        show(d_arrow_, true);
        set_text(d_pos_, format_utm(to_utm(n->lat, n->lon)));
        set_text(d_pos_lbl_, tr("posici\xC3\xB3n UTM \xC2\xB7 ") + ago_text(n->pos_age_s) +
                                 (n->acc_m ? " \xC2\xB7 \xC2\xB1" + std::to_string(n->acc_m) + tr(" m") : ""));
    } else {
        set_font(d_dist_, &tfield_font_14b);
        set_text(d_dist_, tr("sin posici\xC3\xB3n"));
        set_text(d_dist_unit_, "");
        ui::align(d_dist_, LV_ALIGN_CENTER, 0, 0);
        set_text(d_bear_, "");
        show(d_arrow_, false);
        set_text(d_pos_, tr("Este nodo no ha enviado posici\xC3\xB3n"));
        set_text(d_pos_lbl_, tr("posici\xC3\xB3n UTM"));
    }
    set_text(d_hw_, n->hw);
    ui::align(d_hw_, LV_ALIGN_TOP_RIGHT, -6, 1);

    // desnivel
    set_text(d_tlbl_[0], tr("desnivel"));
    set_text(d_tval_[0], n->has_alt && n->dist_m >= 0 ? format_alt_diff(n->alt_diff_m) : "--");
    // batería: % grande, voltaje chico; fondo según cuánto queda
    auto comma = [](std::string s) {
        if (!english())
            for (auto &ch : s)
                if (ch == '.')
                    ch = ',';
        return s;
    };
    std::string bl = tr("bater\xC3\xAD" "a");
    if (n->voltage > 0) {
        snprintf(buf, sizeof(buf), " \xC2\xB7 %.2f V", n->voltage);
        bl += comma(buf);
    }
    set_text(d_tlbl_[1], bl);
    if (n->battery > 100)
        set_text(d_tval_[1], "USB");
    else if (n->battery >= 0) {
        snprintf(buf, sizeof(buf), "%d %%", n->battery);
        set_text(d_tval_[1], buf);
    } else
        set_text(d_tval_[1], "--");
    uint32_t bbg = n->battery < 0 ? col::SURF : n->battery < 20 ? 0xFCEBEB : n->battery < 40 ? col::AMBER_BG : 0xEAF3DE;
    set_bg(d_tile_[1], bbg);
    set_color(d_tval_[1], n->battery >= 0 && n->battery < 20 ? col::RED : col::INK);
    // señal: SNR grande, RSSI chico
    snprintf(buf, sizeof(buf), "%s%d dBm", n->rssi < 0 ? "\xE2\x88\x92" : "", std::abs(n->rssi));
    set_text(d_tlbl_[2], tr("se\xC3\xB1" "al \xC2\xB7 ") + std::string(buf));
    snprintf(buf, sizeof(buf), "%.1f dB", n->snr);
    set_text(d_tval_[2], n->stale ? tr("sin se\xC3\xB1" "al reciente") : comma(buf));
    // escuchado: hace cuánto y por dónde
    set_text(d_tlbl_[3], tr("escuchado"));
    std::string heard = format_ago(n->heard_ago_s);
    if (n->hops >= 0)
        heard += " \xC2\xB7 " + format_hops(n->hops);
    set_fit(d_tval_[3], heard, &tfield_font_14b, &tfield_font_12b, 92);
    set_color(d_tval_[3], n->stale ? col::AMBER : (n->heard_ago_s < 60 ? col::GREEN : col::INK));
    set_bg(d_tile_[3], n->stale ? col::AMBER_BG : col::SURF);

    // equipo: agregar (lleno, morado) o quitar (contorno)
    set_bg(d_btn_[B_TEAM], n->in_team ? col::SURF : col::PURPLE);
    set_border_color(d_btn_[B_TEAM], col::PURPLE);
    set_color(d_btn_lbl_[B_TEAM], n->in_team ? col::PURPLE : col::SURF);
    const void *team_icon = n->in_team ? &tfield_picto_user_minus : &tfield_picto_user_plus;
    if (lv_image_get_src(d_btn_img_[B_TEAM]) != team_icon)
        lv_image_set_src(d_btn_img_[B_TEAM], team_icon);
    set_recolor(d_btn_img_[B_TEAM], n->in_team ? col::PURPLE : col::SURF);
    set_fit(d_btn_lbl_[B_TEAM], n->in_team ? tr("Quitar del equipo") : tr("Agregar a equipo"), &tfield_font_12b,
            &tfield_font_12, 96);
    ui::align(d_btn_lbl_[B_TEAM], LV_ALIGN_TOP_MID, 0, 22);
    // sin posición: Ver en mapa e Ir hacia, apagados
    bool pos = n->dist_m >= 0;
    for (int b : {B_GOTO, B_MAP}) {
        if (pos)
            lv_obj_remove_state(d_btn_[b], LV_STATE_DISABLED);
        else
            lv_obj_add_state(d_btn_[b], LV_STATE_DISABLED);
    }
    set_bg(d_btn_[B_MAP], pos ? col::GREEN : col::SURF);
    set_border_color(d_btn_[B_MAP], pos ? col::GREEN : col::LINE);
    set_color(d_btn_lbl_[B_MAP], pos ? col::SURF : col::MUTED);
    set_recolor(d_btn_img_[B_MAP], pos ? col::SURF : col::MUTED);
    set_border_color(d_btn_[B_GOTO], pos ? col::INK : col::LINE);
    set_color(d_btn_lbl_[B_GOTO], pos ? col::INK : col::MUTED);
    set_recolor(d_btn_img_[B_GOTO], pos ? col::INK : col::MUTED);

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
