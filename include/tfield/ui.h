// T-Field UI — piezas básicas. Posicionamiento absoluto: barato de calcular.
#pragma once

#include "tfield/theme.h"
#include <initializer_list>
#include <string>
#include <vector>

namespace tfield::ui {

// Rectángulo plano sin estilos del tema, sin scroll, sin padding.
lv_obj_t *box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t bg);
void border(lv_obj_t *o, uint32_t color, int32_t width, lv_border_side_t side = LV_BORDER_SIDE_FULL);

lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *txt);
// Cambia el texto solo si es distinto: evita redibujos innecesarios.
void set_text(lv_obj_t *label, const char *txt);
inline void set_text(lv_obj_t *label, const std::string &s) { set_text(label, s.c_str()); }
// Los setters de estilo de LVGL redibujan aunque el valor sea el mismo; estos no hacen nada
// si no cambia (en la T-Deck, cada redibujo es SPI y CPU: batería).
void set_color(lv_obj_t *o, uint32_t color); // color del texto
void set_bg(lv_obj_t *o, uint32_t color);
void set_border_color(lv_obj_t *o, uint32_t color);
void set_font(lv_obj_t *o, const lv_font_t *font);
void show(lv_obj_t *o, bool visible);
void align(lv_obj_t *o, lv_align_t a, int32_t x, int32_t y); // lv_obj_align sin redibujo si no cambia
// lv_obj_align_to para un hermano (mismo padre); solo OUT_RIGHT_MID, OUT_RIGHT_BOTTOM y OUT_LEFT_MID
void align_to(lv_obj_t *o, lv_obj_t *base, lv_align_t a, int32_t x, int32_t y);
// Lleva "objs" (del mismo contenedor) al frente, en ese orden, solo si hace falta: LVGL
// redibuja todo el contenedor cada vez que cambia el orden, aunque quede igual que antes
void keep_on_top(std::initializer_list<lv_obj_t *> objs);

// Botón plano (rectángulo clicable y enfocable).
lv_obj_t *button(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t bg, uint32_t border_color,
                 lv_group_t *group);
// Estilo de foco por teclado/trackball: contorno de 2 px.
void focus_style(lv_obj_t *o);

// Botón que hay que mantener presionado `ms` milisegundos (SOS, cancelar SOS).
// Se rellena de izquierda a derecha mientras se mantiene; al completarse llama a `cb`.
// Con teclado (que no informa "tecla mantenida"): Enter 3 veces en menos de 2 s.
lv_obj_t *hold_button(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t bg, uint32_t fill,
                      uint32_t fg, const char *text, uint32_t ms, void (*cb)(void *), void *user, lv_group_t *g);

// Limita un cuadro de texto a max_bytes en UTF-8 (lo que se escribe de más no entra).
void limit_bytes(lv_obj_t *textarea, uint32_t max_bytes);

// Reconstruye un grupo con estos objetos y deja el foco donde estaba (si sigue en la lista).
void regroup(lv_group_t *g, const std::vector<lv_obj_t *> &objs);

// Batería pequeña (12x7) con relleno proporcional.
lv_obj_t *battery(lv_obj_t *parent, int32_t x, int32_t y);
void battery_set(lv_obj_t *bat, int8_t pct, uint32_t ink);

// 4 barras de señal (11x9).
lv_obj_t *signal_bars(lv_obj_t *parent, int32_t x, int32_t y);
void signal_set(lv_obj_t *bars, uint8_t level, uint32_t on, uint32_t off);

} // namespace tfield::ui
