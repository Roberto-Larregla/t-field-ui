// T-Field UI — pantalla de mapa: mapa base por teselas, equipo, rutas, puntos, regla.
#pragma once

#include "tfield/model.h"
#include "tfield/theme.h"

#include <array>
#include <deque>
#include <string>
#include <vector>

namespace tfield {

class App;

// Fuente de teselas raster de 256x256 (z/x/y). En la T-Deck la implementa un
// adaptador sobre los servicios de device-ui (carpetas /maps/<estilo>/z/x/y.png
// o archivos .pmtiles en la tarjeta SD). En el simulador, un mapa de ejemplo.
class TileSource {
  public:
    virtual ~TileSource() = default;
    virtual const char *name() const = 0;          // "Topográfico"
    virtual const char *attribution() const { return ""; }
    virtual uint8_t min_zoom() const { return 1; }
    virtual uint8_t max_zoom() const { return 18; }
    // Devuelve nullptr si la tesela no existe (zona sin cobertura).
    virtual const lv_image_dsc_t *tile(uint8_t z, uint32_t x, uint32_t y) = 0;
    // La pantalla avisa qué teselas está mostrando, para poder liberar el resto.
    virtual void retain(const std::vector<uint64_t> &keys) {}
    static uint64_t key(uint8_t z, uint32_t x, uint32_t y) { return ((uint64_t)z << 48) | ((uint64_t)x << 24) | y; }
};

class MapScreen {
  public:
    void create(App *app);
    void set_layers(const std::vector<TileSource *> &layers);
    void set_position(const Position &p);
    void set_team(const std::vector<TeamNode> &team);
    void set_waypoints(const std::vector<Waypoint> &wps);
    void set_track(const Track &t); // la ruta en curso cambió (la lista es la de la App)
    void set_saved_tracks(const std::vector<SavedTrack> &tracks);
    void zoom_by(int delta);
    void pan(int32_t dx, int32_t dy);
    void center_on_me();
    void focus_waypoint(uint32_t id); // centra el mapa en un punto y lo resalta
    void focus_node(uint32_t num);    // centra en un nodo y muestra su rastro
    void on_enter();
    void shown();                // la pantalla quedó visible: dibuja lo pendiente
    void set_team_only(bool on); // ocultar nodos fuera de mi equipo
    void show_route(uint32_t id); // muestra una ruta guardada y la encuadra
    void center(double &lat, double &lon) const { lat = clat_, lon = clon_; }
    bool pan_mode() const { return pan_mode_ || ruler_; } // las flechas mueven el mapa
    uint8_t zoom() const { return zoom_; }
    void set_pan_mode(bool on);
    bool dialog_open() const { return dlg_kind_ != DLG_NONE || share_open_; }
    // Al rehacer la interfaz (tema o idioma): lo que se estaba viendo vuelve igual
    struct State {
        std::vector<TileSource *> layers;
        int layer = 0;
        double clat = 0, clon = 0;
        uint8_t zoom = 15;
        bool follow = true, team_only = false;
        uint32_t shown_route = 0, highlight_wp = 0, trail_node = 0;
    };
    State save_state() const;
    void restore_state(const State &s);
    void release(std::vector<lv_group_t *> &groups); // grupos a borrar; detiene temporizadores
    void toast(const std::string &msg);
    // Esc: cierra regla, diálogo o panel. Devuelve true si cerró algo.
    bool handle_esc();
    lv_group_t *active_group() const { return share_open_ ? share_group_ : (dlg_kind_ ? dlg_group_ : group); }
    lv_obj_t *screen = nullptr;
    lv_group_t *group = nullptr;
    lv_obj_t *view = nullptr; // el área del mapa (enfocable)
    lv_obj_t *layers_button() const { return btn_names_; }

  private:
    struct NodeMarker {
        lv_obj_t *dot, *label;
        lv_obj_t *arrow = nullptr; // fuera de la vista: flecha hacia donde está
    };
    // Marcador dibujado (para tocarlo o recorrerlos con ‹ ›)
    struct Hit {
        int32_t x, y;  // centro, en el área del mapa
        uint8_t kind;  // SEL_NODE o SEL_WP
        uint32_t id;   // número de nodo o id del punto
        bool edge;     // nodo fuera de la vista, en el borde
        size_t mark;   // índice en team_marks_ o wp_marks_
    };
    enum SelKind : uint8_t { SEL_NONE = 0, SEL_NODE, SEL_WP };
    enum DlgKind { DLG_NONE = 0, DLG_NEW_POINT, DLG_EDIT_POINT, DLG_ROUTE, DLG_EDIT_ROUTE };
    enum PanelKind { PANEL_NONE = 0, PANEL_POINTS, PANEL_LAYERS, PANEL_ROUTES, PANEL_NAMES };

