#include "tfield/ui.h"

#include <algorithm>
#include <cstring>

namespace tfield::ui {

void keep_on_top(std::initializer_list<lv_obj_t *> objs)
{
    if (objs.size() == 0)
        return;
    lv_obj_t *parent = lv_obj_get_parent(*objs.begin());
    int32_t n = (int32_t)lv_obj_get_child_count(parent), k = (int32_t)objs.size(), i = 0;
    bool ok = n >= k;
    for (lv_obj_t *o : objs) {
        if (!ok)
            break;
        ok = lv_obj_get_child(parent, n - k + i) == o;
        i++;
    }
    if (!ok)
        for (lv_obj_t *o : objs)
            lv_obj_move_foreground(o);
}

lv_obj_t *box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t bg)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, c(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

void border(lv_obj_t *o, uint32_t color, int32_t width, lv_border_side_t side)
{
    lv_obj_set_style_border_color(o, c(color), 0);
    lv_obj_set_style_border_width(o, width, 0);
    lv_obj_set_style_border_side(o, side, 0);
}

lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *txt)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, c(color), 0);
    lv_label_set_text(l, txt);
    return l;
}

// Con LV_LABEL_LONG_MODE_DOTS, LVGL escribe "..." dentro del texto guardado: comparar con
// lv_label_get_text falla y la etiqueta se redibujaría siempre. Se guarda el último texto pedido.
static void text_cache_free(lv_event_t *e) { delete static_cast<std::string *>(lv_event_get_user_data(e)); }

static std::string *text_cache(lv_obj_t *label)
{
    uint32_t n = lv_obj_get_event_count(label);
    for (uint32_t i = 0; i < n; i++) {
        lv_event_dsc_t *d = lv_obj_get_event_dsc(label, i);
        if (lv_event_dsc_get_cb(d) == text_cache_free)
            return static_cast<std::string *>(lv_event_dsc_get_user_data(d));
    }
    auto *s = new std::string(lv_label_get_text(label));
    lv_obj_add_event_cb(label, text_cache_free, LV_EVENT_DELETE, s);
    return s;
}

void set_text(lv_obj_t *label, const char *txt)
{
    if (lv_label_get_long_mode(label) == LV_LABEL_LONG_MODE_DOTS) {
        std::string *last = text_cache(label);
        if (*last == txt)
            return;
        *last = txt;
        lv_label_set_text(label, txt);
        return;
    }
    const char *cur = lv_label_get_text(label);
    if (cur && std::strcmp(cur, txt) == 0)
        return;
    lv_label_set_text(label, txt);
}

static bool same(lv_color_t a, uint32_t hex)
{
    lv_color_t b = c(hex);
    return a.red == b.red && a.green == b.green && a.blue == b.blue;
}

void set_color(lv_obj_t *o, uint32_t color)
{
    if (!same(lv_obj_get_style_text_color(o, LV_PART_MAIN), color))
        lv_obj_set_style_text_color(o, c(color), 0);
}

void set_bg(lv_obj_t *o, uint32_t color)
{
    if (!same(lv_obj_get_style_bg_color(o, LV_PART_MAIN), color))
        lv_obj_set_style_bg_color(o, c(color), 0);
}

void set_border_color(lv_obj_t *o, uint32_t color)
{
    if (!same(lv_obj_get_style_border_color(o, LV_PART_MAIN), color))
        lv_obj_set_style_border_color(o, c(color), 0);
}

void set_font(lv_obj_t *o, const lv_font_t *font)
{
    if (lv_obj_get_style_text_font(o, LV_PART_MAIN) != font)
        lv_obj_set_style_text_font(o, font, 0);
}

void align(lv_obj_t *o, lv_align_t a, int32_t x, int32_t y)
{
    if (lv_obj_get_style_align(o, LV_PART_MAIN) != a)
        lv_obj_set_style_align(o, a, 0);
    lv_obj_set_pos(o, x, y); // LVGL ya compara x/y
}

void align_to(lv_obj_t *o, lv_obj_t *base, lv_align_t a, int32_t xo, int32_t yo)
{
    if (lv_obj_get_parent(o) != lv_obj_get_parent(base)) { // no es hermano: el de LVGL
        lv_obj_align_to(o, base, a, xo, yo);
        return;
    }
    lv_obj_update_layout(o);
    int32_t bx = lv_obj_get_x(base), by = lv_obj_get_y(base), bw = lv_obj_get_width(base), bh = lv_obj_get_height(base);
    int32_t w = lv_obj_get_width(o), h = lv_obj_get_height(o), x = 0, y = 0;
    switch (a) {
    case LV_ALIGN_OUT_RIGHT_BOTTOM: x = bx + bw + xo, y = by + bh - h + yo; break;
    case LV_ALIGN_OUT_LEFT_MID: x = bx - w + xo, y = by + (bh - h) / 2 + yo; break;
    default: x = bx + bw + xo, y = by + (bh - h) / 2 + yo; break; // OUT_RIGHT_MID
    }
    if (lv_obj_get_style_align(o, LV_PART_MAIN) != LV_ALIGN_TOP_LEFT)
        lv_obj_set_style_align(o, LV_ALIGN_TOP_LEFT, 0);
    lv_obj_set_pos(o, x, y);
}

