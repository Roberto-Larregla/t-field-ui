// Pantalla de ajustes: cuadrícula de 9 secciones y una página por sección.
// Cada fila es un interruptor, una opción (‹ valor ›), un texto o una acción.
// Las opciones que dependen de "mi equipo" llevan una barra morada a la izquierda.
#include "tfield/app.h"
#include "tfield/i18n.h"
#include "tfield/format.h"
#include "tfield/ui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <utility>

namespace tfield {

using namespace ui;

static constexpr int32_t HEAD_H = 28;
static constexpr int32_t BODY_H = SCREEN_H - HEAD_H;
static constexpr int32_t TILE_W = 100, TILE_H = 64;

static const char *SECTION_NAMES[SettingsScreen::S_COUNT] = {
    "Pantalla", "GPS y posici\xC3\xB3n", "Mensajes", "Mapa y puntos", "Canal privado", "SOS", "Nodo y hora", "Avanzado"};
// en la cuadrícula, nombres cortos (100 px por casilla)
static const char *TILE_NAMES[SettingsScreen::S_COUNT] = {"Pantalla", "GPS", "Mensajes", "Mapa", "Canal", "SOS",
                                                          "Nodo y hora", "Avanzado"};

// ------------------------------------------------------------ ayudas
// siguiente/anterior de una lista ordenada; si el valor actual no está (p. ej. 70 %), el
// paso lleva al vecino más cercano en esa dirección (70 → 80 o 70 → 60)
template <typename T> static T pick(const std::vector<T> &v, T cur, int dir, bool wrap)
{
    int n = (int)v.size();
    for (int k = 0; k < n; k++)
        if (v[k] == cur) {
            int j = k + dir;
            j = wrap ? ((j % n) + n) % n : std::clamp(j, 0, n - 1);
            return v[j];
        }
    if (dir > 0) {
        for (int k = 0; k < n; k++)
            if (v[k] > cur)
                return v[k];
        return wrap ? v[0] : v[n - 1];
    }
    for (int k = n - 1; k >= 0; k--)
        if (v[k] < cur)
            return v[k];
    return wrap ? v[n - 1] : v[0];
}

static std::string secs_text(uint32_t s)
{
    if (s == 0)
        return tr("Nunca");
    if (s < 60)
        return std::to_string(s) + tr(" s");
    if (s < 3600)
        return std::to_string(s / 60) + tr(" min");
    return std::to_string(s / 3600) + tr(" h");
}

static std::string pct(int v) { return std::to_string(v) + " %"; }

static const char *policy_name(PointPolicy p)
{
    switch (p) {
    case PointPolicy::Auto: return tr("Guardar solos");
    case PointPolicy::Ask: return tr("Preguntar");
    default: return tr("Ignorar");
    }
}

static std::string trim(std::string s)
{
    while (!s.empty() && s.back() == ' ')
        s.pop_back();
    while (!s.empty() && s.front() == ' ')
        s.erase(s.begin());
    return s;
}

// --------------------------------------------------------------- creación
void SettingsScreen::create(App *app)
{
    app_ = app;
    screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen);
    set_bg(screen, col::SURF);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    group = lv_group_create();

    lv_obj_t *h = header_bar(screen, app, group, tr("Ajustes"));
    esc_ = lv_obj_get_child(h, 0);
    title_ = lv_obj_get_child(h, 1);