    // Polilínea dibujada en trozos de pocos puntos, cada uno con su propio rectángulo: al
    // agregar un punto a la ruta solo se redibuja el último trozo, y cuando cambia un área
    // chica (el punto propio, las coordenadas) LVGL no vuelve a trazar la ruta entera.
    struct Poly {
        lv_obj_t *layer = nullptr; // contenedor sin estilo: su lugar en el orden de dibujo
        lv_style_t style{};
        std::vector<lv_obj_t *> parts;
        std::vector<std::vector<lv_point_precise_t>> pts; // LVGL guarda el puntero
    };
    // Puntos ya proyectados (Mercator a zoom 22, enteros): sin log/tan en cada redibujo.
    // El ESP32-S3 solo tiene coma flotante simple: las cuentas en double van por software.
    struct Proj {
        std::vector<int32_t> xy;
        TrackPoint first{}, last{};
        size_t n = 0;
    };
    struct Wxy { // proyección guardada de un marcador
        uint32_t key = 0;
        double lat = 0, lon = 0;
        int32_t x = 0, y = 0;
    };
    void init_poly(Poly &p);
    void set_poly(Poly &p, std::vector<lv_point_precise_t> &fresh);
    void hide_poly(Poly &p);
    void project(Proj &c, const std::vector<TrackPoint> &pts);
    int32_t px_x(int32_t wx) const { return (int32_t)(((int64_t)wx - l22_ + (1LL << (shift_ - 1))) >> shift_); }
    int32_t px_y(int32_t wy) const { return (int32_t)(((int64_t)wy - t22_ + (1LL << (shift_ - 1))) >> shift_); }
    bool cached_screen(std::vector<Wxy> &cache, size_t i, uint32_t key, double lat, double lon, int32_t &sx, int32_t &sy);

    void redraw();
    void fit(const std::vector<TrackPoint> &pts);
    void fit_box(double lat0, double lat1, double lon0, double lon1);
    void set_grid_line(int i, lv_point_precise_t a, lv_point_precise_t b);
    void place_me();
    std::string team_signature() const;
    void place_markers();
    void update_toolbar();
    void update_strip(); // franja de la regla
    void update_center_button();
    void show_panel(int which);
    void rebuild_points_panel();
    void rebuild_layers_panel();
    void rebuild_routes_panel();
    void rebuild_names_panel();   // Capas: ver y nombres por capa; dentro de una, uno por uno
    void open_layer_items(int layer);
    // elegir un marcador: anillo y ficha con nombre, distancia y desnivel
    void select(uint8_t kind, uint32_t id);
    void clear_selection();
    void step_selection(int dir);  // ‹ › : el siguiente o el anterior, del centro hacia afuera
    void open_selection();         // Enter o tocar la ficha
    void place_selection(std::vector<lv_area_t> &placed);
    void update_names_button();
    bool to_screen(double lat, double lon, int32_t &sx, int32_t &sy) const;
    void line_points(Proj &c, const std::vector<TrackPoint> &pts, std::vector<lv_point_precise_t> &out);
    // regla
    void ruler_toggle();
    void ruler_tap(int32_t sx, int32_t sy);
    // diálogo de nombre y nota
    void build_dialog();
    void open_dialog(DlgKind kind, uint32_t id = 0);
    void close_dialog();
    void dialog_action(int which);
    void update_dialog_hint();
    void set_dialog_buttons(const char *a, const char *b, const char *c, const char *d);

    // selector de destinatarios
    void build_share();
    void open_share(uint32_t wp_id, bool just_created);
    void close_share(bool sent);
    void refresh_share();

    static void on_view(lv_event_t *e);
    static void on_key(lv_event_t *e);

    App *app_ = nullptr;
    std::vector<TileSource *> layers_;
    int layer_ = 0;  // índice en layers_, -1 = sin mapa base
    double clat_ = 0, clon_ = 0;
    uint8_t zoom_ = 15;
    bool follow_ = true;
    uint32_t shown_route_ = 0; // ruta guardada dibujada en el mapa
    uint32_t highlight_wp_ = 0; // punto resaltado (nombre completo)
    uint32_t trail_node_ = 0;   // nodo cuyo rastro se dibuja
    bool team_only_ = false;
    bool pan_mode_ = false;
    bool dirty_ = false; // hay cambios sin dibujar (la pantalla no estaba visible)

    Position pos_;
    const std::vector<TeamNode> &team_nodes() const; // los de la App (sin copia)
    // listas de la App, sin copia (con 300 puntos y 10 rutas, una copia eran ~1 MB)
    const std::vector<Waypoint> &wps() const;
    const Track &track() const;
    const std::vector<SavedTrack> &saved() const;

