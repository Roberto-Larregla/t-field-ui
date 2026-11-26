// Pantalla de mapa.
#include "tfield/map.h"
#include "tfield/i18n.h"
#include "tfield/app.h"
#include "tfield/format.h"
#include "tfield/gpx.h"
#include "tfield/ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

// pictogramas (fonts/tfield_pictos.c, C): fuera del namespace
LV_IMAGE_DECLARE(tfield_picto_layers);
LV_IMAGE_DECLARE(tfield_picto_ruler);
LV_IMAGE_DECLARE(tfield_picto_eye);
LV_IMAGE_DECLARE(tfield_picto_tag);

namespace tfield {

using namespace ui;

static constexpr int32_t HEAD_H = 28;
static constexpr int32_t MAP_Y = HEAD_H;
static constexpr int32_t MAP_W = SCREEN_W;
static constexpr int32_t MAP_H = 182;
static constexpr size_t RULER_MAX = 30; // puntos de la regla
static constexpr int32_t TOOL_Y = MAP_Y + MAP_H;
static constexpr int32_t CTRL_X = MAP_W - 30; // columna de botones a la derecha
static constexpr uint32_t WP_COLOR = 0xA12C6B;    // puntos
static constexpr uint32_t TRACK_COLOR = col::RED; // ruta en curso
static constexpr uint32_t ROUTE_COLOR = 0x6B3FA0; // ruta guardada mostrada
static constexpr uint32_t PAPER = 0xF4F2EC;       // fondo sin mapa base

// ------------------------------------------------------------- piezas
static lv_obj_t *pill(lv_obj_t *parent, const lv_font_t *font, uint32_t fg, uint32_t bg, lv_opa_t opa)
{
    lv_obj_t *l = label(parent, font, fg, "");
    set_bg(l, bg);
    lv_obj_set_style_bg_opa(l, opa, 0);
    lv_obj_set_style_pad_hor(l, 3, 0);
    lv_obj_set_style_pad_ver(l, 1, 0);
    lv_obj_set_style_radius(l, 3, 0);
    return l;
}

static lv_obj_t *tool_button(lv_obj_t *parent, int i, const char *l1, const char *l2, lv_obj_t **out_l1,
                             lv_obj_t **out_l2, lv_group_t *g)
{
    lv_obj_t *b = button(parent, i * 80, 1, 80, SCREEN_H - TOOL_Y - 1, col::SURF, 0, g);
    lv_obj_set_style_radius(b, 0, 0);
    lv_obj_set_style_bg_color(b, c(col::BG), LV_STATE_PRESSED);
    if (i < 3) {
        set_border_color(b, col::LINE2);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_border_side(b, LV_BORDER_SIDE_RIGHT, 0);
    }
    lv_obj_t *a = label(b, &tfield_font_12b, col::INK, l1);
    ui::align(a, LV_ALIGN_TOP_MID, 0, 1);
    lv_obj_t *z = label(b, &tfield_font_12, col::INK2, l2);
    ui::align(z, LV_ALIGN_TOP_MID, 0, 14);
    if (out_l1)
        *out_l1 = a;
    if (out_l2)
        *out_l2 = z;
    return b;
}

static lv_obj_t *map_button(lv_obj_t *view, int32_t y, lv_group_t *g)
{
    lv_obj_t *b = button(view, CTRL_X + 1, y, 25, 25, col::SURF, col::INK, g); // también con la bola
    lv_obj_set_style_bg_color(b, c(col::BG), LV_STATE_PRESSED);
    return b;
}

static std::string duration_text(uint32_t s)
{
    char buf[16];
    snprintf(buf, sizeof(buf), tr("%u h %02u"), s / 3600, (s / 60) % 60);
    return buf;
}

// ------------------------------------------------------------- creación
void MapScreen::create(App *app)
{
    app_ = app;
    screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen);
    set_bg(screen, col::SURF);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    group = lv_group_create();

