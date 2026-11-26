// Editor de respuestas rápidas: 4 textos de máx. QUICK_MAX_CHARS (14) caracteres.
#include "tfield/app.h"
#include "tfield/i18n.h"
#include "tfield/format.h"
#include "tfield/ui.h"

#include <cstdio>

namespace tfield {

using namespace ui;

void QuickScreen::create(App *app)
{
    app_ = app;
    screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen);
    set_bg(screen, col::SURF);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    group = lv_group_create();

    header_bar(screen, app, group, tr("Respuestas r\xC3\xA1pidas"));

    for (int i = 0; i < 4; i++) {
        int32_t y = 34 + i * 29;
        lv_obj_t *sq = box(screen, 6, y + 4, 16, 16, col::BLUE);
        lv_obj_set_style_radius(sq, 2, 0);
        char n[4];
        snprintf(n, sizeof(n), "%d", i + 1);
        lv_obj_center(label(sq, &tfield_font_12b, col::SURF, n));

        lv_obj_t *ta = lv_textarea_create(screen);
        lv_obj_set_pos(ta, 28, y);
        lv_obj_set_size(ta, 246, 24);
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_max_length(ta, QUICK_MAX_CHARS);
        lv_textarea_set_placeholder_text(ta, tr("vac\xC3\xAD"
                                             "a"));
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
        lv_obj_add_event_cb(ta, on_change, LV_EVENT_VALUE_CHANGED, this);
        lv_group_add_obj(group, ta);
        inputs_[i] = ta;

        counters_[i] = label(screen, &tfield_font_12, col::INK2, "0/14");
        lv_obj_set_width(counters_[i], 38);
        lv_obj_set_style_text_align(counters_[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(counters_[i], 276, y + 5);
    }

    lv_obj_t *pl = label(screen, &tfield_font_12, col::INK2, tr("Vista previa: as\xC3\xAD se ver\xC3\xA1n en el inicio"));
    lv_obj_set_pos(pl, 6, 151);
    for (int i = 0; i < 4; i++) {
        // mismo ancho que los botones del inicio (94 px): lo que no cabe se ve con "…"
        int32_t x = (i % 2) ? 100 : 4, y = (i / 2) ? 192 : 167, w = 94;
        lv_obj_t *chip = box(screen, x, y, w, 22, col::SURF);
        border(chip, col::BLUE, 1);
        lv_obj_set_style_radius(chip, 3, 0);
        preview_[i] = label(chip, &tfield_font_12b, col::BLUE, "");
        lv_obj_set_size(preview_[i], w - 4, 14);
        lv_obj_set_style_text_align(preview_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(preview_[i], LV_LABEL_LONG_MODE_DOTS);
        lv_obj_center(preview_[i]);
    }

    lv_obj_t *foot = box(screen, 0, 214, SCREEN_W, SCREEN_H - 214, col::SURF);
    border(foot, col::INK, 1, LV_BORDER_SIDE_TOP);
    status_ = label(foot, &tfield_font_12b, col::INK2, "");
    lv_obj_set_pos(status_, 6, 6);
    lv_obj_set_size(status_, 168, 14);
    lv_label_set_long_mode(status_, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_t *reset = button(foot, 178, 3, 66, 20, col::SURF, col::INK, group);
    lv_obj_set_style_bg_color(reset, c(col::BG), LV_STATE_PRESSED);
    lv_obj_center(label(reset, &tfield_font_12b, col::INK, tr("Restaurar")));
    lv_obj_add_event_cb(reset, on_reset, LV_EVENT_CLICKED, this);
    lv_obj_t *save = button(foot, 250, 3, 64, 20, col::BLUE, 0, group);
    lv_obj_set_style_bg_color(save, c(col::BLUE_DK), LV_STATE_PRESSED);
    lv_obj_center(label(save, &tfield_font_12b, col::SURF, tr("Guardar")));
    lv_obj_add_event_cb(save, on_save, LV_EVENT_CLICKED, this);
}

void QuickScreen::load(const QuickReplies &q)
{
    loading_ = true;
    for (int i = 0; i < 4; i++)
        lv_textarea_set_text(inputs_[i], q.text[i].c_str());
    loading_ = false;
    dirty_ = warned_ = false;
    set_text(status_, "");
    refresh();
    lv_group_focus_obj(inputs_[0]); // listo para escribir
}

bool QuickScreen::handle_esc()
{
    if (!dirty_ || warned_) {
        dirty_ = warned_ = false;
        return false; // salir
    }
    warned_ = true;
    set_text(status_, tr("Sin guardar: Esc descarta"));
    set_color(status_, col::RED);
    return true;
}

void QuickScreen::refresh()
{
    char buf[12];
    for (int i = 0; i < 4; i++) {
        const char *t = lv_textarea_get_text(inputs_[i]);
        size_t n = utf8_length(t);
        snprintf(buf, sizeof(buf), "%u/%u", (unsigned)n, (unsigned)QUICK_MAX_CHARS);
        set_text(counters_[i], buf);
        bool full = n >= QUICK_MAX_CHARS;
        set_font(counters_[i], full ? &tfield_font_12b : &tfield_font_12);
        set_color(counters_[i], full ? col::AMBER : col::INK2);
        set_text(preview_[i], *t ? t : tr("(vac\xC3\xAD"
                                       "a)"));
    }
}

void QuickScreen::on_change(lv_event_t *e)
{
    auto *self = static_cast<QuickScreen *>(lv_event_get_user_data(e));
    if (self->loading_)
        return;
    self->refresh();
    self->dirty_ = true;
    self->warned_ = false;
    set_text(self->status_, tr("Sin guardar"));
    set_color(self->status_, col::AMBER);
}

void QuickScreen::on_reset(lv_event_t *e)
{
    auto *self = static_cast<QuickScreen *>(lv_event_get_user_data(e));
    self->load(self->app_->quick_defaults());
    self->dirty_ = true;
    set_text(self->status_, tr("Sin guardar"));
    set_color(self->status_, col::AMBER);
}

void QuickScreen::on_save(lv_event_t *e)
{
    auto *self = static_cast<QuickScreen *>(lv_event_get_user_data(e));
    QuickReplies q;
    for (int i = 0; i < 4; i++) {
        std::string t = lv_textarea_get_text(self->inputs_[i]);
        while (!t.empty() && t.back() == ' ')
            t.pop_back();
        q.text[i] = utf8_truncate(t, QUICK_MAX_CHARS);
    }
    self->app_->save_quick(q);
    self->dirty_ = self->warned_ = false;
    set_text(self->status_, tr("Guardadas"));
    set_color(self->status_, col::GREEN);
}

} // namespace tfield