    // objetos del mapa
    lv_obj_t *tiles_[6]{};
    lv_obj_t *grid_[16]{};
    std::vector<lv_point_precise_t> grid_pts_;
    Poly track_poly_, route_poly_, trail_poly_;
    Proj track_proj_, route_proj_, trail_proj_;
    uint32_t trail_color_ = 0;
    std::vector<Wxy> wp_xy_, node_xy_;
    // vista actual en píxeles de zoom 22 (se fija en redraw): la misma cuenta entera para
    // teselas, líneas y marcadores
    int64_t l22_ = 0, t22_ = 0;
    int shift_ = 7;
    std::vector<lv_point_precise_t> ruler_vec_;
    std::string team_sig_; // lo dibujado del equipo: si no cambia, no se redibuja
    lv_obj_t *me_halo_ = nullptr, *me_dot_ = nullptr, *cross_ = nullptr;
    std::vector<NodeMarker> team_marks_;
    std::vector<NodeMarker> wp_marks_;
    std::deque<std::array<lv_point_precise_t, 3>> arrow_pts_; // LVGL guarda el puntero: no se mueven
    std::vector<Hit> hits_;
    uint8_t sel_kind_ = SEL_NONE;
    uint32_t sel_id_ = 0;
    lv_obj_t *sel_ring_ = nullptr, *callout_ = nullptr, *callout_l1_ = nullptr, *callout_l2_ = nullptr,
             *callout_l3_ = nullptr;
    lv_obj_t *btn_names_ = nullptr, *names_img_ = nullptr, *panel_close_lbl_ = nullptr;
    int layer_view_ = -1; // en el panel Capas: -1 la lista de capas; 0..3 los elementos de una
    std::vector<std::pair<uint8_t, uint32_t>> names_rows_; // filas "uno por uno" del panel Nombres
    lv_obj_t *coords_ = nullptr, *strip_ = nullptr, *strip_lbl_ = nullptr;
    lv_obj_t *scale_bar_ = nullptr, *scale_lbl_ = nullptr, *attrib_ = nullptr, *north_ = nullptr;
    lv_obj_t *layer_btn_lbl_ = nullptr;
    lv_obj_t *zoom_btn_[2]{};
    lv_obj_t *btn_center_ = nullptr, *center_ring_ = nullptr, *center_dot_ = nullptr, *btn_ruler_ = nullptr;
    lv_obj_t *btn_rec_ = nullptr, *btn_mark_ = nullptr, *btn_points_ = nullptr, *btn_routes_ = nullptr;
    lv_obj_t *rec_l1_ = nullptr, *rec_l2_ = nullptr, *rec_dot_ = nullptr, *pts_l2_ = nullptr, *routes_l2_ = nullptr;
    lv_obj_t *panel_ = nullptr, *panel_title_ = nullptr, *panel_list_ = nullptr;
    lv_obj_t *toast_ = nullptr;
    lv_timer_t *toast_timer_ = nullptr;
    int panel_kind_ = PANEL_NONE;

    // regla: 0 apagada, 1 espera A, 2 espera B, 3 midiendo
    int ruler_ = 0;
    double ra_lat_ = 0, ra_lon_ = 0, rb_lat_ = 0, rb_lon_ = 0;
    lv_obj_t *ruler_line_ = nullptr, *ruler_a_ = nullptr, *ruler_b_ = nullptr, *ruler_lbl_ = nullptr;
    lv_point_precise_t ruler_pts_[2]{};
    int32_t drag_ = 0; // px arrastrados desde que se presionó

    // diálogo
    lv_obj_t *dlg_ = nullptr, *dlg_title_ = nullptr, *dlg_info_ = nullptr, *dlg_hint_ = nullptr;
    lv_obj_t *dlg_name_ = nullptr, *dlg_note_ = nullptr;
    lv_obj_t *dlg_btn_[4]{}, *dlg_btn_lbl_[4]{};
    lv_group_t *dlg_group_ = nullptr;
    DlgKind dlg_kind_ = DLG_NONE;
    Waypoint draft_;
    uint32_t dlg_route_ = 0;
    bool armed_ = false; // confirmación de Borrar / Desechar

    lv_obj_t *share_ = nullptr, *share_title_ = nullptr, *share_list_ = nullptr, *share_send_ = nullptr,
             *share_send_lbl_ = nullptr;
    lv_group_t *share_group_ = nullptr;
    bool share_open_ = false, share_new_ = false;
    uint32_t share_wp_ = 0;
    std::vector<Recipient> share_to_;
    std::vector<bool> share_sel_;
    std::vector<lv_obj_t *> share_rows_;
};

} // namespace tfield