    // --- Encabezado: Esc · Mapa · [capa]
    lv_obj_t *h = header_bar(screen, app, group, tr("Mapa"));
    lv_obj_t *lb = button(h, 0, 5, 150, 18, col::SURF, col::GREEN, group);
    ui::align(lb, LV_ALIGN_TOP_RIGHT, -6, 5);
    lv_obj_set_style_radius(lb, 2, 0);
    lv_obj_set_style_bg_color(lb, c(col::BG), LV_STATE_PRESSED);
    layer_btn_lbl_ = label(lb, &tfield_font_12b, col::GREEN, "");
    lv_obj_set_size(layer_btn_lbl_, 140, 14);
    lv_label_set_long_mode(layer_btn_lbl_, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(layer_btn_lbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(layer_btn_lbl_);
    lv_obj_add_event_cb(
        lb, [](lv_event_t *e) {
            auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
            s->show_panel(s->panel_kind_ == PANEL_LAYERS ? PANEL_NONE : PANEL_LAYERS);
        },
        LV_EVENT_CLICKED, this);

    // --- Área del mapa
    view = box(screen, 0, MAP_Y, MAP_W, MAP_H, PAPER);
    lv_obj_add_flag(view, LV_OBJ_FLAG_CLICKABLE);
    focus_style(view);
    lv_obj_add_event_cb(view, on_view, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(view, on_key, LV_EVENT_KEY, this);
    lv_obj_add_event_cb(
        view, [](lv_event_t *e) { // al salir del mapa con el foco, se suelta
            auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
            if (s->pan_mode_)
                s->set_pan_mode(false);
        },
        LV_EVENT_DEFOCUSED, this);
    lv_group_add_obj(group, view);

    for (auto &t : tiles_) {
        t = lv_image_create(view);
        show(t, false);
        lv_obj_remove_flag(t, LV_OBJ_FLAG_CLICKABLE);
        if (g_dark) { // de noche el mapa base no encandila: se oscurece (algo más de CPU al dibujar)
            lv_obj_set_style_image_recolor(t, lv_color_hex(0x0F151A), 0);
            lv_obj_set_style_image_recolor_opa(t, LV_OPA_50, 0);
        }
    }
    grid_pts_.resize(32);
    for (int i = 0; i < 16; i++) {
        grid_[i] = lv_line_create(view);
        lv_obj_set_style_line_color(grid_[i], c(0xD8D3C4), 0);
        lv_obj_set_style_line_width(grid_[i], 1, 0);
        show(grid_[i], false);
    }
    init_poly(route_poly_);
    lv_style_set_line_color(&route_poly_.style, c(ROUTE_COLOR));
    lv_style_set_line_width(&route_poly_.style, 3);
    lv_style_set_line_rounded(&route_poly_.style, true);

    init_poly(trail_poly_);
    lv_style_set_line_width(&trail_poly_.style, 2);
    lv_style_set_line_dash_width(&trail_poly_.style, 4);
    lv_style_set_line_dash_gap(&trail_poly_.style, 3);

    init_poly(track_poly_);
    lv_style_set_line_color(&track_poly_.style, c(TRACK_COLOR));
    lv_style_set_line_width(&track_poly_.style, 3);
    lv_style_set_line_rounded(&track_poly_.style, true);


    // Regla: una capa con los tramos marcados, el tramo hasta la cruz (punteado), los puntos y
    // el total junto a la cruz. No recibe toques: pasan al mapa.
    ruler_layer_ = lv_obj_create(view);
    lv_obj_remove_style_all(ruler_layer_);
    lv_obj_set_size(ruler_layer_, MAP_W, MAP_H);
    lv_obj_remove_flag(ruler_layer_, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE));
    ruler_line_ = lv_line_create(ruler_layer_);
    lv_obj_set_style_line_color(ruler_line_, c(col::INK), 0);
    lv_obj_set_style_line_width(ruler_line_, 2, 0);
    show(ruler_line_, false);
    ruler_live_ = lv_line_create(ruler_layer_);
    lv_obj_set_style_line_color(ruler_live_, c(col::BLUE), 0); // el tramo que falta marcar
    lv_obj_set_style_line_width(ruler_live_, 2, 0);
    lv_obj_set_style_line_dash_width(ruler_live_, 5, 0);
    lv_obj_set_style_line_dash_gap(ruler_live_, 4, 0);
    show(ruler_live_, false);
    ruler_lbl_ = pill(ruler_layer_, &tfield_font_12b, col::SURF, col::INK, LV_OPA_COVER);
    show(ruler_lbl_, false);
    show(ruler_layer_, false);

    // Posición propia: halo de precisión + punto
    me_halo_ = box(view, 0, 0, 20, 20, col::BLUE);
    lv_obj_set_style_bg_opa(me_halo_, LV_OPA_20, 0);
    lv_obj_set_style_radius(me_halo_, LV_RADIUS_CIRCLE, 0);
    border(me_halo_, col::BLUE, 1);
    lv_obj_set_style_border_opa(me_halo_, LV_OPA_50, 0);
    me_dot_ = box(view, 0, 0, 12, 12, col::BLUE);
    lv_obj_set_style_radius(me_dot_, LV_RADIUS_CIRCLE, 0);
    border(me_dot_, col::SURF, 2);

    // Cruz del centro (mover el mapa, regla): dos barras con borde claro, se ven sobre cualquier fondo
    cross_ = lv_obj_create(view);
    lv_obj_remove_style_all(cross_);
    lv_obj_set_size(cross_, 21, 21);
    lv_obj_remove_flag(cross_, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    for (int v = 0; v < 2; v++) {
        lv_obj_t *bar = box(cross_, v ? 9 : 0, v ? 0 : 9, v ? 3 : 21, v ? 21 : 3, col::INK);
        lv_obj_set_style_outline_width(bar, 1, 0);
        lv_obj_set_style_outline_color(bar, c(col::SURF), 0);
        lv_obj_set_style_outline_opa(bar, LV_OPA_COVER, 0);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_move_foreground(lv_obj_get_child(cross_, 0)); // la barra horizontal sobre el borde de la vertical
    show(cross_, false);

    // Coordenadas, norte, escala, atribución
    coords_ = pill(view, &tfield_font_12, col::INK, col::SURF, LV_OPA_90);
    north_ = box(view, 3, MAP_H - 33, 16, 30, col::SURF);
    lv_obj_set_style_bg_opa(north_, LV_OPA_TRANSP, 0);
    lv_obj_t *nl = label(north_, &tfield_font_12b, col::INK, tr("N"));
    ui::align(nl, LV_ALIGN_TOP_MID, 0, -1);
    lv_obj_add_event_cb(
        north_, [](lv_event_t *e) {
            lv_obj_t *o = (lv_obj_t *)lv_event_get_current_target(e);
            lv_layer_t *layer = lv_event_get_layer(e);
            lv_area_t a;
            lv_obj_get_coords(o, &a);
            lv_value_precise_t cx = a.x1 + 8, top = a.y1 + 13, bot = a.y1 + 29, mid = a.y1 + 24;
            lv_draw_triangle_dsc_t t;
            lv_draw_triangle_dsc_init(&t);
            t.opa = LV_OPA_COVER;
            t.color = c(col::SURF); // halo para leerse sobre cualquier mapa
            t.p[0] = {cx, (lv_value_precise_t)(top - 2)};
            t.p[1] = {(lv_value_precise_t)(cx - 8), (lv_value_precise_t)(bot + 1)};
            t.p[2] = {(lv_value_precise_t)(cx + 8), (lv_value_precise_t)(bot + 1)};
            lv_draw_triangle(layer, &t);
            t.color = c(col::INK);
            t.p[0] = {cx, top};
            t.p[1] = {(lv_value_precise_t)(cx - 6), bot};
            t.p[2] = {cx, mid};
            lv_draw_triangle(layer, &t);
            t.color = c(col::MUTED);
            t.p[0] = {cx, top};
            t.p[1] = {(lv_value_precise_t)(cx + 6), bot};
            t.p[2] = {cx, mid};
            lv_draw_triangle(layer, &t);
        },
        LV_EVENT_DRAW_MAIN, nullptr);
    scale_bar_ = box(view, 24, MAP_H - 9, 60, 4, col::INK);
    border(scale_bar_, col::SURF, 1);
    scale_lbl_ = pill(view, &tfield_font_12b, col::INK, col::SURF, LV_OPA_80);
    attrib_ = pill(view, &tfield_font_12, col::INK2, col::SURF, LV_OPA_80);

    // --- Columna de botones: zoom, centrar, regla
    for (int i = 0; i < 2; i++) {
        lv_obj_t *z = map_button(view, 36 + i * 27, group);
        zoom_btn_[i] = z;
        lv_obj_center(label(z, &tfield_font_14b, col::INK, i == 0 ? "+" : "\xE2\x88\x92"));
        lv_obj_set_user_data(z, (void *)(intptr_t)(i == 0 ? 1 : -1));
        lv_obj_add_event_cb(
            z, [](lv_event_t *e) {
                auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
                s->zoom_by((int)(intptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e)));
            },
            LV_EVENT_CLICKED, this);
    }
    btn_center_ = map_button(view, 94, group);
    center_ring_ = box(btn_center_, 5, 5, 14, 14, col::SURF);
    lv_obj_set_style_radius(center_ring_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(center_ring_, LV_OPA_TRANSP, 0);
    border(center_ring_, col::INK, 2);
    center_dot_ = box(btn_center_, 10, 10, 4, 4, col::INK);
    lv_obj_set_style_radius(center_dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(
        btn_center_, [](lv_event_t *e) { static_cast<MapScreen *>(lv_event_get_user_data(e))->center_on_me(); },
        LV_EVENT_CLICKED, this);

    btn_ruler_ = map_button(view, 121, group);
    lv_obj_t *rl = lv_image_create(btn_ruler_);
    lv_image_set_src(rl, &tfield_picto_ruler);
    lv_obj_set_style_image_recolor_opa(rl, LV_OPA_COVER, 0);
    lv_obj_set_style_image_recolor(rl, c(col::INK), 0);
    lv_obj_remove_flag(rl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(rl);
    lv_obj_add_event_cb(
        btn_ruler_, [](lv_event_t *e) { static_cast<MapScreen *>(lv_event_get_user_data(e))->ruler_button(); },
        LV_EVENT_CLICKED, this);

    // Capas: qué se ve y qué lleva nombre (por capa y, dentro de una, uno por uno)
    btn_names_ = map_button(view, 148, group);
    names_img_ = lv_image_create(btn_names_);
    lv_image_set_src(names_img_, &tfield_picto_layers);
    lv_obj_set_style_image_recolor_opa(names_img_, LV_OPA_COVER, 0);
    lv_obj_set_style_image_recolor(names_img_, c(col::INK), 0);
    lv_obj_remove_flag(names_img_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(names_img_);
    lv_obj_add_event_cb(
        btn_names_, [](lv_event_t *e) {
            auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
            s->show_panel(s->panel_kind_ == PANEL_NAMES ? PANEL_NONE : PANEL_NAMES);
        },
        LV_EVENT_CLICKED, this);

    // Lo elegido: anillo alrededor y ficha con nombre, distancia y desnivel (tocarla abre)
    sel_ring_ = box(view, 0, 0, 22, 22, col::SURF);
    lv_obj_set_style_bg_opa(sel_ring_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(sel_ring_, LV_RADIUS_CIRCLE, 0);
    border(sel_ring_, col::INK, 3);
    show(sel_ring_, false);
    callout_ = button(view, 0, 0, 10, 10, col::SURF, col::INK, nullptr);
    lv_obj_set_size(callout_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(callout_, 2, 0);
    lv_obj_set_style_radius(callout_, 5, 0);
    lv_obj_set_style_pad_hor(callout_, 6, 0);
    lv_obj_set_style_pad_ver(callout_, 3, 0);
    lv_obj_set_style_bg_color(callout_, c(col::BG), LV_STATE_PRESSED);
    lv_obj_set_flex_flow(callout_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(callout_, 1, 0);
    callout_l1_ = label(callout_, &tfield_font_12b, col::INK, "");
    callout_l2_ = label(callout_, &tfield_font_12, col::INK, "");
    callout_l3_ = label(callout_, &tfield_font_12, col::INK2, "");
    lv_obj_add_event_cb(
        callout_, [](lv_event_t *e) { static_cast<MapScreen *>(lv_event_get_user_data(e))->open_selection(); },
        LV_EVENT_CLICKED, this);
    show(callout_, false);

    // Franja superior de la regla
    strip_ = box(view, 0, 0, MAP_W, 20, col::INK);
    strip_lbl_ = label(strip_, &tfield_font_12b, col::SURF, "");
    lv_obj_set_pos(strip_lbl_, 5, 3);
    lv_obj_set_size(strip_lbl_, MAP_W - 32, 14);
    lv_label_set_long_mode(strip_lbl_, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_t *nx = button(strip_, MAP_W - 22, 2, 18, 16, col::INK, col::SURF, nullptr);
    lv_obj_set_style_bg_opa(nx, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(nx, 2, 0);
    lv_obj_center(label(nx, &tfield_font_12b, col::SURF, "\xC3\x97"));
    lv_obj_add_event_cb(
        nx, [](lv_event_t *e) {
            static_cast<MapScreen *>(lv_event_get_user_data(e))->ruler_off();
        },
        LV_EVENT_CLICKED, this);
    show(strip_, false);

    toast_ = pill(view, &tfield_font_12b, col::SURF, col::INK, LV_OPA_COVER);
    lv_obj_set_style_max_width(toast_, MAP_W - 16, 0); // textos largos pasan a dos líneas
    lv_obj_set_style_text_align(toast_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_hor(toast_, 8, 0);
    lv_obj_set_style_pad_ver(toast_, 4, 0);
    show(toast_, false);

    // --- Panel inferior (Puntos / Rutas / Capas)
    panel_ = box(screen, 0, TOOL_Y - 142, SCREEN_W, 142, col::SURF);
    border(panel_, col::INK, 1, LV_BORDER_SIDE_TOP);
    lv_obj_t *ph = box(panel_, 0, 1, SCREEN_W, 20, col::BG);
    panel_title_ = label(ph, &tfield_font_12b, col::INK, "");
    lv_obj_set_pos(panel_title_, 6, 3);
    lv_obj_t *pc = button(ph, SCREEN_W - 66, 2, 62, 16, col::SURF, col::INK, nullptr);
    lv_obj_set_style_radius(pc, 2, 0);
    panel_close_lbl_ = label(pc, &tfield_font_12b, col::INK, tr("Cerrar"));
    lv_obj_center(panel_close_lbl_);
    lv_obj_add_event_cb(
        pc, [](lv_event_t *e) {
            auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
            if (s->panel_kind_ == PANEL_NAMES && s->layer_view_ >= 0) // dentro de una capa: a la lista
                s->open_layer_items(-1);
            else
                s->show_panel(PANEL_NONE);
        },
        LV_EVENT_CLICKED, this);
    panel_list_ = box(panel_, 0, 21, SCREEN_W, 121, col::SURF);
    lv_obj_add_flag(panel_list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(panel_list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(panel_list_, LV_SCROLLBAR_MODE_ACTIVE);
    show(panel_, false);

    // --- Barra de acciones
    lv_obj_t *tool = box(screen, 0, TOOL_Y, SCREEN_W, SCREEN_H - TOOL_Y, col::SURF);
    border(tool, col::INK, 1, LV_BORDER_SIDE_TOP);
    btn_rec_ = tool_button(tool, 0, tr("Grabar"), tr("ruta"), &rec_l1_, &rec_l2_, group);
    rec_dot_ = box(btn_rec_, 6, 4, 8, 8, TRACK_COLOR);
    lv_obj_set_style_radius(rec_dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(
        btn_rec_, [](lv_event_t *e) {
            auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
            if (s->app_->track().recording) {
                s->app_->stop_recording();
                s->open_dialog(DLG_ROUTE);
            } else if (s->app_->track().pts.size() >= 2) {
                s->open_dialog(DLG_ROUTE, 2); // hay una ruta sin guardar (p. ej. recuperada): primero decidir
            } else {
                s->app_->start_recording();
                s->toast(tr("Grabando ruta"));
            }
        },
        LV_EVENT_CLICKED, this);
    btn_mark_ = tool_button(tool, 1, tr("Marcar"), tr("punto aqu\xC3\xAD"), nullptr, nullptr, group);
    lv_obj_add_event_cb(
        btn_mark_, [](lv_event_t *e) {
            auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
            if (!s->app_->position().valid)
                s->toast(tr("Sin posici\xC3\xB3n GPS"));
            else
                s->open_dialog(DLG_NEW_POINT);
        },
        LV_EVENT_CLICKED, this);
    btn_points_ = tool_button(tool, 2, tr("Puntos"), "", nullptr, &pts_l2_, group);
    lv_obj_add_event_cb(
        btn_points_, [](lv_event_t *e) {
            auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
            s->show_panel(s->panel_kind_ == PANEL_POINTS ? PANEL_NONE : PANEL_POINTS);
        },
        LV_EVENT_CLICKED, this);
    btn_routes_ = tool_button(tool, 3, tr("Rutas"), "", nullptr, &routes_l2_, group);
    lv_obj_add_event_cb(
        btn_routes_, [](lv_event_t *e) {
            auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
            s->show_panel(s->panel_kind_ == PANEL_ROUTES ? PANEL_NONE : PANEL_ROUTES);
        },
        LV_EVENT_CLICKED, this);

    build_dialog();
    build_share();
    update_toolbar();
    update_center_button();
    update_names_button();
}

// ------------------------------------------------------------- datos
void MapScreen::set_layers(const std::vector<TileSource *> &layers)
{
    layers_ = layers;
    if (layer_ >= (int)layers_.size())
        layer_ = layers_.empty() ? -1 : 0;
    redraw();
}

void MapScreen::set_position(const Position &p)
{
    bool changed = p.valid != pos_.valid || p.lat != pos_.lat || p.lon != pos_.lon || p.alt != pos_.alt || p.acc_m != pos_.acc_m;
    pos_ = p;
    if (!changed)
        return;
    bool visible = lv_screen_active() == screen && !dirty_;
    if (follow_ && p.valid) {
        // con holgura: mientras el punto no se aleje del centro, solo se mueve el punto
        // (el GPS da una posición por segundo; recentrar es redibujar todo el mapa)
        double cx, cy, px, py;
        mercator_px(clat_, clon_, zoom_, cx, cy);
        mercator_px(p.lat, p.lon, zoom_, px, py);
        if (visible && std::fabs(px - cx) < MAP_W / 4.0 && std::fabs(py - cy) < MAP_H / 4.0) {
            place_me();
            return;
        }
        clat_ = p.lat;
        clon_ = p.lon;
        redraw();
        return;
    }
    if (visible) // sin seguir: el mapa queda donde está, solo el punto propio
        place_me();
    else
        dirty_ = true;
}

// Lo que el mapa muestra del equipo; si no cambió, no hay nada que redibujar
std::string MapScreen::team_signature() const
{
    std::string sig = team_only_ ? "1" : "0";
    char buf[96];
    for (auto &n : team_nodes()) {
        if (!n.has_pos)
            continue;
        snprintf(buf, sizeof(buf), "|%u,%ld,%ld,%d%d,%x,%d,", (unsigned)n.num, std::lround(n.lat * 2e5), std::lround(n.lon * 2e5),
                 n.in_team, n.stale, (unsigned)n.color, n.dist_m / 100);
        sig += buf + n.name;
    }
    return sig;
}

const std::vector<Waypoint> &MapScreen::wps() const { return app_->waypoints(); }
const Track &MapScreen::track() const { return app_->track(); }
const std::vector<SavedTrack> &MapScreen::saved() const { return app_->saved_tracks(); }

const std::vector<TeamNode> &MapScreen::team_nodes() const { return app_->nodes(); }

void MapScreen::set_team(const std::vector<TeamNode> &)
{
    if (lv_screen_active() == screen && !dirty_ && team_signature() == team_sig_)
        return; // llegó telemetría, pero en el mapa no cambia nada
    redraw();
}

void MapScreen::focus_node(uint32_t num)
{
    for (auto &n : team_nodes()) {
        if (n.num != num || !n.has_pos)
            continue;
        show_panel(PANEL_NONE);
        follow_ = false;
        clat_ = n.lat;
        clon_ = n.lon;
        trail_node_ = num;
        update_center_button();
        sel_kind_ = SEL_NODE; // su ficha: nombre, distancia, desnivel
        sel_id_ = num;
        redraw();
        return;
    }
    toast(tr("Ese nodo no ha enviado posici\xC3\xB3n"));
}

void MapScreen::set_waypoints(const std::vector<Waypoint> &) // la lista es la de la App
{
    update_toolbar();
    if (panel_kind_ == PANEL_POINTS)
        rebuild_points_panel();
    redraw();
}

// La ruta en curso suma un punto cada ~10 s: solo se rehace su línea (no teselas, marcadores
// ni rótulos), y si la pantalla no está a la vista, queda pendiente
void MapScreen::set_track(const Track &)
{
    update_toolbar();
    if (!view || lv_screen_active() != screen || dirty_) {
        dirty_ = true;
        return;
    }
    std::vector<lv_point_precise_t> fresh;
    line_points(track_proj_, track().pts, fresh);
    set_poly(track_poly_, fresh);
}

void MapScreen::set_saved_tracks(const std::vector<SavedTrack> &)
{
    bool shown = false;
    for (auto &t : saved())
        shown |= t.id == shown_route_;
    if (!shown)
        shown_route_ = 0;
    update_toolbar();
    if (panel_kind_ == PANEL_ROUTES)
        rebuild_routes_panel();
    redraw();
}

void MapScreen::on_enter()
{
    show_panel(PANEL_NONE);
    highlight_wp_ = 0;
    trail_node_ = 0;
    sel_kind_ = SEL_NONE;
    sel_id_ = 0;
    redraw();
    if (app_->take_recovered()) // se apagó grabando: la ruta está a salvo
        open_dialog(DLG_ROUTE, 1);
}

void MapScreen::zoom_by(int delta)
{
    int lo = 3, hi = 18;
    if (layer_ >= 0 && layer_ < (int)layers_.size()) {
        lo = std::max<int>(lo, layers_[layer_]->min_zoom());
        hi = std::min<int>(hi, layers_[layer_]->max_zoom());
    }
    int z = std::clamp((int)zoom_ + delta, lo, hi);
    if (z == zoom_)
        return;
    zoom_ = (uint8_t)z;
    redraw();
}

void MapScreen::pan(int32_t dx, int32_t dy)
{
    double x, y;
    mercator_px(clat_, clon_, zoom_, x, y);
    mercator_latlon(x - dx, y - dy, zoom_, clat_, clon_);
    follow_ = false;
    update_center_button();
    redraw();
}

void MapScreen::center_on_me()
{
    follow_ = true;
    highlight_wp_ = 0;
    if (pos_.valid) {
        clat_ = pos_.lat;
        clon_ = pos_.lon;
    }
    update_center_button();
    redraw();
}

void MapScreen::focus_waypoint(uint32_t id)
{
    for (auto &w : wps()) {
        if (w.id != id)
            continue;
        show_panel(PANEL_NONE);
        follow_ = false;
        clat_ = w.lat;
        clon_ = w.lon;
        highlight_wp_ = id;
        update_center_button();
        sel_kind_ = SEL_WP;
        sel_id_ = id;
        redraw();
        return;
    }
    toast(tr("El punto ya no existe"));
}

void MapScreen::update_center_button()
{
    // relleno azul = el mapa sigue tu posición
    set_bg(btn_center_, follow_ ? col::BLUE : col::SURF);
    set_border_color(btn_center_, follow_ ? col::BLUE : col::INK);
    set_border_color(center_ring_, follow_ ? col::SURF : col::INK);
    set_bg(center_dot_, follow_ ? col::SURF : col::INK);
}

// píxeles del mundo a zoom 22 (256·2²² = 2³⁰: cabe en int32)
static void world22(double lat, double lon, int32_t &x, int32_t &y)
{
    double wx, wy;
    mercator_px(lat, lon, 22, wx, wy);
    x = (int32_t)std::lround(wx);
    y = (int32_t)std::lround(wy);
}

bool MapScreen::to_screen(double lat, double lon, int32_t &sx, int32_t &sy) const
{
    int32_t wx, wy;
    world22(lat, lon, wx, wy);
    sx = px_x(wx);
    sy = px_y(wy);
    return sx >= 0 && sx < MAP_W && sy >= 0 && sy < MAP_H;
}

bool MapScreen::cached_screen(std::vector<Wxy> &cache, size_t i, uint32_t key, double lat, double lon, int32_t &sx,
                              int32_t &sy)
{
    if (cache.size() <= i)
        cache.resize(i + 1);
    Wxy &w = cache[i];
    if (w.key != key || w.lat != lat || w.lon != lon) {
        w.key = key, w.lat = lat, w.lon = lon;
        world22(lat, lon, w.x, w.y);
    }
    sx = px_x(w.x);
    sy = px_y(w.y);
    return sx >= 0 && sx < MAP_W && sy >= 0 && sy < MAP_H;
}

// --- Recorte de líneas. LVGL calcula las líneas con enteros de 32 bits y se desborda con
// coordenadas enormes (una ruta lejana vista con zoom alto). Cada tramo se recorta a un cuadro
// apenas más grande que la vista (lo que sobra del grosor de línea); si la línea sale y vuelve
// a entrar, se une por el borde de ese cuadro, que queda fuera de la pantalla, así nunca
// aparece un trazo que no existe. Un margen chico deja fuera de la cuenta casi todo lo que no
// se ve (antes 1200 px: miles de tramos invisibles por redibujo con una ruta larga).
static constexpr int32_t CLIP_M = 24;
static constexpr int32_t CX0 = -CLIP_M, CY0 = -CLIP_M, CX1 = MAP_W + CLIP_M, CY1 = MAP_H + CLIP_M;
static constexpr double CW = CX1 - CX0, CH = CY1 - CY0, PERIM = 2 * (CW + CH);

static bool clip_segment(double x0, double y0, double x1, double y1, double &t0, double &t1)
{
    t0 = 0, t1 = 1;
    double dx = x1 - x0, dy = y1 - y0;
    double p[4] = {-dx, dx, -dy, dy}, q[4] = {x0 - CX0, CX1 - x0, y0 - CY0, CY1 - y0};
    for (int i = 0; i < 4; i++) {
        if (p[i] == 0) {
            if (q[i] < 0)
                return false;
            continue;
        }
        double r = q[i] / p[i];
        if (p[i] < 0) {
            if (r > t1)
                return false;
            t0 = std::max(t0, r);
        } else {
            if (r < t0)
                return false;
            t1 = std::min(t1, r);
        }
    }
    return true;
}

static double perimeter_pos(double x, double y)
{
    double dt = std::fabs(y - CY0), dr = std::fabs(x - CX1), db = std::fabs(y - CY1), dl = std::fabs(x - CX0);
    double m = std::min(std::min(dt, dr), std::min(db, dl));
    if (m == dt)
        return x - CX0;
    if (m == dr)
        return CW + (y - CY0);
    if (m == db)
        return CW + CH + (CX1 - x);
    return 2 * CW + CH + (CY1 - y);
}

static void push_pt(std::vector<lv_point_precise_t> &out, int32_t x, int32_t y)
{
    if (!out.empty() && out.back().x == x && out.back().y == y)
        return;
    out.push_back({(lv_value_precise_t)x, (lv_value_precise_t)y});
}

// camino por el borde del cuadro, por el lado más corto, sumando las esquinas
static void route_border(std::vector<lv_point_precise_t> &out, double ax, double ay, double bx, double by)
{
    static const double corner_s[4] = {0, CW, CW + CH, 2 * CW + CH};
    static const int32_t corner_x[4] = {CX0, CX1, CX1, CX0}, corner_y[4] = {CY0, CY0, CY1, CY1};
    double sa = perimeter_pos(ax, ay), sb = perimeter_pos(bx, by);
    double fwd = std::fmod(sb - sa + PERIM, PERIM);
    bool forward = fwd <= PERIM / 2;
    double dist = forward ? fwd : PERIM - fwd;
    for (int step = 0; step < 4; step++) {
        double best = 1e18;
        int bi = -1;
        for (int i = 0; i < 4; i++) {
            double d = forward ? std::fmod(corner_s[i] - sa + PERIM, PERIM) : std::fmod(sa - corner_s[i] + PERIM, PERIM);
            if (d > 1e-6 && d < best)
                best = d, bi = i;
        }
        if (bi < 0 || best >= dist)
            break;
        push_pt(out, corner_x[bi], corner_y[bi]);
        dist -= best;
        sa = corner_s[bi];
    }
}

static inline bool in_clip(int32_t x, int32_t y) { return x >= CX0 && x <= CX1 && y >= CY0 && y <= CY1; }

// Lo común (tramo dentro del cuadro, o fuera por un mismo lado) se resuelve con enteros;
// solo los tramos que cruzan el borde hacen la cuenta en double.
static void clip_polyline(const std::vector<lv_point_t> &in, std::vector<lv_point_precise_t> &out)
{
    out.clear();
    if (in.size() == 1) {
        if (in_clip(in[0].x, in[0].y))
            push_pt(out, in[0].x, in[0].y);
        return;
    }
    bool have = false;
    int32_t lx = 0, ly = 0;
    for (size_t i = 1; i < in.size(); i++) {
        int32_t ax = in[i - 1].x, ay = in[i - 1].y, bx = in[i].x, by = in[i].y;
        int32_t c0x, c0y, c1x, c1y;
        if (in_clip(ax, ay) && in_clip(bx, by)) {
            c0x = ax, c0y = ay, c1x = bx, c1y = by;
        } else {
            if ((ax < CX0 && bx < CX0) || (ax > CX1 && bx > CX1) || (ay < CY0 && by < CY0) || (ay > CY1 && by > CY1))
                continue; // tramo completo fuera, de un mismo lado
            double t0, t1;
            if (!clip_segment(ax, ay, bx, by, t0, t1))
                continue;
            c0x = (int32_t)std::lround(ax + t0 * (double)(bx - ax)), c0y = (int32_t)std::lround(ay + t0 * (double)(by - ay));
            c1x = (int32_t)std::lround(ax + t1 * (double)(bx - ax)), c1y = (int32_t)std::lround(ay + t1 * (double)(by - ay));
        }
        if (!have) {
            push_pt(out, c0x, c0y);
            have = true;
        } else if (lx != c0x || ly != c0y) {
            route_border(out, lx, ly, c0x, c0y); // salió y volvió a entrar: por el borde
            push_pt(out, c0x, c0y);
        }
        push_pt(out, c1x, c1y);
        lx = c1x, ly = c1y;
    }
}

// lv_line_set_points redibuja siempre: solo si los puntos cambiaron. LVGL guarda el puntero,
// así que la lista queda en "keep" (miembro de la pantalla) mientras se dibuja.
static bool same_points(const std::vector<lv_point_precise_t> &a, const std::vector<lv_point_precise_t> &b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); i++)
        if (a[i].x != b[i].x || a[i].y != b[i].y)
            return false;
    return true;
}

static void set_line(lv_obj_t *line, std::vector<lv_point_precise_t> &keep, std::vector<lv_point_precise_t> &fresh)
{
    if (fresh.size() < 2) {
        show(line, false);
        return;
    }
    if (!same_points(keep, fresh) || lv_obj_has_flag(line, LV_OBJ_FLAG_HIDDEN)) {
        keep.swap(fresh);
        lv_line_set_points(line, keep.data(), (uint32_t)keep.size());
    }
    show(line, true);
}

void MapScreen::init_poly(Poly &p)
{
    p.layer = lv_obj_create(view);
    lv_obj_remove_style_all(p.layer);
    lv_obj_set_size(p.layer, MAP_W, MAP_H);
    lv_obj_remove_flag(p.layer, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE));
    lv_style_init(&p.style);
}

void MapScreen::hide_poly(Poly &p)
{
    for (lv_obj_t *l : p.parts)
        show(l, false);
}

static constexpr size_t POLY_CHUNK = 24; // puntos por trozo (comparten el último con el siguiente)

void MapScreen::set_poly(Poly &p, std::vector<lv_point_precise_t> &fresh)
{
    size_t parts = fresh.size() < 2 ? 0 : (fresh.size() - 2) / (POLY_CHUNK - 1) + 1;
    std::vector<lv_point_precise_t> local;
    for (size_t i = 0; i < parts; i++) {
        size_t b = i * (POLY_CHUNK - 1), e = std::min(b + POLY_CHUNK, fresh.size());
        int32_t x0 = INT32_MAX, y0 = INT32_MAX;
        for (size_t k = b; k < e; k++)
            x0 = std::min<int32_t>(x0, fresh[k].x), y0 = std::min<int32_t>(y0, fresh[k].y);
        local.resize(e - b);
        for (size_t k = b; k < e; k++)
            local[k - b] = {(lv_value_precise_t)(fresh[k].x - x0), (lv_value_precise_t)(fresh[k].y - y0)};
        if (i == p.parts.size()) {
            lv_obj_t *l = lv_line_create(p.layer);
            lv_obj_add_style(l, &p.style, 0);
            p.parts.push_back(l);
            p.pts.emplace_back();
        }
        lv_obj_t *l = p.parts[i];
        lv_obj_set_pos(l, x0, y0); // sin cambio, no redibuja
        if (!same_points(p.pts[i], local) || lv_obj_has_flag(l, LV_OBJ_FLAG_HIDDEN)) {
            p.pts[i].swap(local);
            lv_line_set_points(l, p.pts[i].data(), (uint32_t)p.pts[i].size());
        }
        show(l, true);
    }
    for (size_t i = parts; i < p.parts.size(); i++)
        show(p.parts[i], false);
}

static bool same_tp(const TrackPoint &a, const TrackPoint &b) { return a.lat == b.lat && a.lon == b.lon && a.epoch == b.epoch; }

// Proyecta solo lo nuevo: la ruta en curso crece por el final
void MapScreen::project(Proj &c, const std::vector<TrackPoint> &pts)
{
    if (pts.empty()) {
        c.n = 0;
        c.xy.clear();
        return;
    }
    size_t from = 0;
    if (c.n && pts.size() >= c.n && same_tp(pts[0], c.first) && same_tp(pts[c.n - 1], c.last))
        from = c.n;
    if (from == pts.size())
        return;
    c.xy.resize(pts.size() * 2);
    for (size_t i = from; i < pts.size(); i++)
        world22(pts[i].lat, pts[i].lon, c.xy[2 * i], c.xy[2 * i + 1]);
    c.n = pts.size();
    c.first = pts.front();
    c.last = pts.back();
}

// A pantalla, sin los puntos a menos de 2 px del anterior (no se distinguen y cada tramo
// cuesta: con una ruta de 12 h a zoom bajo, miles de tramos de una fracción de píxel)
void MapScreen::line_points(Proj &c, const std::vector<TrackPoint> &pts, std::vector<lv_point_precise_t> &out)
{
    project(c, pts);
    std::vector<lv_point_t> raw;
    raw.reserve(std::min<size_t>(c.n, 2048));
    for (size_t i = 0; i < c.n; i++) {
        int32_t x = px_x(c.xy[2 * i]), y = px_y(c.xy[2 * i + 1]);
        if (!raw.empty() && i + 1 < c.n) {
            int32_t dx = x - raw.back().x, dy = y - raw.back().y;
            if (dx < 2 && dx > -2 && dy < 2 && dy > -2)
                continue;
        }
        raw.push_back({x, y});
    }
    clip_polyline(raw, out);
}

// ------------------------------------------------------------- dibujo
void MapScreen::set_grid_line(int i, lv_point_precise_t a, lv_point_precise_t b)
{
    lv_point_precise_t *p = &grid_pts_[i * 2];
    bool hidden = lv_obj_has_flag(grid_[i], LV_OBJ_FLAG_HIDDEN);
    if (hidden || p[0].x != a.x || p[0].y != a.y || p[1].x != b.x || p[1].y != b.y) {
        p[0] = a;
        p[1] = b;
        lv_line_set_points(grid_[i], p, 2);
    }
    show(grid_[i], true);
}

void MapScreen::shown()
{
    if (dirty_)
        redraw();
}

// Encuadra una lista de puntos: el mayor zoom en que entran todos, con margen
void MapScreen::fit(const std::vector<TrackPoint> &pts)
{
    if (pts.empty())
        return;
    double la0 = 90, la1 = -90, lo0 = 180, lo1 = -180;
    for (auto &p : pts)
        la0 = std::min(la0, p.lat), la1 = std::max(la1, p.lat), lo0 = std::min(lo0, p.lon), lo1 = std::max(lo1, p.lon);
    fit_box(la0, la1, lo0, lo1);
}

void MapScreen::fit_box(double lat0, double lat1, double lon0, double lon1)
{
    double x0, y0, x1, y1;
    mercator_px(lat1, lon0, 0, x0, y0); // noroeste
    mercator_px(lat0, lon1, 0, x1, y1); // sureste
    int lo = 3, hi = 18;
    if (layer_ >= 0 && layer_ < (int)layers_.size()) {
        lo = std::max<int>(lo, layers_[layer_]->min_zoom());
        hi = std::min<int>(hi, layers_[layer_]->max_zoom());
    }
    int z = lo;
    for (int k = hi; k >= lo; k--) {
        double f = std::pow(2.0, k);
        if ((x1 - x0) * f <= CTRL_X - 30 && (y1 - y0) * f <= MAP_H - 50) {
            z = k;
            break;
        }
    }
    zoom_ = (uint8_t)z;
    mercator_latlon((x0 + x1) / 2, (y0 + y1) / 2, 0, clat_, clon_);
    follow_ = false;
    update_center_button();
}

void MapScreen::show_route(uint32_t id)
{
    shown_route_ = 0;
    for (auto &t : saved())
        if (t.id == id) {
            if (!app_->route_points(id)) { // se lee de la tarjeta SD
                toast(tr("No se pudo leer la ruta de la tarjeta"));
                break;
            }
            shown_route_ = id;
            fit_box(t.lat0, t.lat1, t.lon0, t.lon1);
        }
    redraw();
}

void MapScreen::redraw()
{
    if (!view)
        return;
    // sin pantalla visible no se dibuja: ahorra batería cuando llegan posiciones o telemetría
    if (lv_screen_active() != screen) {
        dirty_ = true;
        return;
    }
    dirty_ = false;
    double cx, cy;
    mercator_px(clat_, clon_, zoom_, cx, cy);
    double left = cx - MAP_W / 2.0, top = cy - MAP_H / 2.0;
    double mpp = meters_per_px(clat_, zoom_);
    shift_ = 22 - zoom_;
    l22_ = std::llround(left * (double)(1LL << shift_));
    t22_ = std::llround(top * (double)(1LL << shift_));

    // --- Mapa base
    TileSource *src = (layer_ >= 0 && layer_ < (int)layers_.size()) ? layers_[layer_] : nullptr;
    int used = 0;
    std::vector<uint64_t> keys;
    if (src) {
        int32_t tx0 = (int32_t)std::floor(left / 256), ty0 = (int32_t)std::floor(top / 256);
        int32_t tx1 = (int32_t)std::floor((left + MAP_W - 1) / 256), ty1 = (int32_t)std::floor((top + MAP_H - 1) / 256);
        int32_t n = 1 << zoom_;
        for (int32_t ty = ty0; ty <= ty1; ty++)
            for (int32_t tx = tx0; tx <= tx1; tx++) {
                if (used >= 6 || ty < 0 || ty >= n)
                    continue;
                uint32_t wx = (uint32_t)((tx % n + n) % n);
                const lv_image_dsc_t *img = src->tile(zoom_, wx, (uint32_t)ty);
                if (!img)
                    continue;
                keys.push_back(TileSource::key(zoom_, wx, (uint32_t)ty));
                lv_obj_t *t = tiles_[used++];
                if (lv_image_get_src(t) != img)
                    lv_image_set_src(t, img);
                lv_obj_set_pos(t, (int32_t)std::lround(tx * 256 - left), (int32_t)std::lround(ty * 256 - top));
                show(t, true);
            }
        src->retain(keys);
    }
    for (int i = used; i < 6; i++)
        show(tiles_[i], false);

    // --- Cuadrícula UTM sin mapa base (o sin cobertura)
    int glines = 0, grid_step = 0;
    if (!src || used == 0) {
        static const int steps[] = {100, 200, 500, 1000, 2000, 5000, 10000, 20000};
        int step = steps[0];
        for (int s : steps) {
            step = s;
            if (s / mpp >= 60)
                break;
        }
        grid_step = step;
        Utm u = to_utm(clat_, clon_);
        double e0 = u.e - (MAP_W / 2.0) * mpp, n_top = u.n + (MAP_H / 2.0) * mpp;
        for (double e = std::ceil(e0 / step) * step; e < e0 + MAP_W * mpp && glines < 8; e += step) {
            int32_t x = (int32_t)std::lround((e - e0) / mpp);
            lv_point_precise_t a = {(lv_value_precise_t)x, 0}, b = {(lv_value_precise_t)x, (lv_value_precise_t)MAP_H};
            set_grid_line(glines, a, b);
            glines++;
        }
        for (double nn = std::floor(n_top / step) * step; nn > n_top - MAP_H * mpp && glines < 16; nn -= step) {
            int32_t y = (int32_t)std::lround((n_top - nn) / mpp);
            lv_point_precise_t a = {0, (lv_value_precise_t)y}, b = {(lv_value_precise_t)MAP_W, (lv_value_precise_t)y};
            set_grid_line(glines, a, b);
            glines++;
        }
    }
    for (int i = glines; i < 16; i++)
        show(grid_[i], false);

    // --- Ruta guardada mostrada y ruta en curso
    // (los puntos de la ruta mostrada se leen de la tarjeta SD al mostrarla)
    const std::vector<TrackPoint> *route = shown_route_ ? app_->route_points(shown_route_) : nullptr;
    std::vector<lv_point_precise_t> fresh;
    if (route) {
        line_points(route_proj_, *route, fresh);
        set_poly(route_poly_, fresh);
    } else
        hide_poly(route_poly_);
    // rastro del compañero elegido
    if (trail_node_) {
        uint32_t tc = col::INK;
        for (auto &n : team_nodes())
            if (n.num == trail_node_)
                tc = n.color;
        if (tc != trail_color_) { // estilo compartido por los trozos: un solo cambio
            trail_color_ = tc;
            lv_style_set_line_color(&trail_poly_.style, c(tc));
            lv_obj_report_style_change(&trail_poly_.style);
        }
        line_points(trail_proj_, app_->trail(trail_node_), fresh);
        set_poly(trail_poly_, fresh);
    } else
        hide_poly(trail_poly_);
    line_points(track_proj_, track().pts, fresh);
    set_poly(track_poly_, fresh);

    // --- Coordenadas (antes de los marcadores: las etiquetas las evitan)
    bool strip_on = ruler_;
    if (follow_ && pos_.valid) {
        char acc[16];
        snprintf(acc, sizeof(acc), tr("  \xC2\xB1%u m"), pos_.acc_m);
        set_text(coords_, format_utm(to_utm(pos_.lat, pos_.lon)) + acc);
        show(cross_, false);
    } else {
        set_text(coords_, tr("Centro  ") + format_utm(to_utm(clat_, clon_)));
    }
    if (!follow_ || ruler_) {
        show(cross_, true);
        ui::align(cross_, LV_ALIGN_CENTER, 0, 0);
    }
    lv_obj_set_pos(coords_, 4, strip_on ? 24 : 4);
    lv_obj_set_pos(north_, 3, MAP_H - 33); // abajo a la izquierda, junto a la escala
    // columna de botones centrada en el alto libre (bajo la franja, si la hay)
    {
        int32_t top = strip_on ? 20 : 0, step = 27, n = 5;
        int32_t y0 = top + (MAP_H - top - (n * step - 2)) / 2;
        lv_obj_t *col_btns[5] = {zoom_btn_[0], zoom_btn_[1], btn_center_, btn_ruler_, btn_names_};
        for (int32_t i = 0; i < n; i++)
            lv_obj_set_y(col_btns[i], y0 + i * step);
    }

    place_markers();
    update_names_button(); // los ajustes pudieron cambiar en otra pantalla

    place_ruler();

    // --- Escala
    static const int nice[] = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000};
    int dist = nice[0];
    for (int d : nice)
        if (d / mpp <= 80)
            dist = d;
    int32_t len = (int32_t)std::lround(dist / mpp);
    lv_obj_set_size(scale_bar_, len + 2, 5);
    lv_obj_set_pos(scale_bar_, 24, MAP_H - 10);
    set_text(scale_lbl_, dist >= 1000 ? std::to_string(dist / 1000) + tr(" km") : std::to_string(dist) + tr(" m"));
    ui::align_to(scale_lbl_, scale_bar_, LV_ALIGN_OUT_RIGHT_MID, 4, -1);

    if (src && *src->attribution()) {
        set_text(attrib_, src->attribution());
        show(attrib_, true);
    } else if (!src) {
        std::string g = grid_step >= 1000 ? std::to_string(grid_step / 1000) + tr(" km") : std::to_string(grid_step) + tr(" m");
        set_text(attrib_, tr("Cuadr\xC3\xAD" "cula UTM ") + g);
        show(attrib_, true);
    } else
        show(attrib_, false);
    ui::align(attrib_, LV_ALIGN_BOTTOM_RIGHT, -(MAP_W - CTRL_X) - 2, -3); // a la izquierda de los botones

    set_text(layer_btn_lbl_, src ? src->name() : tr("Sin mapa base"));
    update_strip();

    // encima de los marcadores (que se crean a medida que hacen falta); cambiar el orden
    // redibuja todo el mapa, así que solo si no está ya así
    keep_on_top({me_halo_, me_dot_, ruler_layer_, cross_, coords_, north_, strip_, toast_});
    team_sig_ = team_signature();
}

// Posición propia, línea al destino, coordenadas y franja: lo que cambia con cada fix de GPS
void MapScreen::place_me()
{
    double mpp = meters_per_px(clat_, zoom_);
    int32_t mx = 0, my = 0;
    bool me_in = false;
    if (pos_.valid) {
        me_in = to_screen(pos_.lat, pos_.lon, mx, my);
        int32_t r = (int32_t)std::clamp(pos_.acc_m / mpp, 8.0, 70.0);
        lv_obj_set_size(me_halo_, r * 2, r * 2);
        lv_obj_set_pos(me_halo_, mx - r, my - r);
        lv_obj_set_pos(me_dot_, mx - 6, my - 6);
    }
    show(me_halo_, me_in);
    show(me_dot_, me_in);
    if (follow_ && pos_.valid) {
        char acc[16];
        snprintf(acc, sizeof(acc), tr("  \xC2\xB1%u m"), pos_.acc_m);
        set_text(coords_, format_utm(to_utm(pos_.lat, pos_.lon)) + acc);
    }
    update_strip();
}

void MapScreen::place_markers()
{
    place_me();
    hits_.clear();
    const Settings &st = app_->settings();
    auto pinned = [](const std::vector<uint32_t> &v, uint32_t id) { return std::find(v.begin(), v.end(), id) != v.end(); };
    bool strip_on = ruler_;

    // Puntos: cuadrado (los recibidos de otro equipo, con borde de color y fondo blanco).
    // Una campaña junta cientos: solo se crean marcadores para los que se ven, hasta
    // MAP_WP_MARKS (los más cercanos al centro).
    struct WpShown {
        const Waypoint *w;
        int32_t sx, sy;
        double d2;
    };
    std::vector<WpShown> wv;
    {
        const std::vector<Waypoint> &all = wps();
        for (size_t i = 0; i < all.size(); i++) {
            int32_t sx, sy;
            if (!cached_screen(wp_xy_, i, all[i].id, all[i].lat, all[i].lon, sx, sy))
                continue;
            double dx = sx - MAP_W / 2.0, dy = sy - MAP_H / 2.0;
            bool key = highlight_wp_ == all[i].id ||
                       (sel_kind_ == SEL_WP && sel_id_ == all[i].id);
            uint8_t layer = all[i].from.empty() ? MAP_NAMES_MINE : MAP_NAMES_SHARED;
            if ((st.map_hidden & layer) && !key) // capa oculta (el destino y lo elegido, igual se ven)
                continue;
            wv.push_back({&all[i], sx, sy, key ? -1.0 : dx * dx + dy * dy});
        }
        if (wp_xy_.size() > all.size())
            wp_xy_.resize(all.size());
    }
    std::sort(wv.begin(), wv.end(), [](const WpShown &a, const WpShown &b) { return a.d2 < b.d2; });
    if (wv.size() > MAP_WP_MARKS)
        wv.resize(MAP_WP_MARKS);
    while (wp_marks_.size() < wv.size()) {
        NodeMarker m{};
        m.dot = box(view, 0, 0, 10, 10, WP_COLOR);
        border(m.dot, col::SURF, 2);
        lv_obj_set_style_radius(m.dot, 1, 0);
        m.label = pill(view, &tfield_font_12b, WP_COLOR, col::SURF, LV_OPA_80);
        wp_marks_.push_back(m);
    }
    for (size_t i = 0; i < wp_marks_.size(); i++) {
        NodeMarker &m = wp_marks_[i];
        show(m.label, false);
        if (i >= wv.size()) {
            show(m.dot, false);
            continue;
        }
        const Waypoint &w = *wv[i].w;
        bool theirs = !w.from.empty();
        set_bg(m.dot, theirs ? col::SURF : WP_COLOR);
        set_border_color(m.dot, theirs ? WP_COLOR : col::SURF);
        show(m.dot, true);
        lv_obj_set_pos(m.dot, wv[i].sx - 5, wv[i].sy - 5);
        hits_.push_back({wv[i].sx, wv[i].sy, SEL_WP, w.id, false, i});
    }

    // Equipo y red: punto; fuera de la vista, en el borde con una flecha hacia donde está.
    // Con 250 nodos en la red no se crean 500 objetos: solo marcadores para lo que se dibuja
    // (mi equipo siempre; los ajenos a la vista, hasta 40, los más cercanos al centro).
    struct Shown {
        const TeamNode *n;
        int32_t sx, sy;
        bool inside;
    };
    std::vector<Shown> list, others;
    const std::vector<TeamNode> &nodes = team_nodes();
    if (node_xy_.size() > nodes.size())
        node_xy_.resize(nodes.size());
    for (size_t ni = 0; ni < nodes.size(); ni++) {
        const TeamNode &n = nodes[ni];
        if (!n.has_pos)
            continue;
        int32_t sx, sy;
        bool inside = cached_screen(node_xy_, ni, n.num, n.lat, n.lon, sx, sy);
        bool key = n.num == trail_node_ || (sel_kind_ == SEL_NODE && sel_id_ == n.num);
        if (n.in_team && (st.map_hidden & MAP_NAMES_TEAM) && !key)
            continue; // capa "Mi equipo" oculta
        if (n.in_team || key)
            list.push_back({&n, sx, sy, inside});
        else if (inside && !team_only_)
            others.push_back({&n, sx, sy, inside});
    }
    if (others.size() > MAP_NODE_MARKS) {
        auto d2 = [](const Shown &a) {
            double dx = a.sx - MAP_W / 2.0, dy = a.sy - MAP_H / 2.0;
            return dx * dx + dy * dy;
        };
        std::partial_sort(others.begin(), others.begin() + MAP_NODE_MARKS, others.end(),
                          [&](const Shown &a, const Shown &b) { return d2(a) < d2(b); });
        others.resize(MAP_NODE_MARKS);
    }
    list.insert(list.end(), others.begin(), others.end());
    while (team_marks_.size() < list.size()) {
        NodeMarker m{};
        m.dot = box(view, 0, 0, 10, 10, col::INK);
        lv_obj_set_style_radius(m.dot, LV_RADIUS_CIRCLE, 0);
        border(m.dot, col::SURF, 2);
        m.label = pill(view, &tfield_font_12b, col::SURF, col::INK, LV_OPA_COVER);
        m.arrow = lv_line_create(view);
        lv_obj_set_style_line_width(m.arrow, 3, 0);
        lv_obj_set_style_line_rounded(m.arrow, true, 0);
        arrow_pts_.push_back({});
        lv_line_set_points(m.arrow, arrow_pts_.back().data(), 3);
        team_marks_.push_back(m);
    }
    // borde de la vista donde van los que están fuera (dejando lugar a la flecha)
    const double top_m = strip_on ? 52 : 34, bot_m = 32, side_m = 16;
    for (size_t i = 0; i < team_marks_.size(); i++) {
        NodeMarker &m = team_marks_[i];
        show(m.label, false);
        if (i >= list.size()) {
            show(m.dot, false);
            show(m.arrow, false);
            continue;
        }
        const TeamNode &n = *list[i].n;
        int32_t sx = list[i].sx, sy = list[i].sy;
        uint32_t color = !n.in_team ? col::MUTED : (n.stale ? col::AMBER : n.color);
        set_bg(m.dot, color);
        set_bg(m.label, color);
        show(m.dot, true);
        if (list[i].inside) {
            lv_obj_set_pos(m.dot, sx - 5, sy - 5);
            show(m.arrow, false);
            hits_.push_back({sx, sy, SEL_NODE, n.num, false, i});
            continue;
        }
        double cy0 = (top_m + MAP_H - bot_m) / 2.0, half_h = (MAP_H - bot_m - top_m) / 2.0;
        double dx = sx - CTRL_X / 2.0, dy = sy - cy0;
        double kx = dx != 0 ? (CTRL_X / 2.0 - side_m) / std::fabs(dx) : 1e9;
        double ky = dy != 0 ? half_h / std::fabs(dy) : 1e9;
        double k = std::min(kx, ky);
        int32_t ex = (int32_t)(CTRL_X / 2.0 + dx * k), ey = (int32_t)(cy0 + dy * k);
        lv_obj_set_pos(m.dot, ex - 5, ey - 5);
        // flecha: punta hacia afuera, en el color del nodo
        double len = std::sqrt(dx * dx + dy * dy);
        double ux = len > 0 ? dx / len : 0, uy = len > 0 ? dy / len : -1;
        auto &p = arrow_pts_[i];
        p[0] = {(lv_value_precise_t)(ex + ux * 8 - uy * 5), (lv_value_precise_t)(ey + uy * 8 + ux * 5)};
        p[1] = {(lv_value_precise_t)(ex + ux * 13), (lv_value_precise_t)(ey + uy * 13)};
        p[2] = {(lv_value_precise_t)(ex + ux * 8 + uy * 5), (lv_value_precise_t)(ey + uy * 8 - ux * 5)};
        lv_obj_set_style_line_color(m.arrow, c(color), 0);
        lv_line_set_points(m.arrow, p.data(), 3);
        show(m.arrow, true);
        hits_.push_back({ex, ey, SEL_NODE, n.num, true, i});
    }

    // Lugares ocupados (en coordenadas de la pantalla): coordenadas, norte, franja, botones,
    // escala, marcadores y la ficha de lo elegido; los nombres buscan un hueco libre
    lv_area_t va;
    lv_obj_get_coords(view, &va);
    std::vector<lv_area_t> placed;
    for (lv_obj_t *fixed : {coords_, north_, strip_, scale_lbl_}) {
        if (lv_obj_has_flag(fixed, LV_OBJ_FLAG_HIDDEN))
            continue;
        lv_obj_update_layout(fixed);
        lv_area_t a;
        lv_obj_get_coords(fixed, &a);
        placed.push_back(a);
    }
    placed.push_back({va.x1 + CTRL_X - 1, va.y1, va.x2, va.y2}); // columna de botones
    for (auto &h : hits_)
        placed.push_back({va.x1 + h.x - 6, va.y1 + h.y - 6, va.x1 + h.x + 6, va.y1 + h.y + 6});
    if (!lv_obj_has_flag(me_dot_, LV_OBJ_FLAG_HIDDEN)) {
        lv_area_t a;
        lv_obj_get_coords(me_dot_, &a);
        placed.push_back(a);
    }
    place_selection(placed);

    // Nombres: lo esencial (destino, rastro), luego por tipo (equipo, compartidos, míos, red)
    // y los marcados uno por uno; lo elegido no, porque ya lo dice su ficha
    struct Want {
        lv_obj_t *label;
        std::string text;
        int32_t x, y;
        int prio;
        bool edge;
    };
    std::vector<Want> want;
    size_t wp_named = 0;
    for (auto &h : hits_) {
        if (h.kind == sel_kind_ && h.id == sel_id_)
            continue;
        if (h.kind == SEL_WP) {
            const Waypoint &w = *wv[h.mark].w;
            bool theirs = !w.from.empty();
            bool key = highlight_wp_ == w.id;
            bool pin = pinned(st.map_name_wps, w.id);
            bool type = (st.map_names & (theirs ? MAP_NAMES_SHARED : MAP_NAMES_MINE)) && wp_named < MAP_WP_LABELS;
            if (!key && !pin && !type)
                continue;
            if (type && !key && !pin)
                wp_named++;
            want.push_back({wp_marks_[h.mark].label, theirs ? w.name + " \xC2\xB7 " + w.from : w.name, h.x, h.y,
                            key ? 0 : pin ? 1 : 3, false});
        } else {
            const TeamNode &n = *list[h.mark].n;
            bool key = trail_node_ == n.num;
            bool pin = pinned(st.map_name_nodes, n.num);
            bool type = st.map_names & (n.in_team ? MAP_NAMES_TEAM : MAP_NAMES_OTHERS);
            if (!key && !pin && !type)
                continue;
            want.push_back({team_marks_[h.mark].label, h.edge ? n.name + " " + format_distance(n.dist_m) : n.name, h.x,
                            h.y, key ? 0 : pin ? 1 : n.in_team ? 2 : 4, h.edge});
        }
    }
    std::stable_sort(want.begin(), want.end(), [](const Want &a, const Want &b) { return a.prio < b.prio; });
    int32_t ymin = strip_on ? 22 : 2, ymax = MAP_H - 16;
    for (auto &wn : want) {
        set_text(wn.label, wn.text);
        lv_obj_update_layout(wn.label);
        int32_t w = lv_obj_get_width(wn.label), h = lv_obj_get_height(wn.label);
        // a la derecha, a la izquierda, arriba o abajo del marcador; el primer hueco libre
        const int32_t cand[4][2] = {{wn.x + 8, wn.y - h / 2}, {wn.x - 8 - w, wn.y - h / 2},
                                    {wn.x - w / 2, wn.y - 8 - h}, {wn.x - w / 2, wn.y + 8}};
        int best = -1;
        for (int k = 0; k < 4 && best < 0; k++) {
            int32_t x = cand[k][0], y = cand[k][1];
            if (x < 2 || x + w > CTRL_X - 2 || y < ymin || y + h > ymax)
                continue;
            lv_area_t a{va.x1 + x, va.y1 + y, va.x1 + x + w - 1, va.y1 + y + h - 1};
            bool hit = false;
            for (auto &p : placed)
                hit |= a.x1 <= p.x2 && a.x2 >= p.x1 && a.y1 <= p.y2 && a.y2 >= p.y1;
            if (!hit)
                best = k;
        }
        if (best < 0 && wn.prio > 1)
            continue; // sin lugar: solo el marcador (al acercar el mapa, aparece)
        int32_t x = best >= 0 ? cand[best][0] : std::clamp(cand[0][0], (int32_t)2, CTRL_X - 2 - w);
        int32_t y = best >= 0 ? cand[best][1] : std::clamp(cand[0][1], ymin, ymax - h);
        if (best < 0 && x + w > CTRL_X - 2)
            x = std::clamp(cand[1][0], (int32_t)2, CTRL_X - 2 - w);
        lv_obj_set_pos(wn.label, x, y);
        show(wn.label, true);
        placed.push_back({va.x1 + x, va.y1 + y, va.x1 + x + w - 1, va.y1 + y + h - 1});
    }
    ui::keep_on_top({sel_ring_, callout_, toast_});
}

// La ficha de lo elegido, sobre o bajo su marcador
void MapScreen::place_selection(std::vector<lv_area_t> &placed)
{
    const Hit *hit = nullptr;
    for (auto &h : hits_)
        if (h.kind == sel_kind_ && h.id == sel_id_)
            hit = &h;
    if (!hit) {
        if (sel_kind_ != SEL_NONE) { // ya no se dibuja (punto borrado, nodo sin posición)
            sel_kind_ = SEL_NONE;
            sel_id_ = 0;
        }
        show(sel_ring_, false);
        show(callout_, false);
        return;
    }
    uint32_t color = WP_COLOR;
    std::string l1, l2;
    if (hit->kind == SEL_NODE) {
        for (auto &n : team_nodes())
            if (n.num == hit->id) {
                color = !n.in_team ? col::MUTED : (n.stale ? col::AMBER : n.color);
                l1 = n.name + " \xC2\xB7 " + format_ago(n.heard_ago_s);
                l2 = n.dist_m >= 0 ? format_distance(n.dist_m) + " " + cardinal(n.bearing_deg) : tr("sin posici\xC3\xB3n");
                if (n.has_alt && n.dist_m >= 0)
                    l2 += " \xC2\xB7 " + format_alt_diff(n.alt_diff_m);
            }
        set_text(callout_l3_, tr("Enter: ficha \xC2\xB7 \xE2\x80\xB9 \xE2\x80\xBA otro"));
    } else {
        for (auto &w : wps())
            if (w.id == hit->id) {
                l1 = w.from.empty() ? w.name : w.name + " \xC2\xB7 " + tr("de ") + w.from;
                if (pos_.valid) {
                    int32_t d = (int32_t)haversine_m(pos_.lat, pos_.lon, w.lat, w.lon);
                    l2 = format_distance(d) + " " + cardinal((int)bearing_deg(pos_.lat, pos_.lon, w.lat, w.lon)) +
                         " \xC2\xB7 " + format_alt_diff(w.alt - pos_.alt);
                } else
                    l2 = w.time;
            }
        set_text(callout_l3_, tr("Enter: editar \xC2\xB7 \xE2\x80\xB9 \xE2\x80\xBA otro"));
    }
    set_text(callout_l1_, l1);
    set_text(callout_l2_, l2);
    set_border_color(callout_, color);
    show(sel_ring_, true);
    lv_obj_set_pos(sel_ring_, hit->x - 11, hit->y - 11);
    show(callout_, true);
    lv_obj_update_layout(callout_);
    int32_t w = lv_obj_get_width(callout_), h = lv_obj_get_height(callout_);
    int32_t top = (ruler_) ? 22 : 20;
    lv_area_t va;
    lv_obj_get_coords(view, &va);
    // arriba, abajo, a la derecha o a la izquierda: donde no tape otro marcador ni tu posición
    const int32_t cand[4][2] = {{hit->x - w / 2, hit->y - 14 - h}, {hit->x - w / 2, hit->y + 14},
                                {hit->x + 14, hit->y - h / 2}, {hit->x - 14 - w, hit->y - h / 2}};
    int32_t x = 0, y = 0;
    for (int k = 0; k < 5; k++) {
        int kk = k < 4 ? k : 0; // ninguno libre: arriba (o abajo si no cabe)
        x = std::clamp(cand[kk][0], (int32_t)2, CTRL_X - 3 - w);
        y = cand[kk][1];
        if (k == 4 && y < top)
            y = cand[1][1];
        y = std::clamp(y, top, MAP_H - 2 - h);
        if (k == 4)
            break;
        lv_area_t a{va.x1 + x, va.y1 + y, va.x1 + x + w - 1, va.y1 + y + h - 1};
        bool clash = false;
        for (auto &p : placed) {
            bool own = p.x1 <= va.x1 + hit->x && p.x2 >= va.x1 + hit->x && p.y1 <= va.y1 + hit->y && p.y2 >= va.y1 + hit->y;
            if (!own && a.x1 <= p.x2 && a.x2 >= p.x1 && a.y1 <= p.y2 && a.y2 >= p.y1)
                clash = true;
        }
        // la ficha no tapa su propio marcador
        if (a.x1 <= va.x1 + hit->x + 6 && a.x2 >= va.x1 + hit->x - 6 && a.y1 <= va.y1 + hit->y + 6 && a.y2 >= va.y1 + hit->y - 6)
            clash = true;
        if (!clash)
            break;
    }
    lv_obj_set_pos(callout_, x, y);
    placed.push_back({va.x1 + x, va.y1 + y, va.x1 + x + w - 1, va.y1 + y + h - 1});
}

void MapScreen::select(uint8_t kind, uint32_t id)
{
    sel_kind_ = kind;
    sel_id_ = id;
    place_markers();
}

void MapScreen::clear_selection()
{
    if (sel_kind_ == SEL_NONE)
        return;
    sel_kind_ = SEL_NONE;
    sel_id_ = 0;
    place_markers();
}

void MapScreen::step_selection(int dir)
{
    if (hits_.empty()) {
        toast(tr("Nada que elegir en esta vista"));
        return;
    }
    // del centro hacia afuera: el más cercano primero
    std::vector<const Hit *> order;
    for (auto &h : hits_)
        order.push_back(&h);
    auto d2 = [](const Hit *h) {
        double dx = h->x - CTRL_X / 2.0, dy = h->y - MAP_H / 2.0;
        return dx * dx + dy * dy + (h->edge ? 1e7 : 0);
    };
    std::stable_sort(order.begin(), order.end(), [&](const Hit *a, const Hit *b) { return d2(a) < d2(b); });
    int cur = -1;
    for (size_t i = 0; i < order.size(); i++)
        if (order[i]->kind == sel_kind_ && order[i]->id == sel_id_)
            cur = (int)i;
    int n = (int)order.size();
    int next = cur < 0 ? (dir > 0 ? 0 : n - 1) : (cur + dir + n) % n;
    select(order[next]->kind, order[next]->id);
}

void MapScreen::open_selection()
{
    uint8_t kind = sel_kind_;
    uint32_t id = sel_id_;
    if (kind == SEL_NODE)
        app_->show_node(id);
    else if (kind == SEL_WP)
        open_dialog(DLG_EDIT_POINT, id);
}

// ------------------------------------------------------------------ regla
void MapScreen::update_strip()
{
    if (!ruler_) {
        show(strip_, false);
        return;
    }
    set_bg(strip_, col::INK);
    char buf[128];
    if (rpts_.empty())
        set_text(strip_lbl_, tr("Regla: pon la cruz en el inicio y toca la regla"));
    else {
        // tramo: el que va hasta la cruz; recién marcado (la cruz sobre el punto), el último
        const RulerPt &last = rpts_.back();
        double live = haversine_m(last.lat, last.lon, clat_, clon_);
        const RulerPt *a = &last;
        double blat = clat_, blon = clon_, seg = live;
        if (live < 1 && rpts_.size() >= 2) {
            a = &rpts_[rpts_.size() - 2];
            blat = last.lat, blon = last.lon;
            seg = haversine_m(a->lat, a->lon, blat, blon);
        }
        if (seg < 1)
            snprintf(buf, sizeof(buf), "%s", tr("Regla: mueve el mapa y marca el siguiente punto"));
        else {
            int b = (int)bearing_deg(a->lat, a->lon, blat, blon);
            snprintf(buf, sizeof(buf), tr("Regla: %s \xC2\xB7 tramo %s %s %d\xC2\xB0"), format_distance((int32_t)ruler_total_m()).c_str(),
                     format_distance((int32_t)seg).c_str(), cardinal(b), b);
        }
        set_text(strip_lbl_, buf);
    }
    show(strip_, true);
}

void MapScreen::update_toolbar()
{
    if (!btn_rec_)
        return;
    char buf[48];
    snprintf(buf, sizeof(buf), wps().size() == 1 ? tr("1 guardado") : tr("%u guardados"), (unsigned)wps().size());
    set_text(pts_l2_, buf);
    snprintf(buf, sizeof(buf), saved().size() == 1 ? tr("1 guardada") : tr("%u guardadas"), (unsigned)saved().size());
    set_text(routes_l2_, buf);
    if (track().recording) {
        set_text(rec_l1_, tr("Grabando"));
        snprintf(buf, sizeof(buf), "%s \xC2\xB7 %u:%02u", format_distance(track().distance_m).c_str(), track().duration_s / 3600,
                 (track().duration_s / 60) % 60);
        set_text(rec_l2_, buf);
        set_color(rec_l1_, TRACK_COLOR);
        show(rec_dot_, true);
        ui::align(rec_l1_, LV_ALIGN_TOP_MID, 5, 1);
    } else if (track().pts.size() >= 2) { // detenida o recuperada, sin guardar
        set_text(rec_l1_, tr("Ruta"));
        set_text(rec_l2_, tr("sin guardar"));
        set_color(rec_l1_, TRACK_COLOR);
        show(rec_dot_, true);
        ui::align(rec_l1_, LV_ALIGN_TOP_MID, 5, 1);
    } else {
        set_text(rec_l1_, tr("Grabar"));
        set_text(rec_l2_, tr("ruta"));
        set_color(rec_l1_, col::INK);
        show(rec_dot_, false);
        ui::align(rec_l1_, LV_ALIGN_TOP_MID, 0, 1);
    }
    ui::align(rec_l2_, LV_ALIGN_TOP_MID, 0, 14);
    ui::align(pts_l2_, LV_ALIGN_TOP_MID, 0, 14);
    ui::align(routes_l2_, LV_ALIGN_TOP_MID, 0, 14);
}

// ------------------------------------------------------------- regla
double MapScreen::ruler_total_m() const
{
    double d = 0;
    for (size_t i = 1; i < rpts_.size(); i++)
        d += haversine_m(rpts_[i - 1].lat, rpts_[i - 1].lon, rpts_[i].lat, rpts_[i].lon);
    if (!rpts_.empty())
        d += haversine_m(rpts_.back().lat, rpts_.back().lon, clat_, clon_);
    return d;
}

void MapScreen::ruler_button()
{
    if (!ruler_) {
        ruler_ = true;
        rpts_.clear();
        set_bg(btn_ruler_, col::BG);
        lv_obj_set_style_border_width(btn_ruler_, 2, 0);
        lv_group_focus_obj(view); // las flechas mueven el mapa y Enter marca, como el botón
        redraw();
        return;
    }
    ruler_mark();
}

void MapScreen::ruler_mark()
{
    if (!rpts_.empty() && haversine_m(rpts_.back().lat, rpts_.back().lon, clat_, clon_) < 1) {
        toast(tr("Mueve el mapa para marcar el siguiente punto"));
        return;
    }
    if (rpts_.size() >= RULER_MAX) {
        toast(tr("La regla lleva 30 puntos: Esc quita el \xC3\xBAltimo"));
        return;
    }
    rpts_.push_back({clat_, clon_});
    redraw();
}

void MapScreen::ruler_undo()
{
    if (rpts_.empty()) {
        ruler_off();
        return;
    }
    rpts_.pop_back();
    redraw();
}

void MapScreen::ruler_off()
{
    if (!ruler_)
        return;
    ruler_ = false;
    rpts_.clear();
    set_bg(btn_ruler_, col::SURF);
    lv_obj_set_style_border_width(btn_ruler_, 1, 0);
    redraw();
}

// Los tramos marcados, el punteado hasta la cruz, un punto por marca y el total junto a la cruz
void MapScreen::place_ruler()
{
    show(ruler_layer_, ruler_);
    if (!ruler_)
        return;
    std::vector<lv_point_t> pts;
    std::vector<lv_point_precise_t> seg;
    for (auto &p : rpts_) {
        int32_t x, y;
        to_screen(p.lat, p.lon, x, y);
        pts.push_back({x, y});
    }
    clip_polyline(pts, seg);
    set_line(ruler_line_, ruler_vec_, seg);
    const int32_t cx = MAP_W / 2, cy = MAP_H / 2;
    if (!pts.empty()) {
        std::vector<lv_point_precise_t> live;
        clip_polyline({pts.back(), {cx, cy}}, live);
        set_line(ruler_live_, ruler_live_vec_, live);
    } else
        show(ruler_live_, false);
    while (ruler_dots_.size() < pts.size()) {
        lv_obj_t *d = box(ruler_layer_, 0, 0, 10, 10, col::SURF);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        border(d, col::INK, 2);
        lv_obj_remove_flag(d, LV_OBJ_FLAG_CLICKABLE);
        ruler_dots_.push_back(d);
    }
    for (size_t i = 0; i < ruler_dots_.size(); i++) {
        bool on = i < pts.size() && pts[i].x > -10 && pts[i].x < MAP_W + 10 && pts[i].y > -10 && pts[i].y < MAP_H + 10;
        if (on)
            lv_obj_set_pos(ruler_dots_[i], pts[i].x - 5, pts[i].y - 5);
        show(ruler_dots_[i], on);
    }
    if (!pts.empty()) {
        set_text(ruler_lbl_, format_distance((int32_t)ruler_total_m()));
        lv_obj_update_layout(ruler_lbl_);
        lv_obj_set_pos(ruler_lbl_, std::min<int32_t>(cx + 10, MAP_W - 40 - lv_obj_get_width(ruler_lbl_)), cy - 26);
        show(ruler_lbl_, true);
    } else
        show(ruler_lbl_, false);
    lv_obj_move_foreground(ruler_lbl_);
}

// ------------------------------------------------------------- paneles
void MapScreen::show_panel(int which)
{
    panel_kind_ = which;
    set_text(panel_close_lbl_, tr("Cerrar"));
    if (which == PANEL_NONE) {
        show(panel_, false);
        return;
    }
    if (which == PANEL_POINTS)
        rebuild_points_panel();
    else if (which == PANEL_ROUTES)
        rebuild_routes_panel();
    else if (which == PANEL_NAMES) {
        layer_view_ = -1;
        rebuild_names_panel();
    }
    else
        rebuild_layers_panel();
    show(panel_, true);
    lv_obj_move_foreground(panel_);
    if (lv_obj_get_child_count(panel_list_) > 0)
        lv_group_focus_obj(lv_obj_get_child(panel_list_, 0));
}

static lv_obj_t *list_row(lv_obj_t *list, lv_group_t *g, bool selected, const std::string &l1, const std::string &l2,
                          const std::string &right)
{
    lv_obj_t *r = button(list, 0, 0, SCREEN_W, 30, selected ? col::BLUE : col::SURF, 0, g);
    lv_obj_set_style_radius(r, 0, 0);
    border(r, col::LINE2, 1, LV_BORDER_SIDE_BOTTOM);
    lv_obj_set_style_bg_color(r, c(col::BG), LV_STATE_PRESSED);
    uint32_t fg = selected ? col::SURF : col::INK, fg2 = selected ? col::SURF : col::INK2;
    lv_obj_t *a = label(r, &tfield_font_14b, fg, l1.c_str());
    lv_obj_set_pos(a, 6, 0);
    lv_obj_t *b = label(r, &tfield_font_12, fg2, l2.c_str());
    lv_obj_set_pos(b, 6, 15);
    lv_obj_set_size(b, right.empty() ? SCREEN_W - 12 : SCREEN_W - 76, 14);
    lv_label_set_long_mode(b, LV_LABEL_LONG_MODE_DOTS);
    if (!right.empty()) {
        lv_obj_t *rl = label(r, &tfield_font_12b, fg, right.c_str());
        ui::align(rl, LV_ALIGN_RIGHT_MID, -6, 0);
    }
    return r;
}

static void empty_note(lv_obj_t *list, const char *text)
{
    lv_obj_t *l = label(list, &tfield_font_12, col::INK2, text);
    lv_obj_set_width(l, SCREEN_W - 12);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_pad_all(l, 6, 0);
}

void MapScreen::rebuild_points_panel()
{
    lv_obj_clean(panel_list_);
    set_text(panel_title_, tr("Puntos"));
    for (auto &w : wps()) {
        std::string l2;
        if (pos_.valid) {
            int32_t d = (int32_t)haversine_m(pos_.lat, pos_.lon, w.lat, w.lon);
            int b = (int)bearing_deg(pos_.lat, pos_.lon, w.lat, w.lon);
            l2 = format_distance(d) + " " + cardinal(b) + " \xC2\xB7 " + format_alt_diff(w.alt - pos_.alt) + " \xC2\xB7 ";
        }
        l2 += w.from.empty() ? w.time : tr("de ") + w.from;
        if (!w.note.empty())
            l2 += " \xC2\xB7 " + w.note;
        lv_obj_t *r = list_row(panel_list_, group, false, w.name, l2, "");
        lv_obj_set_user_data(r, (void *)(uintptr_t)w.id);
        lv_obj_add_event_cb(
            r, [](lv_event_t *e) {
                auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
                uint32_t id = (uint32_t)(uintptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e));
                s->show_panel(PANEL_NONE);
                s->open_dialog(DLG_EDIT_POINT, id);
            },
            LV_EVENT_CLICKED, this);
    }
    if (wps().empty())
        empty_note(panel_list_, tr("A\xC3\xBAn no hay puntos. Usa \"Marcar punto aqu\xC3\xAD\"."));
}

void MapScreen::rebuild_routes_panel()
{
    lv_obj_clean(panel_list_);
    set_text(panel_title_, tr("Rutas guardadas"));
    if (track().recording) {
        std::string l2 = format_distance(track().distance_m) + " \xC2\xB7 " + duration_text(track().duration_s) + tr(" \xC2\xB7 en curso");
        lv_obj_t *r = list_row(panel_list_, group, false, tr("Grabando ahora"), l2, tr("Detener"));
        lv_obj_add_event_cb(
            r, [](lv_event_t *e) {
                auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
                s->show_panel(PANEL_NONE);
                s->app_->stop_recording();
                s->open_dialog(DLG_ROUTE);
            },
            LV_EVENT_CLICKED, this);
    }
    for (auto it = saved().rbegin(); it != saved().rend(); ++it) {
        const SavedTrack &t = *it;
        std::string l2 = format_distance(t.track.distance_m) + " \xC2\xB7 " + duration_text(t.track.duration_s);
        if (t.start.epoch)
            l2 += " \xC2\xB7 " + app_->local_date_time(t.start.epoch);
        if (!t.note.empty())
            l2 += " \xC2\xB7 " + t.note;
        bool shown = shown_route_ == t.id;
        lv_obj_t *r = list_row(panel_list_, group, shown, t.name, l2, shown ? tr("En mapa") : "");
        lv_obj_set_user_data(r, (void *)(uintptr_t)t.id);
        lv_obj_add_event_cb(
            r, [](lv_event_t *e) {
                auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
                uint32_t id = (uint32_t)(uintptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e));
                s->show_panel(PANEL_NONE);
                s->open_dialog(DLG_EDIT_ROUTE, id);
            },
            LV_EVENT_CLICKED, this);
    }
    if (saved().empty() && !track().recording)
        empty_note(panel_list_, tr("A\xC3\xBAn no hay rutas. Usa \"Grabar ruta\"."));
}

void MapScreen::rebuild_layers_panel()
{
    lv_obj_clean(panel_list_);
    set_text(panel_title_, tr("Mapa base"));
    for (int i = 0; i <= (int)layers_.size(); i++) {
        bool none = i == (int)layers_.size();
        int idx = none ? -1 : i;
        std::string name = none ? tr("Sin mapa base") : layers_[i]->name();
        std::string l2 = none ? tr("Cuadr\xC3\xAD" "cula UTM, siempre disponible")
                              : std::string(tr("Tarjeta SD \xC2\xB7 ")) + layers_[i]->attribution();
        lv_obj_t *r = list_row(panel_list_, group, layer_ == idx, name, l2, layer_ == idx ? tr("En uso") : "");
        lv_obj_set_user_data(r, (void *)(intptr_t)idx);
        lv_obj_add_event_cb(
            r, [](lv_event_t *e) {
                auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
                s->layer_ = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e));
                s->zoom_by(0);
                s->show_panel(PANEL_NONE);
                s->redraw();
            },
            LV_EVENT_CLICKED, this);
    }
    // nodos a mostrar (el mismo ajuste que Ajustes › Mapa y puntos)
    lv_obj_t *r = list_row(panel_list_, group, false, tr("Nodos en el mapa"),
                           team_only_ ? tr("Solo tu equipo \xC2\xB7 toca para ver toda la red") : tr("Toda la red \xC2\xB7 toca para ver solo tu equipo"),
                           team_only_ ? tr("Mi equipo") : tr("Todos"));
    lv_obj_add_event_cb(
        r, [](lv_event_t *e) {
            auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
            Settings st = s->app_->settings();
            st.map_team_only = !st.map_team_only;
            s->app_->update_settings(st);
            s->show_panel(PANEL_NONE);
        },
        LV_EVENT_CLICKED, this);
    empty_note(panel_list_, tr("M\xC3\xA1s mapas: carpetas /maps/<nombre>/z/x/y.png o archivos .pmtiles en la tarjeta SD."));
}

// Capas, de arriba hacia abajo como se dibujan: Mi equipo, Otros nodos de la red, Puntos
// compartidos, Mis puntos (y el mapa base). Cada una: el ojo (se ve o no) y la etiqueta
// (nombres siempre a la vista). Tocar el nombre de una capa abre sus elementos, para marcar
// uno por uno cuáles llevan nombre aunque su capa no.
static const uint8_t LAYER_BITS[4] = {MAP_NAMES_TEAM, MAP_NAMES_OTHERS, MAP_NAMES_SHARED, MAP_NAMES_MINE};
static const char *const LAYER_NAMES[4] = {"Mi equipo", "Otros nodos de la red", "Puntos compartidos", "Mis puntos"};

// botoncito de una fila: ojo o etiqueta; lleno = activo
static lv_obj_t *layer_toggle(lv_obj_t *row, int32_t x, const lv_image_dsc_t *icon, lv_group_t *g)
{
    lv_obj_t *b = button(row, x, 5, 30, 22, col::SURF, col::LINE, g);
    lv_obj_set_style_radius(b, 3, 0);
    lv_obj_t *img = lv_image_create(b);
    lv_image_set_src(img, icon);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(img);
    return b;
}

static void toggle_look(lv_obj_t *b, bool on, bool usable = true)
{
    set_bg(b, on && usable ? col::INK : col::SURF);
    set_border_color(b, on && usable ? col::INK : col::LINE);
    lv_obj_set_style_image_recolor(lv_obj_get_child(b, 0), c(on && usable ? col::SURF : col::MUTED), 0);
}

void MapScreen::rebuild_names_panel()
{
    lv_obj_clean(panel_list_);
    names_rows_.clear();
    set_text(panel_title_, tr("Capas"));
    set_text(panel_close_lbl_, tr("Cerrar"));
    const Settings &st = app_->settings();
    // cuántos hay en cada capa
    unsigned count[4] = {0, 0, 0, 0};
    for (auto &n : team_nodes())
        if (n.has_pos)
            count[n.in_team ? 0 : 1]++;
    for (auto &w : wps())
        count[w.from.empty() ? 3 : 2]++;
    for (int i = 0; i < 4; i++) {
        bool hidden = i == 1 ? team_only_ : (st.map_hidden & LAYER_BITS[i]);
        bool named = st.map_names & LAYER_BITS[i];
        lv_obj_t *r = button(panel_list_, 0, 0, SCREEN_W, 32, col::SURF, 0, group);
        lv_obj_set_style_radius(r, 0, 0);
        border(r, col::LINE2, 1, LV_BORDER_SIDE_BOTTOM);
        lv_obj_set_style_bg_color(r, c(col::BG), LV_STATE_PRESSED);
        lv_obj_t *nm = label(r, &tfield_font_14b, hidden ? col::MUTED : col::INK, tr(LAYER_NAMES[i]));
        lv_obj_set_pos(nm, 8, 1);
        char buf[48];
        if (count[i] == 1)
            snprintf(buf, sizeof(buf), "%s", i < 2 ? tr("1 nodo") : tr("1 punto"));
        else
            snprintf(buf, sizeof(buf), i < 2 ? tr("%u nodos") : tr("%u puntos"), count[i]);
        lv_obj_t *sub = label(r, &tfield_font_12, col::INK2, buf);
        lv_obj_set_pos(sub, 8, 16);
        lv_obj_t *eye = layer_toggle(r, 222, &tfield_picto_eye, group);
        lv_obj_t *tag = layer_toggle(r, 256, &tfield_picto_tag, group);
        toggle_look(eye, !hidden);
        toggle_look(tag, named, !hidden);
        lv_obj_t *chev = label(r, &tfield_font_14b, col::INK2, "\xE2\x80\xBA");
        lv_obj_set_pos(chev, 297, 6);
        lv_obj_set_user_data(r, (void *)(intptr_t)i);
        lv_obj_set_user_data(eye, (void *)(intptr_t)i);
        lv_obj_set_user_data(tag, (void *)(intptr_t)i);
        // fila: ver sus elementos
        lv_obj_add_event_cb(
            r, [](lv_event_t *e) {
                auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
                int i = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e));
                lv_async_call([](void *p) { // la lista se rehace: fuera del evento
                    auto *pr = static_cast<std::pair<MapScreen *, int> *>(p);
                    pr->first->open_layer_items(pr->second);
                    delete pr;
                }, new std::pair<MapScreen *, int>(s, i));
            },
            LV_EVENT_CLICKED, this);
        // ojo: la capa se ve o no
        lv_obj_add_event_cb(
            eye, [](lv_event_t *e) {
                auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
                lv_obj_t *b = (lv_obj_t *)lv_event_get_current_target(e);
                int i = (int)(intptr_t)lv_obj_get_user_data(b);
                Settings st = s->app_->settings();
                bool hidden;
                if (i == 1) { // otros nodos: el mismo ajuste de Ajustes › Mapa y puntos
                    st.map_team_only = !st.map_team_only;
                    hidden = st.map_team_only;
                } else {
                    st.map_hidden ^= LAYER_BITS[i];
                    hidden = st.map_hidden & LAYER_BITS[i];
                }
                s->app_->update_settings(st);
                lv_obj_t *row = lv_obj_get_parent(b);
                toggle_look(b, !hidden);
                toggle_look(lv_obj_get_child(row, 3), st.map_names & LAYER_BITS[i], !hidden);
                set_color(lv_obj_get_child(row, 0), hidden ? col::MUTED : col::INK);
                s->update_names_button();
                s->redraw();
            },
            LV_EVENT_CLICKED, this);
        // etiqueta: nombres siempre a la vista
        lv_obj_add_event_cb(
            tag, [](lv_event_t *e) {
                auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
                lv_obj_t *b = (lv_obj_t *)lv_event_get_current_target(e);
                int i = (int)(intptr_t)lv_obj_get_user_data(b);
                Settings st = s->app_->settings();
                st.map_names ^= LAYER_BITS[i];
                s->app_->update_settings(st);
                bool hidden = i == 1 ? st.map_team_only : (st.map_hidden & LAYER_BITS[i]);
                toggle_look(b, st.map_names & LAYER_BITS[i], !hidden);
                s->update_names_button();
                s->place_markers();
            },
            LV_EVENT_CLICKED, this);
    }
    // mapa base: la misma lista que el botón de arriba
    lv_obj_t *r = list_row(panel_list_, group, false, tr("Mapa base"),
                           layer_ >= 0 && layer_ < (int)layers_.size() ? std::string(layers_[layer_]->name()) : std::string(tr("Sin mapa base")),
                           "\xE2\x80\xBA");
    lv_obj_add_event_cb(
        r, [](lv_event_t *e) {
            auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
            lv_async_call([](void *p) { static_cast<MapScreen *>(p)->show_panel(PANEL_LAYERS); }, s);
        },
        LV_EVENT_CLICKED, this);
}

// Dentro de una capa: cada elemento con su etiqueta (nombre siempre a la vista, uno por uno)
void MapScreen::open_layer_items(int layer)
{
    layer_view_ = layer;
    if (layer < 0) {
        rebuild_names_panel();
        if (lv_obj_get_child_count(panel_list_) > 0)
            lv_group_focus_obj(lv_obj_get_child(panel_list_, 0));
        return;
    }
    lv_obj_clean(panel_list_);
    names_rows_.clear();
    set_text(panel_title_, std::string(tr("Capas")) + " \xE2\x80\xBA " + tr(LAYER_NAMES[layer]));
    set_text(panel_close_lbl_, std::string("\xE2\x80\xB9 ") + tr("Capas"));
    const Settings &st = app_->settings();
    struct Item {
        uint8_t kind;
        uint32_t id;
        std::string name, detail;
        double d;
    };
    std::vector<Item> items;
    if (layer < 2) {
        for (auto &n : team_nodes())
            if (n.has_pos && n.in_team == (layer == 0)) {
                std::string detail = format_distance(n.dist_m) + " " + cardinal(n.bearing_deg) + " \xC2\xB7 " + format_ago(n.heard_ago_s);
                items.push_back({SEL_NODE, n.num, n.name, detail, (double)n.dist_m});
            }
    } else {
        for (auto &w : wps())
            if (w.from.empty() == (layer == 3)) {
                double d = pos_.valid ? haversine_m(pos_.lat, pos_.lon, w.lat, w.lon) : 0;
                std::string detail = pos_.valid ? format_distance((int32_t)d) + " " + cardinal((int)bearing_deg(pos_.lat, pos_.lon, w.lat, w.lon)) : w.time;
                if (!w.from.empty())
                    detail += " \xC2\xB7 " + std::string(tr("de ")) + w.from;
                items.push_back({SEL_WP, w.id, w.name, detail, d});
            }
    }
    std::stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return a.d < b.d; });
    if (items.size() > 40)
        items.resize(40);
    for (auto &it : items) {
        const auto &v = it.kind == SEL_NODE ? st.map_name_nodes : st.map_name_wps;
        bool on = std::find(v.begin(), v.end(), it.id) != v.end();
        lv_obj_t *r = button(panel_list_, 0, 0, SCREEN_W, 32, col::SURF, 0, nullptr);
        lv_obj_set_style_radius(r, 0, 0);
        border(r, col::LINE2, 1, LV_BORDER_SIDE_BOTTOM);
        lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *nm = label(r, &tfield_font_14b, col::INK, it.name.c_str());
        lv_obj_set_pos(nm, 8, 1);
        lv_obj_t *sub = label(r, &tfield_font_12, col::INK2, it.detail.c_str());
        lv_obj_set_pos(sub, 8, 16);
        lv_obj_set_size(sub, 240, 14);
        lv_label_set_long_mode(sub, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_t *tag = layer_toggle(r, 282, &tfield_picto_tag, group);
        toggle_look(tag, on);
        lv_obj_set_user_data(tag, (void *)(uintptr_t)names_rows_.size());
        names_rows_.push_back({it.kind, it.id});
        lv_obj_add_event_cb(
            tag, [](lv_event_t *e) {
                auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
                lv_obj_t *b = (lv_obj_t *)lv_event_get_current_target(e);
                auto [kind, id] = s->names_rows_[(size_t)(uintptr_t)lv_obj_get_user_data(b)];
                Settings st = s->app_->settings();
                auto &v = kind == SEL_NODE ? st.map_name_nodes : st.map_name_wps;
                auto f = std::find(v.begin(), v.end(), id);
                bool on = f == v.end();
                if (on) {
                    v.push_back(id);
                    if (v.size() > 64)
                        v.erase(v.begin());
                } else
                    v.erase(f);
                s->app_->update_settings(st);
                toggle_look(b, on);
                s->update_names_button();
                s->place_markers();
            },
            LV_EVENT_CLICKED, this);
    }
    if (items.empty())
        empty_note(panel_list_, tr("Esta capa no tiene elementos."));
    // foco en el primero
    for (uint32_t k = 0; k < lv_obj_get_child_count(panel_list_); k++) {
        lv_obj_t *row = lv_obj_get_child(panel_list_, (int32_t)k);
        if (lv_obj_get_child_count(row) > 2) {
            lv_group_focus_obj(lv_obj_get_child(row, 2));
            break;
        }
    }
}

// el botón de capas, lleno cuando algo se aparta de lo normal (capa oculta o con nombres)
void MapScreen::update_names_button()
{
    if (!btn_names_)
        return;
    const Settings &st = app_->settings();
    bool any = st.map_names || st.map_hidden || st.map_team_only || !st.map_name_nodes.empty() || !st.map_name_wps.empty();
    set_bg(btn_names_, any ? col::INK : col::SURF);
    lv_obj_set_style_image_recolor(names_img_, c(any ? col::SURF : col::INK), 0);
}

void MapScreen::set_team_only(bool on)
{
    if (team_only_ == on)
        return;
    team_only_ = on;
    if (screen)
        redraw();
}

void MapScreen::toast(const std::string &msg)
{
    set_text(toast_, msg);
    show(toast_, true);
    // abajo, sobre la escala; si lo elegido está abajo, arriba (bajo las coordenadas)
    bool sel_low = !lv_obj_has_flag(sel_ring_, LV_OBJ_FLAG_HIDDEN) && lv_obj_get_y(sel_ring_) > MAP_H / 2;
    if (sel_low)
        ui::align(toast_, LV_ALIGN_TOP_MID, -(MAP_W - CTRL_X) / 2, (ruler_) ? 46 : 26);
    else
        ui::align(toast_, LV_ALIGN_BOTTOM_MID, -(MAP_W - CTRL_X) / 2, -22);
    lv_obj_move_foreground(toast_);
    if (toast_timer_)
        lv_timer_delete(toast_timer_);
    toast_timer_ = lv_timer_create(
        [](lv_timer_t *t) {
            auto *s = static_cast<MapScreen *>(lv_timer_get_user_data(t));
            show(s->toast_, false);
            s->toast_timer_ = nullptr;
        },
        2500, this);
    lv_timer_set_repeat_count(toast_timer_, 1);
}

// ------------------------------------------------------------- entrada
void MapScreen::on_view(lv_event_t *e)
{
    auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *in = lv_indev_active();
    if (!in || lv_indev_get_type(in) != LV_INDEV_TYPE_POINTER)
        return;
    if (code == LV_EVENT_PRESSED) {
        s->drag_ = 0;
    } else if (code == LV_EVENT_PRESSING) {
        lv_point_t v;
        lv_indev_get_vect(in, &v);
        if (v.x || v.y) {
            s->drag_ += std::abs(v.x) + std::abs(v.y);
            s->pan(v.x, v.y);
        }
    } else if (code == LV_EVENT_CLICKED && s->drag_ < 6) {
        lv_point_t p;
        lv_indev_get_point(in, &p);
        lv_area_t a;
        lv_obj_get_coords(s->view, &a);
        int32_t x = p.x - a.x1, y = p.y - a.y1;
        if (s->ruler_)
            return; // con la regla, se marca en la cruz (botón o Enter)
        // tocar un marcador lo elige (el más cercano, a menos de 14 px); tocar fuera, nada
        const Hit *best = nullptr;
        int32_t bd = 14 * 14 + 1;
        for (auto &h : s->hits_) {
            int32_t d = (h.x - x) * (h.x - x) + (h.y - y) * (h.y - y);
            if (d < bd)
                bd = d, best = &h;
        }
        if (best)
            s->select(best->kind, best->id);
        else
            s->clear_selection();
    }
}

void MapScreen::on_key(lv_event_t *e)
{
    auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
    uint32_t k = lv_event_get_key(e);
    const int32_t step = 48;
    bool arrows = k == LV_KEY_LEFT || k == LV_KEY_RIGHT || k == LV_KEY_UP || k == LV_KEY_DOWN;
    if (arrows && !s->pan_mode()) { // sin "tomar" el mapa, ‹ › recorren los marcadores
        if (k == LV_KEY_LEFT || k == LV_KEY_RIGHT)
            s->step_selection(k == LV_KEY_RIGHT ? 1 : -1);
        return;
    }
    switch (k) {
    case LV_KEY_LEFT:
        s->pan(step, 0);
        break;
    case LV_KEY_RIGHT:
        s->pan(-step, 0);
        break;
    case LV_KEY_UP:
        s->pan(0, step);
        break;
    case LV_KEY_DOWN:
        s->pan(0, -step);
        break;
    case LV_KEY_ENTER:
        if (s->ruler_)
            s->ruler_mark(); // marca en la cruz del centro
        else if (s->sel_kind_ != SEL_NONE && !s->pan_mode_)
            s->open_selection(); // con algo elegido: su ficha
        else
            s->set_pan_mode(!s->pan_mode_); // clic: entrar o salir de "mover el mapa"
        break;
    default:
        break;
    }
}

// Con la bola, arriba/abajo recorren los botones; para mover el mapa hay que "tomarlo" con un
// clic sobre él (como el modo edición de LVGL). Clic otra vez o Esc lo suelta.
void MapScreen::set_pan_mode(bool on)
{
    pan_mode_ = on;
    lv_obj_set_style_outline_color(view, c(on ? col::BLUE : col::INK), LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(view, on ? 3 : 2, LV_STATE_FOCUS_KEY);
    if (on)
        toast(tr("Mover el mapa: bola o flechas \xC2\xB7 clic para soltar"));
}

// ------------------------------------------------------------- diálogo
static lv_obj_t *dlg_textarea(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, bool one_line, uint32_t max)
{
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_set_pos(ta, x, y);
    lv_obj_set_size(ta, w, h);
    lv_textarea_set_one_line(ta, one_line);
    lv_textarea_set_max_length(ta, max);
    set_font(ta, &tfield_font_14);
    set_color(ta, col::INK);
    lv_obj_set_style_text_color(ta, c(col::MUTED), LV_PART_TEXTAREA_PLACEHOLDER);
    set_bg(ta, col::SURF);
    set_border_color(ta, col::LINE);
    lv_obj_set_style_border_color(ta, c(col::BLUE), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_border_width(ta, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(ta, 3, 0);
    lv_obj_set_style_pad_hor(ta, 5, 0);
    lv_obj_set_style_pad_ver(ta, 3, 0);
    lv_obj_set_style_shadow_width(ta, 0, 0);
    lv_obj_set_style_outline_width(ta, 0, LV_STATE_FOCUS_KEY);
    return ta;
}

void MapScreen::build_dialog()
{
    dlg_group_ = lv_group_create();
    dlg_ = box(screen, 0, HEAD_H, SCREEN_W, SCREEN_H - HEAD_H, col::SURF);
    lv_obj_t *tb = box(dlg_, 0, 0, SCREEN_W, 22, col::BG);
    border(tb, col::LINE, 1, LV_BORDER_SIDE_BOTTOM);
    dlg_title_ = label(tb, &tfield_font_14b, col::INK, "");
    lv_obj_set_pos(dlg_title_, 6, 3);
    dlg_info_ = label(dlg_, &tfield_font_12, col::INK2, "");
    lv_obj_set_pos(dlg_info_, 6, 26);
    lv_obj_set_size(dlg_info_, SCREEN_W - 12, 14);
    lv_label_set_long_mode(dlg_info_, LV_LABEL_LONG_MODE_DOTS);

    lv_obj_set_pos(label(dlg_, &tfield_font_12b, col::INK, tr("Nombre")), 6, 44);
    dlg_name_ = dlg_textarea(dlg_, 6, 58, SCREEN_W - 12, 24, true, 24);
    limit_bytes(dlg_name_, WP_NAME_MAX_BYTES); // lo que cabe en un waypoint de Meshtastic
    lv_obj_set_pos(label(dlg_, &tfield_font_12b, col::INK, tr("Nota")), 6, 88);
    dlg_note_ = dlg_textarea(dlg_, 6, 102, SCREEN_W - 12, 48, false, 140);
    limit_bytes(dlg_note_, WP_NOTE_MAX_BYTES);
    lv_textarea_set_placeholder_text(dlg_note_, tr("Opcional"));
    lv_obj_add_event_cb(
        dlg_name_, [](lv_event_t *e) {
            auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
            if (lv_event_get_code(e) == LV_EVENT_READY)
                lv_group_focus_obj(s->dlg_note_);
            else if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED)
                s->update_dialog_hint();
        },
        LV_EVENT_ALL, this);

    dlg_hint_ = label(dlg_, &tfield_font_12, col::INK2, "");
    lv_obj_set_pos(dlg_hint_, 6, 156);
    lv_obj_set_size(dlg_hint_, SCREEN_W - 12, 14);
    lv_label_set_long_mode(dlg_hint_, LV_LABEL_LONG_MODE_DOTS);

    lv_group_add_obj(dlg_group_, dlg_name_);
    lv_group_add_obj(dlg_group_, dlg_note_);
    for (int i = 0; i < 4; i++) {
        dlg_btn_[i] = button(dlg_, 0, SCREEN_H - HEAD_H - 32, 70, 26, col::SURF, col::INK, dlg_group_);
        lv_obj_set_style_bg_color(dlg_btn_[i], c(col::BG), LV_STATE_PRESSED);
        dlg_btn_lbl_[i] = label(dlg_btn_[i], &tfield_font_12b, col::INK, "");
        lv_obj_set_user_data(dlg_btn_[i], (void *)(intptr_t)i);
        lv_obj_add_event_cb(
            dlg_btn_[i], [](lv_event_t *e) {
                auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
                s->dialog_action((int)(intptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e)));
            },
            LV_EVENT_CLICKED, this);
    }
    // el último botón (Guardar) es el principal
    set_bg(dlg_btn_[3], col::BLUE);
    set_border_color(dlg_btn_[3], col::BLUE);
    lv_obj_set_style_bg_color(dlg_btn_[3], c(col::BLUE_DK), LV_STATE_PRESSED);
    set_color(dlg_btn_lbl_[3], col::SURF);
    show(dlg_, false);
}

// Reparte el ancho entre los botones visibles ("" = oculto)
void MapScreen::set_dialog_buttons(const char *a, const char *b, const char *cc, const char *d)
{
    const char *texts[4] = {a, b, cc, d};
    int n = 0;
    for (auto *t : texts)
        n += (t && *t) ? 1 : 0;
    int32_t gap = 4, w = (SCREEN_W - 12 - gap * (n - 1)) / std::max(n, 1), x = 6;
    for (int i = 0; i < 4; i++) {
        if (texts[i] && *texts[i]) {
            set_text(dlg_btn_lbl_[i], texts[i]);
            lv_obj_set_x(dlg_btn_[i], x);
            lv_obj_set_width(dlg_btn_[i], w);
            lv_obj_center(dlg_btn_lbl_[i]);
            show(dlg_btn_[i], true);
            x += w + gap;
        } else
            show(dlg_btn_[i], false);
    }
    set_bg(dlg_btn_[0], col::SURF);
    set_color(dlg_btn_lbl_[0], col::INK);
}

void MapScreen::open_dialog(DlgKind kind, uint32_t id)
{
    dlg_kind_ = kind;
    armed_ = false;
    char buf[128];
    if (kind == DLG_NEW_POINT) {
        draft_ = app_->draft_point();
        set_text(dlg_title_, tr("Nuevo punto"));
        snprintf(buf, sizeof(buf), tr("%s \xC2\xB7 %d m \xC2\xB7 %s"), format_utm(to_utm(draft_.lat, draft_.lon)).c_str(),
                 (int)draft_.alt, draft_.time.c_str());
        set_text(dlg_info_, buf);
        lv_textarea_set_text(dlg_name_, draft_.name.c_str());
        lv_textarea_set_text(dlg_note_, "");
        set_dialog_buttons(tr("Cancelar"), tr("Compartir"), "", tr("Guardar"));
    } else if (kind == DLG_EDIT_POINT) {
        const Waypoint *w = nullptr;
        for (auto &x : app_->waypoints())
            if (x.id == id)
                w = &x;
        if (!w) {
            dlg_kind_ = DLG_NONE;
            return;
        }
        draft_ = *w;
        set_text(dlg_title_, draft_.from.empty() ? tr("Punto ") + draft_.name : tr("Punto de ") + draft_.from);
        std::string info = format_utm(to_utm(draft_.lat, draft_.lon)) + " \xC2\xB7 " + std::to_string(draft_.alt) + tr(" m");
        if (pos_.valid) {
            int32_t d = (int32_t)haversine_m(pos_.lat, pos_.lon, draft_.lat, draft_.lon);
            info += " \xC2\xB7 " + format_distance(d) + " " + cardinal((int)bearing_deg(pos_.lat, pos_.lon, draft_.lat, draft_.lon));
        }
        set_text(dlg_info_, info);
        lv_textarea_set_text(dlg_name_, draft_.name.c_str());
        lv_textarea_set_text(dlg_note_, draft_.note.c_str());
        set_dialog_buttons(tr("Borrar"), tr("Compartir"), "", tr("Guardar"));
    } else if (kind == DLG_ROUTE) {
        const Track &t = app_->track();
        snprintf(buf, sizeof(buf), tr("%s \xC2\xB7 %s \xC2\xB7 +%d m \xC2\xB7 %u puntos"), format_distance(t.distance_m).c_str(),
                 duration_text(t.duration_s).c_str(), (int)t.gain_m, (unsigned)t.pts.size());
        bool empty = t.pts.size() < 2; // sin recorrido no hay nada que guardar
        dlg_route_ = id; // 0: recién detenida (Esc sigue grabando); 1: recuperada; 2: sin guardar
        set_text(dlg_title_, id == 1 ? tr("Ruta recuperada (se apag\xC3\xB3 grabando)") : id == 2 ? tr("Ruta sin guardar") : tr("Ruta detenida"));
        set_text(dlg_info_, empty ? std::string(tr("Sin recorrido todav\xC3\xAD" "a (falta posici\xC3\xB3n GPS o moverse)")) : std::string(buf));
        lv_textarea_set_text(dlg_name_, app_->next_track_name().c_str());
        lv_textarea_set_text(dlg_note_, "");
        set_dialog_buttons(tr("Desechar"), tr("Seguir grabando"), "", empty ? "" : tr("Guardar"));
    } else if (kind == DLG_EDIT_ROUTE) {
        const SavedTrack *t = nullptr;
        for (auto &x : saved())
            if (x.id == id)
                t = &x;
        if (!t) {
            dlg_kind_ = DLG_NONE;
            return;
        }
        dlg_route_ = id;
        set_text(dlg_title_, tr("Ruta ") + t->name);
        std::string info = format_distance(t->track.distance_m) + " \xC2\xB7 " + duration_text(t->track.duration_s) +
                           " \xC2\xB7 +" + std::to_string(t->track.gain_m) + tr(" m");
        if (pos_.valid && t->count) {
            const TrackPoint &s0 = t->start;
            info += tr(" \xC2\xB7 inicio a ") + format_distance((int32_t)haversine_m(pos_.lat, pos_.lon, s0.lat, s0.lon));
        }
        set_text(dlg_info_, info);
        lv_textarea_set_text(dlg_name_, t->name.c_str());
        lv_textarea_set_text(dlg_note_, t->note.c_str());
        set_dialog_buttons(tr("Borrar"), shown_route_ == id ? tr("Ocultar") : tr("Mostrar"), "", tr("Guardar"));
    }
    update_dialog_hint();
    show(dlg_, true);
    lv_obj_move_foreground(dlg_);
    lv_group_focus_obj(dlg_name_);
    app_->refresh_input_group();
}

void MapScreen::update_dialog_hint()
{
    std::string name = lv_textarea_get_text(dlg_name_);
    if (dlg_kind_ == DLG_ROUTE || dlg_kind_ == DLG_EDIT_ROUTE)
        set_text(dlg_hint_, tr("Archivo en la tarjeta SD: /tfield/rutas/") + safe_filename(name) + tr(".gpx"));
    else if (dlg_kind_ != DLG_NONE)
        set_text(dlg_hint_, tr("Se guardan en la tarjeta SD: /tfield/puntos.gpx"));
}

void MapScreen::close_dialog()
{
    dlg_kind_ = DLG_NONE;
    show(dlg_, false);
    app_->refresh_input_group();
    redraw();
}

static void arm(lv_obj_t *btn, lv_obj_t *lbl, const char *text)
{
    set_text(lbl, text);
    lv_obj_center(lbl);
    set_bg(btn, col::RED);
    set_color(lbl, col::SURF);
}

void MapScreen::dialog_action(int which)
{
    std::string name = lv_textarea_get_text(dlg_name_), note = lv_textarea_get_text(dlg_note_);
    while (!name.empty() && name.back() == ' ')
        name.pop_back();

    switch (dlg_kind_) {
    case DLG_NEW_POINT:
        if (which == 0) {
            close_dialog();
            return;
        }
        draft_.name = name.empty() ? draft_.name : name;
        draft_.note = note;
        app_->add_waypoint(draft_);
        close_dialog();
        if (which == 1)
            open_share(draft_.id, true); // elegir a quién
        else
            toast(tr("Punto ") + draft_.name + tr(" guardado"));
        return;

    case DLG_EDIT_POINT:
        if (which == 0) {
            if (!armed_) {
                armed_ = true;
                arm(dlg_btn_[0], dlg_btn_lbl_[0], tr("\xC2\xBF" "Borrar?"));
                return;
            }
            app_->delete_waypoint(draft_.id);
            close_dialog();
            toast(tr("Punto borrado"));
            return;
        }
        app_->update_waypoint(draft_.id, name, note);
        if (which == 1) {
            close_dialog();
            open_share(draft_.id, false);
        } else {
            close_dialog();
            toast(tr("Cambios guardados"));
        }
        return;

    case DLG_ROUTE:
        if (which == 0) {
            if (!armed_) {
                armed_ = true;
                arm(dlg_btn_[0], dlg_btn_lbl_[0], tr("\xC2\xBF" "Desechar?"));
                return;
            }
            app_->discard_track();
            close_dialog();
            toast(tr("Ruta desechada"));
        } else if (which == 1) {
            app_->resume_recording();
            close_dialog();
        } else {
            std::string file = app_->save_track(name.empty() ? app_->next_track_name() : name, note);
            if (file.empty()) { // sin tarjeta SD: la ruta sigue en memoria, se puede reintentar
                toast(tr("No se pudo escribir en la tarjeta SD \xC2\xB7 la ruta no se perdi\xC3\xB3"));
                return;
            }
            close_dialog();
            toast(tr("Ruta guardada: ") + file.substr(file.rfind('/') + 1));
        }
        return;

    case DLG_EDIT_ROUTE: {
        uint32_t id = dlg_route_;
        if (which == 0) {
            if (!armed_) {
                armed_ = true;
                arm(dlg_btn_[0], dlg_btn_lbl_[0], tr("\xC2\xBF" "Borrar?"));
                return;
            }
            app_->delete_track(id);
            close_dialog();
            toast(tr("Ruta borrada"));
            return;
        }
        if (!app_->update_track(id, name, note)) {
            toast(tr("No se pudo escribir en la tarjeta SD"));
            return;
        }
        if (which == 1) {
            close_dialog();
            if (shown_route_ == id) {
                shown_route_ = 0;
                redraw();
            } else
                show_route(id); // la encuadra: puede estar lejos
        } else {
            close_dialog();
            toast(tr("Cambios guardados"));
        }
        return;
    }
    default:
        return;
    }
}

// ------------------------------------------------------------- compartir
void MapScreen::build_share()
{
    share_group_ = lv_group_create();
    share_ = box(screen, 0, HEAD_H, SCREEN_W, SCREEN_H - HEAD_H, col::SURF);
    lv_obj_t *tb = box(share_, 0, 0, SCREEN_W, 22, col::BG);
    border(tb, col::LINE, 1, LV_BORDER_SIDE_BOTTOM);
    share_title_ = label(tb, &tfield_font_14b, col::INK, "");
    lv_obj_set_pos(share_title_, 6, 3);
    lv_obj_set_size(share_title_, SCREEN_W - 12, 16);
    lv_label_set_long_mode(share_title_, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_t *hint = label(share_, &tfield_font_12, col::INK2, tr("Elige uno o m\xC3\xA1s destinatarios"));
    lv_obj_set_pos(hint, 6, 26);
    share_list_ = box(share_, 0, 42, SCREEN_W, SCREEN_H - HEAD_H - 42 - 38, col::SURF);
    border(share_list_, col::LINE2, 1, (lv_border_side_t)(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM));
    lv_obj_add_flag(share_list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(share_list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(share_list_, LV_SCROLLBAR_MODE_ACTIVE);

    int32_t by = SCREEN_H - HEAD_H - 32;
    lv_obj_t *cancel = button(share_, 6, by, 100, 26, col::SURF, col::INK, nullptr);
    lv_obj_set_style_bg_color(cancel, c(col::BG), LV_STATE_PRESSED);
    lv_obj_center(label(cancel, &tfield_font_12b, col::INK, tr("Cancelar")));
    lv_obj_add_event_cb(
        cancel, [](lv_event_t *e) { static_cast<MapScreen *>(lv_event_get_user_data(e))->close_share(false); },
        LV_EVENT_CLICKED, this);
    share_send_ = button(share_, 112, by, SCREEN_W - 118, 26, col::BLUE, col::BLUE, nullptr);
    lv_obj_set_style_bg_color(share_send_, c(col::BLUE_DK), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(share_send_, c(col::LINE2), LV_STATE_DISABLED);
    lv_obj_set_style_border_color(share_send_, c(col::LINE), LV_STATE_DISABLED);
    share_send_lbl_ = label(share_send_, &tfield_font_12b, col::SURF, "");
    lv_obj_center(share_send_lbl_);
    lv_obj_add_event_cb(
        share_send_, [](lv_event_t *e) {
            auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
            if (!lv_obj_has_state(s->share_send_, LV_STATE_DISABLED))
                s->close_share(true);
        },
        LV_EVENT_CLICKED, this);
    show(share_, false);
    // el orden de foco se arma al abrir (filas, luego botones)
    lv_obj_set_user_data(share_, cancel);
}

void MapScreen::open_share(uint32_t wp_id, bool just_created)
{
    share_wp_ = wp_id;
    share_new_ = just_created;
    std::string wname;
    for (auto &w : app_->waypoints())
        if (w.id == wp_id)
            wname = w.name;
    set_text(share_title_, tr("Compartir \xC2\xAB") + wname + tr("\xC2\xBB con\xE2\x80\xA6"));

    share_to_ = app_->share_recipients();
    // "Todo mi equipo": primera fila, marca a todos los miembros de una vez
    if (app_->settings().share_all_team) {
        std::string names;
        int n = 0;
        for (auto &r : share_to_)
            if (r.team) {
                names += (names.empty() ? "" : ", ") + r.name;
                n++;
            }
        if (n > 1) {
            Recipient all{"@team", tr("Todo mi equipo (") + std::to_string(n) + ")", false, 0, col::PURPLE,
                          names + tr(" \xC2\xB7 directo a cada uno")};
            share_to_.insert(share_to_.begin(), all);
        }
    }
    share_sel_.assign(share_to_.size(), false);
    for (size_t i = 0; i < share_to_.size(); i++)
        for (auto &id : app_->last_share())
            if (id == share_to_[i].id)
                share_sel_[i] = true; // recuerda la última elección

    lv_obj_clean(share_list_);
    share_rows_.clear();
    lv_group_remove_all_objs(share_group_);
    for (size_t i = 0; i < share_to_.size(); i++) {
        const Recipient &r = share_to_[i];
        lv_obj_t *row = button(share_list_, 0, 0, SCREEN_W, 32, col::SURF, 0, share_group_);
        lv_obj_set_style_radius(row, 0, 0);
        border(row, col::LINE2, 1, LV_BORDER_SIDE_BOTTOM);
        lv_obj_set_style_bg_color(row, c(col::BG), LV_STATE_PRESSED);
        // casilla
        lv_obj_t *cb = box(row, 8, 8, 16, 16, col::SURF);
        border(cb, col::INK, 2);
        lv_obj_set_style_radius(cb, 3, 0);
        // el tic, con líneas: no depende de que el firmware traiga la fuente de símbolos
        static const lv_point_precise_t tick_pts[] = {{2, 6}, {5, 9}, {10, 2}};
        lv_obj_t *tick = lv_line_create(cb);
        lv_line_set_points(tick, tick_pts, 3);
        lv_obj_set_style_line_width(tick, 2, 0);
        lv_obj_set_style_line_rounded(tick, true, 0);
        lv_obj_set_style_line_color(tick, c(col::SURF), 0);
        lv_obj_center(tick);
        // identidad: canal (#) o color del nodo
        lv_obj_t *dot = box(row, 32, 12, 8, 8, r.channel ? (r.color ? r.color : col::INK2) : (r.color ? r.color : col::INK));
        lv_obj_set_style_radius(dot, r.channel ? 1 : LV_RADIUS_CIRCLE, 0);
        lv_obj_t *n = label(row, &tfield_font_14b, col::INK, r.name.c_str());
        lv_obj_set_pos(n, 46, 1);
        lv_obj_t *d = label(row, &tfield_font_12, col::INK2, r.detail.c_str());
        lv_obj_set_pos(d, 46, 17);
        lv_obj_set_size(d, SCREEN_W - 52, 14);
        lv_label_set_long_mode(d, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_user_data(row, (void *)(intptr_t)i);
        lv_obj_add_event_cb(
            row, [](lv_event_t *e) {
                auto *s = static_cast<MapScreen *>(lv_event_get_user_data(e));
                int i = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e));
                if (s->share_to_[i].id == "@team") { // todos o ninguno
                    bool all = true;
                    for (size_t k = 0; k < s->share_to_.size(); k++)
                        if (s->share_to_[k].team && !s->share_sel_[k])
                            all = false;
                    for (size_t k = 0; k < s->share_to_.size(); k++)
                        if (s->share_to_[k].team)
                            s->share_sel_[k] = !all;
                } else
                    s->share_sel_[i] = !s->share_sel_[i];
                s->refresh_share();
            },
            LV_EVENT_CLICKED, this);
        share_rows_.push_back(row);
    }
    lv_group_add_obj(share_group_, static_cast<lv_obj_t *>(lv_obj_get_user_data(share_))); // Cancelar
    lv_group_add_obj(share_group_, share_send_);
    refresh_share();
    share_open_ = true;
    show(share_, true);
    lv_obj_move_foreground(share_);
    if (!share_rows_.empty())
        lv_group_focus_obj(share_rows_[0]);
    app_->refresh_input_group();
}

void MapScreen::refresh_share()
{
    int n = 0;
    bool all_team = true, any_team = false;
    for (size_t i = 0; i < share_to_.size(); i++)
        if (share_to_[i].team) {
            all_team &= share_sel_[i];
            any_team = true;
        }
    for (size_t i = 0; i < share_rows_.size(); i++) {
        lv_obj_t *cb = lv_obj_get_child(share_rows_[i], 0);
        bool group_row = share_to_[i].id == "@team";
        if (group_row)
            share_sel_[i] = any_team && all_team;
        bool on = share_sel_[i];
        n += on && !group_row ? 1 : 0;
        set_bg(cb, on ? col::BLUE : col::SURF);
        set_border_color(cb, on ? col::BLUE : col::INK);
        set_bg(share_rows_[i], on ? col::BLUE_BG : col::SURF);
    }
    if (n == 0) {
        set_text(share_send_lbl_, tr("Elige al menos uno"));
        set_color(share_send_lbl_, col::MUTED);
        lv_obj_add_state(share_send_, LV_STATE_DISABLED);
    } else {
        set_text(share_send_lbl_, n == 1 ? tr("Enviar a 1") : tr("Enviar a ") + std::to_string(n));
        set_color(share_send_lbl_, col::SURF);
        lv_obj_remove_state(share_send_, LV_STATE_DISABLED);
    }
    lv_obj_center(share_send_lbl_);
}

void MapScreen::close_share(bool sent)
{
    std::string wname, names;
    for (auto &w : app_->waypoints())
        if (w.id == share_wp_)
            wname = w.name;
    if (sent) {
        std::vector<Recipient> to;
        for (size_t i = 0; i < share_to_.size(); i++)
            if (share_sel_[i] && share_to_[i].id != "@team")
                to.push_back(share_to_[i]);
        names = app_->share_waypoint(share_wp_, to);
    }
    share_open_ = false;
    show(share_, false);
    app_->refresh_input_group();
    redraw();
    if (sent)
        toast(wname + tr(" enviado a ") + names);
    else if (share_new_)
        toast(tr("Punto ") + wname + tr(" guardado, sin compartir"));
}

bool MapScreen::handle_esc()
{
    if (share_open_) {
        close_share(false);
        return true;
    }
    if (dlg_kind_ == DLG_ROUTE) { // Esc nunca borra una ruta
        if (dlg_route_ == 0)
            app_->resume_recording(); // recién detenida: sigue grabando
        close_dialog();
        return true;
    }
    if (dlg_kind_ != DLG_NONE) {
        close_dialog();
        return true;
    }
    if (panel_kind_ == PANEL_NAMES && layer_view_ >= 0) { // dentro de una capa: a la lista
        open_layer_items(-1);
        return true;
    }
    if (panel_kind_) {
        show_panel(PANEL_NONE);
        return true;
    }
    if (ruler_) {
        ruler_undo();
        return true;
    }
    if (pan_mode_) {
        set_pan_mode(false);
        return true;
    }
    if (sel_kind_ != SEL_NONE) {
        clear_selection();
        return true;
    }
    return false;
}

MapScreen::State MapScreen::save_state() const
{
    State s;
    s.layers = layers_;
    s.layer = layer_;
    s.clat = clat_, s.clon = clon_, s.zoom = zoom_;
    s.follow = follow_, s.team_only = team_only_;
    s.shown_route = shown_route_, s.highlight_wp = highlight_wp_, s.trail_node = trail_node_;
    return s;
}

void MapScreen::restore_state(const State &s)
{
    layers_ = s.layers;
    layer_ = s.layer;
    clat_ = s.clat, clon_ = s.clon, zoom_ = s.zoom;
    follow_ = s.follow, team_only_ = s.team_only;
    shown_route_ = s.shown_route, highlight_wp_ = s.highlight_wp, trail_node_ = s.trail_node;
    update_center_button();
    update_toolbar();
    dirty_ = true;
}

void MapScreen::release(std::vector<lv_group_t *> &groups)
{
    if (toast_timer_) {
        lv_timer_delete(toast_timer_);
        toast_timer_ = nullptr;
    }
    groups.push_back(group);
    groups.push_back(dlg_group_);
    groups.push_back(share_group_);
    // los trozos de línea usan estos estilos: se sueltan cuando se borren los objetos
    for (Poly *p : {&track_poly_, &route_poly_, &trail_poly_}) {
        p->parts.clear();
        lv_style_reset(&p->style); // sus propiedades se liberan; los trozos viejos se borran enseguida
    }
}

} // namespace tfield