    // Cuadrícula de 3 columnas: cada sección con dos líneas de su estado actual
    grid_ = box(screen, 0, HEAD_H, SCREEN_W, BODY_H, col::BG);
    for (int i = 0; i < S_COUNT; i++) {
        int32_t x = 6 + (i % 3) * (TILE_W + 4), y = 5 + (i / 3) * (TILE_H + 5);
        tiles_[i] = button(grid_, x, y, TILE_W, TILE_H, col::SURF, col::LINE, nullptr);
        lv_obj_set_style_radius(tiles_[i], 4, 0);
        lv_obj_set_style_bg_color(tiles_[i], c(col::BG), LV_STATE_PRESSED);
        lv_obj_t *t = label(tiles_[i], &tfield_font_14b, col::INK, tr(TILE_NAMES[i]));
        lv_obj_set_pos(t, 7, 5);
        lv_obj_set_size(t, TILE_W - 12, 17);
        lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_DOTS);
        tile_l1_[i] = label(tiles_[i], &tfield_font_12, col::INK2, "");
        lv_obj_set_pos(tile_l1_[i], 7, 26);
        tile_l2_[i] = label(tiles_[i], &tfield_font_12, col::INK2, "");
        lv_obj_set_pos(tile_l2_[i], 7, 42);
        for (lv_obj_t *l : {tile_l1_[i], tile_l2_[i]}) {
            lv_obj_set_size(l, TILE_W - 12, 15);
            lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
        }
        lv_obj_set_user_data(tiles_[i], (void *)(intptr_t)i);
        lv_obj_add_event_cb(
            tiles_[i], [](lv_event_t *e) {
                auto *s = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
                int i = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e));
                s->app_->show_settings_section(i); // queda en el historial: Esc vuelve a la cuadrícula
            },
            LV_EVENT_CLICKED, this);
    }

    page_ = box(screen, 0, HEAD_H, SCREEN_W, BODY_H, col::SURF);
    lv_obj_add_flag(page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(page_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(page_, LV_SCROLLBAR_MODE_ACTIVE);
    show(page_, false);
    rows_.reserve(40);
    show_home();
}

SettingsScreen::Row &SettingsScreen::add(Kind k, const char *title, const char *desc, bool team)
{
    rows_.emplace_back();
    Row &r = rows_.back();
    r.kind = k;
    r.team = team;
    if (desc && *desc) // segunda línea fija (se puede reemplazar con r.detail)
        r.detail = [d = std::string(desc)] { return d; };
    intptr_t idx = (intptr_t)rows_.size() - 1;
    if (k == NOTE) {
        r.obj = box(page_, 0, 0, SCREEN_W, 10, col::SURF);
        lv_obj_set_height(r.obj, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_ver(r.obj, 6, 0);
        r.title = label(r.obj, &tfield_font_12, col::INK2, title);
        lv_obj_set_width(r.title, SCREEN_W - 20);
        lv_obj_set_x(r.title, 10);
        lv_label_set_long_mode(r.title, LV_LABEL_LONG_MODE_WRAP);
        return r;
    }
    bool focusable = k == TOGGLE || k == CHOICE || k == ACTION;
    int32_t h = k == MEMBER ? 22 : k == CAPTION ? 20 : (*desc || k == TEXT ? 34 : 24);
    if (focusable) {
        r.obj = button(page_, 0, 0, SCREEN_W, h, col::SURF, 0, nullptr);
        lv_obj_set_style_radius(r.obj, 0, 0);
        lv_obj_set_style_bg_color(r.obj, c(col::BG), LV_STATE_PRESSED);
    } else
        r.obj = box(page_, 0, 0, SCREEN_W, h, k == CAPTION ? col::BG : col::SURF);
    border(r.obj, col::LINE2, 1, LV_BORDER_SIDE_BOTTOM);
    lv_obj_set_user_data(r.obj, (void *)idx);
    if (team) { // depende de "mi equipo"
        lv_obj_t *bar = box(r.obj, 0, 0, 3, h - 1, col::PURPLE);
        lv_obj_set_height(bar, LV_PCT(100));
    }
    if (k == CAPTION) {
        r.title = label(r.obj, &tfield_font_12b, col::INK2, title);
        lv_obj_set_pos(r.title, 10, 3);
        return r;
    }
    if (k == MEMBER) {
        r.knob = box(r.obj, 12, 7, 8, 8, col::INK);
        lv_obj_set_style_radius(r.knob, LV_RADIUS_CIRCLE, 0);
        r.title = label(r.obj, &tfield_font_14b, col::INK, title);
        lv_obj_set_pos(r.title, 26, 2);
        r.value = label(r.obj, &tfield_font_12b, col::INK2, "");
        return r;
    }
    r.title = label(r.obj, &tfield_font_14b, col::INK, title);
    lv_obj_set_pos(r.title, 10, 2);
    lv_label_set_long_mode(r.title, LV_LABEL_LONG_MODE_DOTS);
    r.desc = label(r.obj, &tfield_font_12, col::INK2, desc);
    lv_obj_set_pos(r.desc, 10, 18);
    lv_obj_set_size(r.desc, SCREEN_W - 20, 14);
    lv_label_set_long_mode(r.desc, LV_LABEL_LONG_MODE_DOTS);
    switch (k) {
    case TOGGLE:
        r.pill = box(r.obj, SCREEN_W - 10 - 30, 3, 30, 16, col::LINE2);
        lv_obj_set_style_radius(r.pill, 8, 0);
        r.knob = box(r.pill, 2, 2, 12, 12, col::SURF);
        lv_obj_set_style_radius(r.knob, LV_RADIUS_CIRCLE, 0);
        break;
    case CHOICE:
        r.value = label(r.obj, &tfield_font_14b, col::BLUE, "");
        break;
    case ACTION:
        r.pill = box(r.obj, 0, 2, 10, 18, col::SURF);
        lv_obj_set_size(r.pill, LV_SIZE_CONTENT, 18);
        lv_obj_set_style_pad_hor(r.pill, 8, 0);
        lv_obj_set_style_radius(r.pill, 3, 0);
        border(r.pill, col::BLUE, 1);
        r.value = label(r.pill, &tfield_font_12b, col::BLUE, "");
        ui::align(r.value, LV_ALIGN_LEFT_MID, 0, 0);
        break;
    case STATIC:
        r.value = label(r.obj, &tfield_font_12b, col::INK2, "");
        break;
    case TEXT: {
        lv_obj_t *ta = lv_textarea_create(r.obj);
        lv_obj_set_size(ta, 90, 22);
        lv_textarea_set_one_line(ta, true);
        set_font(ta, &tfield_font_14);
        set_color(ta, col::INK);
        set_bg(ta, col::SURF);
        set_border_color(ta, col::LINE);
        lv_obj_set_style_border_color(ta, c(col::BLUE), LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(ta, 1, 0);
        lv_obj_set_style_radius(ta, 3, 0);
        lv_obj_set_style_pad_hor(ta, 4, 0);
        lv_obj_set_style_pad_ver(ta, 2, 0);
        lv_obj_set_style_shadow_width(ta, 0, 0);
        lv_obj_set_style_outline_width(ta, 0, LV_STATE_FOCUS_KEY);
        lv_obj_add_flag(ta, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
        lv_obj_set_user_data(ta, (void *)idx);
        lv_obj_add_event_cb(ta, on_text, LV_EVENT_READY, this);
        lv_obj_add_event_cb(ta, on_text, LV_EVENT_DEFOCUSED, this);
        r.value = ta;
        break;
    }
    default:
        break;
    }
    if (focusable) {
        lv_obj_add_event_cb(r.obj, on_row, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(r.obj, on_row_key, LV_EVENT_KEY, this);
        lv_obj_add_event_cb(
            r.obj, [](lv_event_t *e) { // al salir de la fila, una confirmación pendiente se cancela
                auto *s = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
                intptr_t i = (intptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e));
                if (i < (intptr_t)s->rows_.size() && s->rows_[i].armed) {
                    s->rows_[i].armed = false;
                    s->refresh_rows();
                }
            },
            LV_EVENT_DEFOCUSED, this);
    }
    return r;
}

// --------------------------------------------------------------- eventos
void SettingsScreen::on_row(lv_event_t *e)
{
    auto *s = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
    intptr_t i = (intptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e));
    if (i >= (intptr_t)s->rows_.size())
        return;
    Row &r = s->rows_[i];
    if (r.enabled && !r.enabled())
        return;
    for (auto &o : s->rows_)
        o.flash.clear();
    switch (r.kind) {
    case TOGGLE:
        if (r.set && !r.set(!r.get()))
            s->refresh_rows(); // no se permitió: queda el aviso en r.flash
        break;
    case CHOICE:
        if (r.step)
            r.step(+1, true);
        break;
    case ACTION:
        if (r.confirm && !r.armed) { // segunda pulsación para confirmar
            for (auto &o : s->rows_)
                o.armed = false;
            r.armed = true;
            s->refresh_rows();
            break;
        }
        r.armed = false;
        // la acción puede reconstruir la página: se ejecuta fuera del evento
        lv_async_call(
            [](void *p) {
                auto *pr = static_cast<std::pair<SettingsScreen *, intptr_t> *>(p);
                SettingsScreen *s = pr->first;
                if (pr->second < (intptr_t)s->rows_.size() && s->rows_[pr->second].act)
                    s->rows_[pr->second].act();
                s->refresh_rows();
                delete pr;
            },
            new std::pair<SettingsScreen *, intptr_t>(s, i));
        break;
    default:
        break;
    }
}

void SettingsScreen::on_row_key(lv_event_t *e)
{
    auto *s = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
    intptr_t i = (intptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_current_target(e));
    uint32_t key = lv_event_get_key(e);
    if (i >= (intptr_t)s->rows_.size())
        return;
    Row &r = s->rows_[i];
    if (r.enabled && !r.enabled())
        return;
    if (r.kind == CHOICE && r.step && (key == LV_KEY_LEFT || key == LV_KEY_RIGHT))
        r.step(key == LV_KEY_RIGHT ? +1 : -1, false); // ← → recorren las opciones sin dar la vuelta
    if (r.kind == TOGGLE && r.set && (key == LV_KEY_LEFT || key == LV_KEY_RIGHT)) {
        bool want = key == LV_KEY_RIGHT;
        if (want != r.get() && !r.set(want))
            s->refresh_rows();
    }
}

void SettingsScreen::on_text(lv_event_t *e)
{
    auto *s = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_current_target(e);
    intptr_t i = (intptr_t)lv_obj_get_user_data(ta);
    if (i >= (intptr_t)s->rows_.size())
        return;
    Row &r = s->rows_[i];
    std::string v = trim(lv_textarea_get_text(ta));
    if (r.commit && (!r.current || v != r.current()))
        r.commit(v);
    if (lv_event_get_code(e) == LV_EVENT_READY)
        lv_group_focus_next(s->group);
    s->refresh_rows();
}

void SettingsScreen::commit_texts()
{
    for (auto &r : rows_)
        if (r.kind == TEXT && r.commit && r.value) {
            std::string v = trim(lv_textarea_get_text(r.value));
            if (!r.current || v != r.current())
                r.commit(v);
        }
}

void SettingsScreen::apply(const Settings &s) { app_->update_settings(s); }

// ------------------------------------------------------------ navegación
void SettingsScreen::show_home()
{
    commit_texts();
    section_ = -1;
    rows_.clear();
    lv_obj_clean(page_);
    show(page_, false);
    show(grid_, true);
    set_text(title_, tr("Ajustes"));
    refresh_tiles();
    rebuild_group();
}

void SettingsScreen::show_section(int sec)
{
    commit_texts();
    if (sec < 0 || sec >= S_COUNT) {
        show_home();
        return;
    }
    if (sec == S_RADIO) { // cada visita parte de lo que está aplicado
        p_region_ = app_->settings().region;
        p_preset_ = app_->settings().preset;
        p_hops_ = app_->settings().hops;
    }
    section_ = sec;
    show(grid_, false);
    show(page_, true);
    set_text(title_, std::string(tr("Ajustes \xE2\x80\xBA ")) + tr(SECTION_NAMES[sec]));
    build_section(sec);
    lv_obj_scroll_to_y(page_, 0, LV_ANIM_OFF);
}

void SettingsScreen::build_section(int sec)
{
    rows_.clear();
    lv_obj_clean(page_);
    switch (sec) {
    case S_SCREEN: sec_screen(); break;
    case S_GPS: sec_gps(); break;
    case S_MESSAGES: sec_messages(); break;
    case S_MAP: sec_map(); break;
    case S_CHANNEL: sec_channel(); break;
    case S_SOS: sec_sos(); break;
    case S_NODE: sec_node(); break;
    case S_RADIO: sec_radio(); break;
    }
    sig_ = channel_signature();
    refresh_rows();
    rebuild_group();
}

void SettingsScreen::rebuild_group(int focus_index)
{
    lv_group_remove_all_objs(group);
    lv_group_add_obj(group, esc_);
    lv_obj_t *first = nullptr, *want = nullptr;
    if (section_ < 0) {
        for (int i = 0; i < S_COUNT; i++)
            lv_group_add_obj(group, tiles_[i]);
        first = tiles_[0];
    } else {
        for (size_t i = 0; i < rows_.size(); i++) {
            Row &r = rows_[i];
            lv_obj_t *o = r.kind == TEXT ? r.value : (r.kind == TOGGLE || r.kind == CHOICE || r.kind == ACTION) ? r.obj : nullptr;
            if (!o)
                continue;
            lv_group_add_obj(group, o);
            if (!first && !lv_obj_has_state(o, LV_STATE_DISABLED))
                first = o;
            if ((int)i == focus_index)
                want = o;
        }
    }
    if (want || first)
        lv_group_focus_obj(want ? want : first);
    app_->refresh_input_group();
}

std::string SettingsScreen::channel_signature() const
{
    const Settings &st = app_->settings();
    std::string sig = st.channel.on ? "1" : "0";
    for (auto &n : app_->nodes())
        if (n.in_team)
            sig += "," + std::to_string(n.num);
    return sig;
}

void SettingsScreen::refresh()
{
    if (section_ < 0) {
        refresh_tiles();
        return;
    }
    if (section_ == S_CHANNEL && channel_signature() != sig_) {
        // cambió el canal o el equipo: otras filas; se intenta mantener el foco
        lv_obj_t *f = lv_group_get_focused(group);
        int idx = -1;
        for (size_t i = 0; i < rows_.size(); i++)
            if (rows_[i].obj == f || rows_[i].value == f)
                idx = (int)i;
        rows_.clear();
        lv_obj_clean(page_);
        sec_channel();
        sig_ = channel_signature();
        refresh_rows();
        rebuild_group(idx);
        return;
    }
    refresh_rows();
}

// --------------------------------------------------------- dibujo de filas
void SettingsScreen::refresh_rows()
{
    for (auto &r : rows_) {
        bool en = r.enabled ? r.enabled() : true;
        if (r.kind == TOGGLE || r.kind == CHOICE || r.kind == ACTION) {
            if (en)
                lv_obj_remove_state(r.obj, LV_STATE_DISABLED);
            else
                lv_obj_add_state(r.obj, LV_STATE_DISABLED);
        }
        if (r.heading)
            set_text(r.title, r.heading());
        if (r.kind == NOTE) {
            if (r.detail)
                set_text(r.title, r.detail());
            continue;
        }
        if (r.kind == CAPTION)
            continue;
        if (r.kind == MEMBER) {
            set_text(r.value, r.text ? r.text() : "");
            set_color(r.value, r.color ? r.color() : col::INK2);
            ui::align(r.value, LV_ALIGN_RIGHT_MID, -10, 0);
            continue;
        }
        set_color(r.title, en ? col::INK : col::MUTED);
        if (r.desc) {
            if (!r.flash.empty()) {
                set_text(r.desc, r.flash);
                set_color(r.desc, col::RED);
            } else {
                if (r.detail)
                    set_text(r.desc, r.detail());
                set_color(r.desc, en ? col::INK2 : col::MUTED);
            }
        }
        int32_t right = 0; // ancho del control, para que el título no se monte
        switch (r.kind) {
        case TOGGLE: {
            bool on = r.get && r.get();
            set_bg(r.pill, on ? (en ? col::BLUE : col::MUTED) : col::LINE2);
            lv_obj_set_x(r.knob, on ? 16 : 2);
            right = 30;
            break;
        }
        case CHOICE:
            set_text(r.value, "\xE2\x80\xB9 " + (r.text ? r.text() : std::string()) + " \xE2\x80\xBA");
            set_color(r.value, en ? col::BLUE : col::MUTED);
            ui::align(r.value, LV_ALIGN_TOP_RIGHT, -10, 2);
            lv_obj_update_layout(r.value);
            right = lv_obj_get_width(r.value);
            break;
        case ACTION: {
            bool armed = r.armed && en;
            set_text(r.value, armed ? tr("Confirmar") : (r.text ? r.text() : std::string()));
            uint32_t ink = !en ? col::MUTED : (armed ? col::SURF : (r.confirm ? col::RED : col::BLUE));
            set_color(r.value, ink);
            set_bg(r.pill, armed ? col::RED : col::SURF);
            set_border_color(r.pill, !en ? col::LINE : (r.confirm ? col::RED : col::BLUE));
            ui::align(r.pill, LV_ALIGN_TOP_RIGHT, -10, 3);
            lv_obj_update_layout(r.pill);
            right = lv_obj_get_width(r.pill);
            if (armed && r.desc) {
                set_text(r.desc, tr("Pulsa de nuevo para confirmar \xC2\xB7 Esc cancela"));
                set_color(r.desc, col::RED);
            }
            break;
        }
        case STATIC:
            set_text(r.value, r.text ? r.text() : "");
            ui::align(r.value, LV_ALIGN_TOP_RIGHT, -10, 4);
            lv_obj_update_layout(r.value);
            right = lv_obj_get_width(r.value);
            break;
        case TEXT:
            if (!lv_obj_has_state(r.value, LV_STATE_FOCUSED) && r.current)
                lv_textarea_set_text(r.value, r.current().c_str());
            ui::align(r.value, LV_ALIGN_TOP_RIGHT, -10, 6);
            right = lv_obj_get_width(r.value);
            break;
        default:
            break;
        }
        lv_obj_set_width(r.title, SCREEN_W - 20 - (right ? right + 8 : 0));
        // con segunda línea, fila de 34 px; sin ella, 24
        bool two = r.kind == TEXT || (r.desc && lv_label_get_text(r.desc)[0]);
        lv_obj_set_height(r.obj, two ? 34 : 24);
        if (r.kind == TEXT) { // la descripción también queda a la izquierda del cuadro
            lv_obj_set_width(r.desc, SCREEN_W - 20 - right - 8);
            lv_obj_set_y(r.title, 1);
        }
    }
}

void SettingsScreen::refresh_tiles()
{
    const Settings &s = app_->settings();
    auto set = [&](int i, const std::string &a, const std::string &b) {
        set_text(tile_l1_[i], a);
        set_text(tile_l2_[i], b);
    };
    set(S_SCREEN, tr("Brillo ") + pct(s.brightness), s.screen_off_s ? tr("Se apaga: ") + secs_text(s.screen_off_s) : tr("Siempre encendida"));
    set(S_GPS, s.gps ? tr("Env\xC3\xADo: ") + secs_text(s.pos_every_s) : tr("GPS apagado"), tr("Ruta: cada ") + secs_text(s.track_every_s));
    set(S_MESSAGES, s.team_first ? tr("Equipo arriba") : tr("Por actividad"), tr("4 resp. r\xC3\xA1pidas"));
    std::string others = policy_name(s.pts_others);
    others[0] = (char)std::tolower((unsigned char)others[0]);
    set(S_MAP, s.map_team_only ? tr("Solo mi equipo") : tr("Toda la red"), tr("Otros: ") + others);
    if (s.channel.on) {
        int joined = 0, people = 0;
        for (auto *n : app_->team_people()) {
            people++;
            const ChannelMember *m = s.member(n->num);
            joined += m && m->state == ChannelMember::Joined ? 1 : 0;
        }
        set(S_CHANNEL, s.channel.name, std::to_string(joined) + tr(" de ") + std::to_string(people) + tr(" unidos"));
    } else
        set(S_CHANNEL, tr("Sin canal"), tr("Crear uno"));
    set(S_SOS, s.sos_direct ? tr("Canal + directo") : tr("Solo canales"), tr("Cada ") + std::to_string(s.sos_every_min) + tr(" min"));
    set(S_NODE, s.short_name, tr(time_zones()[s.tz % time_zones().size()].name));
    set(S_RADIO, std::string(lora_regions()[s.region % lora_regions().size()].name) + " \xC2\xB7 " +
                     lora_presets()[s.preset % lora_presets().size()].name,
        std::to_string(s.hops) + tr(" saltos"));
    // las secciones con opciones de equipo llevan su marca morada
    for (int i : {(int)S_MESSAGES, (int)S_MAP, (int)S_CHANNEL, (int)S_SOS})
        if (lv_obj_get_child_count(tiles_[i]) == 3) {
            lv_obj_t *bar = box(tiles_[i], 0, 0, 3, TILE_H - 2, col::PURPLE);
            lv_obj_set_style_radius(bar, 0, 0);
            lv_obj_add_flag(bar, LV_OBJ_FLAG_EVENT_BUBBLE);
        }
}

// ---------------------------------------------------------------- secciones
#define ST app_->settings()
#define EDIT(expr)                                                                                                     \
    do {                                                                                                               \
        Settings s = app_->settings();                                                                                 \
        expr;                                                                                                          \
        apply(s);                                                                                                      \
    } while (0)

void SettingsScreen::sec_screen()
{
    Row &b = add(CHOICE, tr("Brillo"), tr("Lo que m\xC3\xA1s influye en la bater\xC3\xAD" "a"));
    b.text = [this] { return pct(ST.brightness); };
    b.step = [this](int d, bool w) { EDIT(s.brightness = pick<uint8_t>({20, 40, 60, 80, 100}, s.brightness, d, w)); };
    Row &o = add(CHOICE, tr("Apagar pantalla tras"), tr("Se enciende con cualquier tecla o el trackball"));
    o.text = [this] { return secs_text(ST.screen_off_s); };
    o.step = [this](int d, bool w) {
        // "Nunca" al final: es lo que más gasta
        static const std::vector<uint32_t> v = {15, 30, 60, 120, 300, 600, 100000};
        uint32_t cur = ST.screen_off_s ? ST.screen_off_s : 100000;
        uint32_t n = pick<uint32_t>(v, cur, d, w);
        EDIT(s.screen_off_s = (uint16_t)(n == 100000 ? 0 : n));
    };
    Row &k = add(TOGGLE, tr("Luz del teclado"), tr("Se apaga junto con la pantalla"));
    k.get = [this] { return ST.kbd_light; };
    k.set = [this](bool v) {
        EDIT(s.kbd_light = v);
        return true;
    };
    Row &th = add(CHOICE, tr("Tema"), "");
    th.text = [this] {
        static const char *n[] = {"Claro", "Oscuro", "Autom\xC3\xA1tico"};
        return std::string(tr(n[ST.theme % 3]));
    };
    th.detail = [this] {
        static const char *d[] = {"Para el d\xC3\xAD" "a y el sol", "No encandila de noche (no ahorra bater\xC3\xAD" "a)",
                                  "Oscuro entre la puesta y la salida del sol"};
        return std::string(tr(d[ST.theme % 3]));
    };
    th.step = [this](int d, bool w) { EDIT(s.theme = (uint8_t)pick<int>({0, 1, 2}, s.theme, d, w)); };
    Row &lg = add(CHOICE, tr("Idioma"), "");
    lg.text = [this] { return std::string(ST.lang ? "English" : "Espa\xC3\xB1ol"); };
    lg.detail = [this] { return std::string(tr("Toda la interfaz y el texto del SOS")); };
    lg.step = [this](int d, bool w) { EDIT(s.lang = (uint8_t)pick<int>({0, 1}, s.lang, d, w)); };
    add(NOTE, tr("Brillo bajo y apagado r\xC3\xA1pido son lo que m\xC3\xA1s alarga la bater\xC3\xAD" "a. La radio y el GPS gastan mucho menos que la pantalla encendida."));
}

void SettingsScreen::sec_gps()
{
    Row &g = add(TOGGLE, tr("GPS"), tr("Apagado: sin posici\xC3\xB3n, rutas ni distancias"));
    g.get = [this] { return ST.gps; };
    g.set = [this](bool v) {
        EDIT(s.gps = v);
        return true;
    };
    auto gps_on = [this] { return ST.gps; };
    Row &p = add(CHOICE, tr("Enviar mi posici\xC3\xB3n cada"), tr("A la red: tu equipo te ve en su mapa"));
    p.text = [this] { return secs_text(ST.pos_every_s); };
    p.step = [this](int d, bool w) { EDIT(s.pos_every_s = pick<uint16_t>({60, 120, 300, 600, 900, 1800, 3600}, s.pos_every_s, d, w)); };
    p.enabled = gps_on;
    Row &m = add(TOGGLE, tr("Enviar antes si me muevo"), tr("Posici\xC3\xB3n inteligente: al avanzar unos 100 m"));
    m.get = [this] { return ST.pos_smart; };
    m.set = [this](bool v) {
        EDIT(s.pos_smart = v);
        return true;
    };
    m.enabled = gps_on;
    Row &t = add(CHOICE, tr("Punto de ruta cada"), tr("Al grabar. M\xC3\xA1s seguido: ruta m\xC3\xA1s fina y m\xC3\xA1s bater\xC3\xAD" "a"));
    t.text = [this] { return secs_text(ST.track_every_s); };
    t.step = [this](int d, bool w) { EDIT(s.track_every_s = pick<uint8_t>({5, 10, 30, 60}, s.track_every_s, d, w)); };
    t.enabled = gps_on;
    add(NOTE, tr("Tu posici\xC3\xB3n viaja con la precisi\xC3\xB3n completa a tu equipo. Enviarla m\xC3\xA1s seguido ocupa m\xC3\xA1s la red."));
}

void SettingsScreen::sec_messages()
{
    Row &f = add(TOGGLE, tr("Chats de mi equipo primero"), tr("Arriba en el inicio y en Mensajes"), true);
    f.get = [this] { return ST.team_first; };
    f.set = [this](bool x) {
        EDIT(s.team_first = x);
        return true;
    };
    Row &q = add(ACTION, tr("Respuestas r\xC3\xA1pidas"), "");
    q.detail = [this] {
        std::string out;
        for (auto &t : app_->quick().text)
            out += (out.empty() ? "" : " \xC2\xB7 ") + t;
        return out;
    };
    q.text = [] { return std::string(tr("Editar")); };
    q.act = [this] { app_->show(ScreenId::QuickReplies); };
    Row &k = add(CHOICE, tr("Borrar mensajes de m\xC3\xA1s de"), tr("Se borran solos; los canales quedan"));
    k.text = [this] {
        switch (ST.msg_keep_days) {
        case 1: return std::string(tr("1 d\xC3\xAD" "a"));
        case 7: return std::string(tr("7 d\xC3\xAD" "as"));
        case 30: return std::string(tr("30 d\xC3\xAD" "as"));
        default: return std::string(tr("Nunca"));
        }
    };
    k.step = [this](int d, bool w) { EDIT(s.msg_keep_days = pick<uint8_t>({0, 1, 7, 30}, s.msg_keep_days, d, w)); };
    Row &x = add(ACTION, tr("Borrar todos los chats"), tr("Los directos desaparecen y los canales quedan vac\xC3\xAD" "os"));
    x.text = [] { return std::string(tr("Borrar")); };
    x.confirm = true;
    x.act = [this] { app_->clear_all_chats(); };
    add(NOTE, tr("Los chats fuera de tu equipo marcan sus mensajes nuevos con un n\xC3\xBAmero gris en vez de azul."));
}

void SettingsScreen::sec_map()
{
    Row &n = add(CHOICE, tr("Nodos en el mapa"), tr("Tambi\xC3\xA9n se cambia en el mapa, en el bot\xC3\xB3n de capas"), true);
    n.text = [this] { return std::string(ST.map_team_only ? tr("Mi equipo") : tr("Todos")); };
    n.step = [this](int d, bool w) {
        if (!w && ((d > 0) == ST.map_team_only))
            return; // ya está en el extremo
        EDIT(s.map_team_only = !s.map_team_only);
    };
    Row &t = add(CHOICE, tr("Puntos de mi equipo"), "", true);
    t.text = [this] { return std::string(policy_name(ST.pts_team)); };
    t.detail = [this] {
        return std::string(ST.pts_team == PointPolicy::Auto ? tr("Se agregan solos a tus puntos") : tr("Aparecen en Mensajes con \xC2\xAB" "Guardar punto\xC2\xBB"));
    };
    t.step = [this](int d, bool w) {
        if (!w && ((d > 0) == (ST.pts_team == PointPolicy::Ask)))
            return;
        EDIT(s.pts_team = s.pts_team == PointPolicy::Auto ? PointPolicy::Ask : PointPolicy::Auto);
    };
    Row &o = add(CHOICE, tr("Puntos de otros nodos"), "", true);
    o.text = [this] { return std::string(policy_name(ST.pts_others)); };
    o.detail = [this] {
        switch (ST.pts_others) {
        case PointPolicy::Auto: return std::string(tr("Se agregan solos a tus puntos"));
        case PointPolicy::Ask: return std::string(tr("Aparecen en Mensajes con \xC2\xAB" "Guardar punto\xC2\xBB"));
        default: return std::string(tr("Solo un aviso en el chat, sin guardarlos"));
        }
    };
    o.step = [this](int d, bool w) {
        static const std::vector<int> v = {(int)PointPolicy::Ask, (int)PointPolicy::Auto, (int)PointPolicy::Ignore};
        int cur = (int)ST.pts_others, i = 0;
        for (int k = 0; k < 3; k++)
            if (v[k] == cur)
                i = k;
        int j = w ? (i + d + 3) % 3 : std::clamp(i + d, 0, 2);
        EDIT(s.pts_others = (PointPolicy)v[j]);
    };
    Row &a = add(TOGGLE, tr("Compartir con \xC2\xAB" "Todo mi equipo\xC2\xBB"), tr("Primera opci\xC3\xB3n al compartir: marca a todos"), true);
    a.get = [this] { return ST.share_all_team; };
    a.set = [this](bool x) {
        EDIT(s.share_all_team = x);
        return true;
    };
}

void SettingsScreen::sec_channel()
{
    if (!ST.channel.on) {
        add(NOTE, tr("Un canal con clave propia: solo lo leen los equipos que la tienen. Un mensaje al canal llega a todo tu "
                  "equipo en un solo env\xC3\xADo, y el SOS y los puntos pueden usarlo."));
        Row &c = add(ACTION, tr("Crear canal privado"), tr("Nombre \xC2\xAB" "Equipo\xC2\xBB, se puede cambiar despu\xC3\xA9s"), true);
        c.text = [] { return std::string(tr("Crear")); };
        c.act = [this] { app_->create_channel(); };
        return;
    }
    Row &n = add(TEXT, tr("Nombre"), tr("Al cambiarlo, invitar de nuevo"));
    lv_obj_set_width(n.value, 110);
    limit_bytes(n.value, CHANNEL_NAME_MAX_BYTES);
    n.current = [this] { return ST.channel.name; };
    n.commit = [this](const std::string &v) { app_->rename_channel(v); };
    Row &k = add(STATIC, tr("Clave propia \xC2\xB7 AES-256"), "");
    k.detail = [this] { return tr("Huella ") + key_fingerprint(ST.channel.key) + tr(" \xC2\xB7 debe ser igual en cada equipo"); };
    add(CAPTION, tr("Tu equipo"));
    for (auto &node : app_->nodes()) {
        if (!node.in_team)
            continue;
        Row &m = add(MEMBER, node.name.c_str());
        set_bg(m.knob, node.color);
        uint32_t num = node.num;
        auto find = [this, num]() -> const TeamNode * {
            for (auto &x : app_->nodes())
                if (x.num == num)
                    return &x;
            return nullptr;
        };
        m.text = [this, num, find] {
            const TeamNode *x = find();
            if (!x)
                return std::string();
            if (x->relay)
                return std::string(tr("repetidor \xC2\xB7 no necesita la clave"));
            if (!x->has_pki)
                return std::string(tr("sin clave p\xC3\xBA" "blica \xC2\xB7 no se puede invitar"));
            const ChannelMember *cm = ST.member(num);
            if (cm && cm->state == ChannelMember::Joined)
                return tr("unido \xC2\xB7 ") + app_->local_hhmm(cm->epoch);
            if (cm && cm->state == ChannelMember::Invited)
                return tr("invitado ") + app_->local_hhmm(cm->epoch) + tr(" \xC2\xB7 sin respuesta");
            return std::string(tr("por invitar"));
        };
        m.color = [this, num, find] {
            const TeamNode *x = find();
            if (!x || x->relay || !x->has_pki)
                return col::MUTED;
            const ChannelMember *cm = ST.member(num);
            if (cm && cm->state == ChannelMember::Joined)
                return col::GREEN;
            if (cm && cm->state == ChannelMember::Invited)
                return col::AMBER;
            return col::INK2;
        };
    }
    Row &i = add(ACTION, tr("Invitar a mi equipo"), "", true);
    i.detail = [this] {
        int n = app_->pending_invites();
        return n ? tr("Env\xC3\xAD" "a la clave cifrada a los que faltan (") + std::to_string(n) + ")"
                 : std::string(tr("Todos los que pueden unirse ya est\xC3\xA1n"));
    };
    i.text = [] { return std::string(tr("Invitar")); };
    i.enabled = [this] { return app_->pending_invites() > 0; };
    i.act = [this] { app_->invite_team(); };
    Row &r = add(ACTION, tr("Cambiar clave"), tr("Todos tendr\xC3\xA1n que unirse de nuevo"));
    r.text = [] { return std::string(tr("Cambiar")); };
    r.confirm = true;
    r.act = [this] { app_->rekey_channel(); };
    Row &q = add(ACTION, tr("Quitar canal"), tr("Se borra el canal y su historial en este equipo"));
    q.text = [] { return std::string(tr("Quitar")); };
    q.confirm = true;
    q.act = [this] { app_->remove_channel(); };
    add(NOTE, tr("La invitaci\xC3\xB3n va por mensaje directo cifrado con la clave p\xC3\xBA" "blica de cada nodo y solo la entiende un "
              "equipo con esta interfaz. Los repetidores retransmiten el canal sin necesitar la clave."));
}

void SettingsScreen::sec_sos()
{
    // nunca dejar el SOS sin destino
    auto targets = [this](const Settings &s) {
        int n = 0;
        n += s.sos_direct && !app_->team_people().empty();
        n += s.sos_public;
        n += s.sos_team_ch && s.channel.on;
        return n;
    };
    auto guarded = [this, targets](bool Settings::*field, int row) {
        return [this, targets, field, row](bool v) {
            Settings s = app_->settings();
            s.*field = v;
            if (!v && targets(s) == 0) {
                rows_[row].flash = tr("Debe quedar al menos un destino para el SOS");
                return false;
            }
            apply(s);
            return true;
        };
    };
    Row &d = add(TOGGLE, tr("Directo a cada miembro"), tr("Con confirmaci\xC3\xB3n de que lo recibieron"), true);
    d.get = [this] { return ST.sos_direct; };
    d.set = guarded(&Settings::sos_direct, (int)rows_.size() - 1);
    Row &p = add(TOGGLE, "", tr("Cualquier nodo cercano puede verlo y ayudar"));
    p.heading = [this] {
        for (auto &ch : app_->chats())
            if (ch.is_channel && ch.channel == 0)
                return tr("Canal p\xC3\xBA" "blico (") + ch.name + ")";
        return std::string(tr("Canal p\xC3\xBA" "blico"));
    };
    p.get = [this] { return ST.sos_public; };
    p.set = guarded(&Settings::sos_public, (int)rows_.size() - 1);
    Row &t = add(TOGGLE, "", "", true);
    t.heading = [this] { return ST.channel.on ? tr("Canal privado (") + ST.channel.name + ")" : std::string(tr("Canal privado")); };
    t.detail = [this] {
        return std::string(ST.channel.on ? tr("Todo tu equipo en un solo env\xC3\xADo") : tr("Primero crea uno en Canal privado"));
    };
    t.get = [this] { return ST.sos_team_ch && ST.channel.on; };
    t.set = guarded(&Settings::sos_team_ch, (int)rows_.size() - 1);
    t.enabled = [this] { return ST.channel.on; };
    Row &e = add(CHOICE, tr("Repetir cada"), tr("Hasta que lo canceles"));
    e.text = [this] { return std::to_string(ST.sos_every_min) + tr(" min"); };
    e.step = [this](int dir, bool w) { EDIT(s.sos_every_min = pick<uint8_t>({1, 2, 5}, s.sos_every_min, dir, w)); };
    add(NOTE, tr("El SOS se activa manteniendo presionado el bot\xC3\xB3n SOS del inicio, y se cancela de la misma forma."));
}

void SettingsScreen::sec_node()
{
    Row &s = add(TEXT, tr("Nombre corto"), tr("4 letras (\xC3\xB1 o tildes ocupan 2) \xC2\xB7 mapa y mensajes"));
    lv_obj_set_width(s.value, 64);
    limit_bytes(s.value, SHORT_NAME_MAX_BYTES);
    s.current = [this] { return ST.short_name; };
    s.commit = [this](const std::string &v) {
        std::string n = utf8_truncate_bytes(v, 4);
        if (n.empty())
            return; // vacío no: se queda el anterior
        EDIT(s.short_name = n);
    };
    Row &l = add(TEXT, tr("Nombre largo"), tr("En la ficha del nodo"));
    lv_obj_set_width(l.value, 150);
    limit_bytes(l.value, LONG_NAME_MAX_BYTES);
    l.current = [this] { return ST.long_name; };
    l.commit = [this](const std::string &v) {
        if (v.empty())
            return;
        EDIT(s.long_name = utf8_truncate_bytes(v, 39));
    };
    Row &z = add(CHOICE, tr("Zona horaria"), "");
    z.text = [this] { return std::string(tr(time_zones()[ST.tz % time_zones().size()].name)); };
    z.detail = [this] { return std::string(tr(time_zones()[ST.tz % time_zones().size()].detail)); };
    z.step = [this](int d, bool w) {
        int n = (int)time_zones().size(), j = ST.tz + d;
        j = w ? (j % n + n) % n : std::clamp(j, 0, n - 1);
        EDIT(s.tz = (uint8_t)j);
    };
    Row &i = add(STATIC, tr("Este equipo"), "");
    i.detail = [this] {
        const Status &st = app_->status();
        std::string t = st.node_id.empty() ? tr("nodo") : st.node_id;
        if (!st.hw.empty())
            t += " \xC2\xB7 " + st.hw;
        if (!st.firmware.empty())
            t += tr(" \xC2\xB7 firmware ") + st.firmware;
        return t;
    };
}

void SettingsScreen::sec_radio()
{
    Row &w = add(NOTE, tr("Todos los equipos deben tener la misma regi\xC3\xB3n y preset, o dejar\xC3\xA1n de verse. Aplicar reinicia "
                       "la radio: unos segundos sin red."));
    set_bg(w.obj, col::AMBER_BG);
    set_color(w.title, col::AMBER);
    Row &r = add(CHOICE, tr("Regi\xC3\xB3n LoRa"), "");
    r.text = [this] { return std::string(lora_regions()[p_region_ % lora_regions().size()].name); };
    r.detail = [this] { return std::string(tr(lora_regions()[p_region_ % lora_regions().size()].detail)); };
    r.step = [this](int d, bool wrap) {
        int n = (int)lora_regions().size(), j = p_region_ + d;
        p_region_ = (uint8_t)(wrap ? (j % n + n) % n : std::clamp(j, 0, n - 1));
        refresh_rows();
    };
    Row &p = add(CHOICE, tr("Preset"), "");
    p.text = [this] { return std::string(lora_presets()[p_preset_ % lora_presets().size()].name); };
    p.detail = [this] { return std::string(tr(lora_presets()[p_preset_ % lora_presets().size()].detail)); };
    p.step = [this](int d, bool wrap) {
        int n = (int)lora_presets().size(), j = p_preset_ + d;
        p_preset_ = (uint8_t)(wrap ? (j % n + n) % n : std::clamp(j, 0, n - 1));
        refresh_rows();
    };
    Row &h = add(CHOICE, tr("Saltos m\xC3\xA1ximos"), tr("Veces que se retransmite un mensaje \xC2\xB7 3 por defecto"));
    h.text = [this] { return std::to_string(p_hops_); };
    h.step = [this](int d, bool wrap) {
        int j = p_hops_ + d;
        p_hops_ = (uint8_t)(wrap ? ((j - 1) % 7 + 7) % 7 + 1 : std::clamp(j, 1, 7));
        refresh_rows();
    };
    auto pending = [this] { return p_region_ != ST.region || p_preset_ != ST.preset || p_hops_ != ST.hops; };
    Row &a = add(ACTION, tr("Aplicar cambios"), "");
    a.detail = [pending] { return std::string(pending() ? tr("Cambios sin aplicar \xC2\xB7 se reinicia la radio") : tr("Sin cambios")); };
    a.text = [] { return std::string(tr("Aplicar")); };
    a.confirm = true;
    a.enabled = pending;
    a.act = [this] {
        uint8_t r = p_region_, p = p_preset_, h = p_hops_;
        EDIT(s.region = r; s.preset = p; s.hops = h);
    };
}

#undef EDIT
#undef ST

int SettingsScreen::focused_row() const
{
    lv_obj_t *f = group ? lv_group_get_focused(group) : nullptr;
    for (uint32_t i = 0; f && i < lv_group_get_obj_count(group); i++)
        if (lv_group_get_obj_by_index(group, i) == f)
            return (int)i;
    return -1;
}

void SettingsScreen::focus_row(int index)
{
    if (group && index >= 0 && index < (int)lv_group_get_obj_count(group))
        lv_group_focus_obj(lv_group_get_obj_by_index(group, (uint32_t)index));
}

} // namespace tfield