void show(lv_obj_t *o, bool visible)
{
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN) == visible) {
        if (visible)
            lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
}

void focus_style(lv_obj_t *o)
{
    lv_obj_set_style_outline_color(o, c(col::INK), LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(o, 2, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(o, -2, LV_STATE_FOCUS_KEY);
}

lv_obj_t *button(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t bg, uint32_t border_color,
                 lv_group_t *group)
{
    lv_obj_t *b = box(parent, x, y, w, h, bg);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    // con teclado o trackball, la lista se desplaza hasta el elemento enfocado
    lv_obj_add_flag(b, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    if (border_color)
        border(b, border_color, 1);
    lv_obj_set_style_radius(b, 3, 0);
    focus_style(b);
    if (group)
        lv_group_add_obj(group, b);
    return b;
}

void limit_bytes(lv_obj_t *ta, uint32_t max_bytes)
{
    lv_textarea_set_max_length(ta, 0); // el límite es en bytes, no en caracteres
    lv_obj_add_event_cb(
        ta, [](lv_event_t *e) {
            lv_obj_t *t = (lv_obj_t *)lv_event_get_current_target(e);
            size_t max = (size_t)(uintptr_t)lv_event_get_user_data(e);
            const char *ins = (const char *)lv_event_get_param(e);
            if (!ins)
                return;
            size_t cur = std::strlen(lv_textarea_get_text(t)), add = std::strlen(ins);
            if (cur + add <= max)
                return;
            size_t room = cur >= max ? 0 : max - cur;
            while (room > 0 && ((unsigned char)ins[room] & 0xC0) == 0x80)
                room--; // sin cortar un carácter por la mitad
            static char keep[256];
            room = std::min(room, sizeof(keep) - 1);
            std::memcpy(keep, ins, room);
            keep[room] = 0;
            lv_textarea_set_insert_replace(t, keep);
        },
        LV_EVENT_INSERT, (void *)(uintptr_t)max_bytes);
}

void regroup(lv_group_t *g, const std::vector<lv_obj_t *> &objs)
{
    uint32_t n = lv_group_get_obj_count(g);
    bool same = n == objs.size();
    for (uint32_t i = 0; same && i < n; i++)
        same = lv_group_get_obj_by_index(g, i) == objs[i];
    if (same)
        return; // nada cambió: el foco queda donde está
    lv_obj_t *f = lv_group_get_focused(g);
    lv_group_remove_all_objs(g);
    for (lv_obj_t *o : objs)
        lv_group_add_obj(g, o);
    if (f && std::find(objs.begin(), objs.end(), f) != objs.end())
        lv_group_focus_obj(f);
}

struct HoldState {
    lv_obj_t *fill;
    lv_timer_t *timer;
    uint32_t ms, start;
    void (*cb)(void *);
    void *user;
    bool fired;
    uint8_t taps = 0;             // con teclado: Enter seguidos
    lv_timer_t *tap_timer = nullptr;
    bool skip_click = false;      // el "soltar" de una pulsación que ya disparó no cuenta como toque
};

static void hold_reset(HoldState *h)
{
    if (h->timer) {
        lv_timer_delete(h->timer);
        h->timer = nullptr;
    }
    h->start = lv_tick_get();
    h->fired = false;
    lv_obj_set_width(h->fill, 0);
}

lv_obj_t *hold_button(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t bg, uint32_t fill,
                      uint32_t fg, const char *text, uint32_t ms, void (*cb)(void *), void *user, lv_group_t *g)
{
    lv_obj_t *b = button(parent, x, y, w, h, bg, 0, g);
    lv_obj_set_style_clip_corner(b, true, 0);
    auto *st = new HoldState{box(b, 0, 0, 0, h, fill), nullptr, ms, 0, cb, user, false};
    lv_obj_t *l = label(b, &tfield_font_12b, fg, text);
    lv_obj_center(l);
    lv_obj_set_user_data(b, st);
    lv_obj_add_event_cb(
        b, [](lv_event_t *e) {
            lv_obj_t *btn = (lv_obj_t *)lv_event_get_current_target(e);
            auto *st = static_cast<HoldState *>(lv_obj_get_user_data(btn));
            lv_event_code_t code = lv_event_get_code(e);
            if (code == LV_EVENT_PRESSED) {
                hold_reset(st);
                st->timer = lv_timer_create(
                    [](lv_timer_t *t) {
                        auto *st = static_cast<HoldState *>(lv_timer_get_user_data(t));
                        uint32_t elapsed = lv_tick_elaps(st->start); // tiempo real, no cantidad de llamadas
                        lv_obj_t *btn = lv_obj_get_parent(st->fill);
                        int32_t w = lv_obj_get_width(btn) * (int32_t)std::min(elapsed, st->ms) / (int32_t)st->ms;
                        lv_obj_set_width(st->fill, w);
                        if (elapsed >= st->ms && !st->fired) {
                            st->fired = true;
                            lv_timer_delete(st->timer);
                            st->timer = nullptr;
                            lv_obj_set_width(st->fill, 0);
                            st->cb(st->user); // al final: puede borrar el botón
                        }
                    },
                    50, st);
            } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
                st->skip_click = st->fired;
                hold_reset(st);
                if (st->taps)
                    lv_obj_set_width(st->fill, lv_obj_get_width(btn) * st->taps / 3);
            } else if (code == LV_EVENT_CLICKED) {
                if (st->skip_click) {
                    st->skip_click = false;
                    return;
                }
                // el teclado de la T-Deck no informa "mantenida": Enter 3 veces en menos de 2 s
                lv_indev_t *in = lv_indev_active();
                lv_indev_type_t type = in ? lv_indev_get_type(in) : LV_INDEV_TYPE_NONE;
                if (type != LV_INDEV_TYPE_KEYPAD && type != LV_INDEV_TYPE_ENCODER)
                    return; // en la pantalla hay que mantener
                st->taps++;
                if (st->tap_timer)
                    lv_timer_delete(st->tap_timer);
                st->tap_timer = nullptr;
                if (st->taps >= 3) {
                    st->taps = 0;
                    lv_obj_set_width(st->fill, 0);
                    st->cb(st->user);
                    return;
                }
                lv_obj_set_width(st->fill, lv_obj_get_width(btn) * st->taps / 3);
                st->tap_timer = lv_timer_create(
                    [](lv_timer_t *t) {
                        auto *st = static_cast<HoldState *>(lv_timer_get_user_data(t));
                        st->taps = 0;
                        st->tap_timer = nullptr;
                        lv_obj_set_width(st->fill, 0);
                    },
                    2000, st);
                lv_timer_set_repeat_count(st->tap_timer, 1);
            } else if (code == LV_EVENT_DEFOCUSED) {
                if (st->taps) { // al salir del botón, la cuenta empieza de nuevo
                    st->taps = 0;
                    if (st->tap_timer)
                        lv_timer_delete(st->tap_timer);
                    st->tap_timer = nullptr;
                    lv_obj_set_width(st->fill, 0);
                }
            } else if (code == LV_EVENT_DELETE) {
                if (st->timer)
                    lv_timer_delete(st->timer);
                if (st->tap_timer)
                    lv_timer_delete(st->tap_timer);
                delete st;
            }
        },
        LV_EVENT_ALL, nullptr);
    return b;
}

lv_obj_t *battery(lv_obj_t *parent, int32_t x, int32_t y)
{
    lv_obj_t *b = box(parent, x, y, 13, 7, col::SURF);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_t *shell = box(b, 0, 0, 11, 7, col::SURF);
    lv_obj_set_style_bg_opa(shell, LV_OPA_TRANSP, 0);
    border(shell, col::INK, 1);
    lv_obj_t *nub = box(b, 11, 2, 2, 3, col::INK);
    lv_obj_t *fill = box(b, 2, 2, 7, 3, col::INK);
    (void)nub;
    (void)fill;
    return b;
}

void battery_set(lv_obj_t *bat, int8_t pct, uint32_t ink)
{
    lv_obj_t *shell = lv_obj_get_child(bat, 0);
    lv_obj_t *nub = lv_obj_get_child(bat, 1);
    lv_obj_t *fill = lv_obj_get_child(bat, 2);
    if (pct > 100)
        pct = 100;
    int32_t w = pct < 0 ? 0 : (pct * 7 + 50) / 100;
    if (pct > 0 && w == 0)
        w = 1;
    uint32_t fc = (pct >= 0 && pct < 20) ? col::RED : ink;
    set_border_color(shell, ink);
    set_bg(nub, ink);
    set_bg(fill, fc);
    lv_obj_set_width(fill, w);
}

lv_obj_t *signal_bars(lv_obj_t *parent, int32_t x, int32_t y)
{
    lv_obj_t *s = box(parent, x, y, 11, 9, col::SURF);
    lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, 0);
    for (int i = 0; i < 4; i++) {
        int32_t h = 3 + i * 2;
        box(s, i * 3, 9 - h, 2, h, col::LINE);
    }
    return s;
}

void signal_set(lv_obj_t *bars, uint8_t level, uint32_t on, uint32_t off)
{
    for (uint32_t i = 0; i < 4; i++)
        set_bg(lv_obj_get_child(bars, i), i < level ? on : off);
}

} // namespace tfield::ui
