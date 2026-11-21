// T-Field UI — pantallas y navegación.
#pragma once

#include "tfield/map.h"
#include "tfield/model.h"
#include "tfield/settings.h"
#include "tfield/theme.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace tfield {

enum class ScreenId { Home, Messages, QuickReplies, Map, Team, Settings };

// Una "vista" = pantalla + su estado (chat abierto, ficha de nodo). Esc vuelve a la anterior.
struct View {
    ScreenId id = ScreenId::Home;
    uint32_t node = 0;   // Equipo: ficha abierta (0 = lista)
    std::string chat;    // Mensajes: chat abierto
    int8_t section = -1; // Ajustes: sección abierta (-1 = cuadrícula)
};

// Tecla de tildes del teclado T-Deck (Shift+$), igual que en device-ui
constexpr uint32_t KEY_DIACRITIC = 0x110;

// Avisos con sonido (el adaptador toca el tono con el volumen de los ajustes)
enum class Alert { Message, Point, Sos, Test };

class App;

// ---------------------------------------------------------------- Inicio
class HomeScreen {
  public:
    void create(App *app);
    void set_status(const Status &s);
    void set_team(const std::vector<TeamNode> &team);
    void set_chats(const std::vector<Chat> &chats);
    void set_quick(const QuickReplies &q);
    void notify_sent(const std::string &to, const std::string &text);
    std::string selected_id() const;
    void clear_sent();
    lv_obj_t *screen = nullptr;
    lv_group_t *group = nullptr;

  private:
    struct TeamRow {
        lv_obj_t *row, *dot, *name, *heard, *dist, *alt, *bat, *pct, *bars;
    };
    struct ChatRow {
        lv_obj_t *row, *dot, *name, *tag, *time, *preview;
    };
    void select_chat(int index);
    void refresh_chips();
    void rebuild_focus();
    void restyle_chat_row(int i);
    static void on_chat(lv_event_t *e);
    static void on_chip(lv_event_t *e);
    static void on_tab(lv_event_t *e);

    App *app_ = nullptr;
    lv_obj_t *clock_, *day_, *month_, *sun_dot_, *daylight_, *sunset_, *name_, *bat_, *bat_pct_, *gps_;
    lv_obj_t *team_title_, *team_list_, *chat_badge_, *chat_list_, *team_empty_ = nullptr;
    lv_obj_t *quick_label_;
    lv_obj_t *chips_[4]{}, *chip_labels_[4]{}, *tabs_[4]{}, *sos_ = nullptr;
    std::vector<TeamRow> team_rows_;
    std::vector<ChatRow> chat_rows_;
    std::vector<Chat> chats_;
    QuickReplies quick_;
    int selected_ = -1;
    bool sent_shown_ = false;
};

// -------------------------------------------------------------- Mensajes
class MessagesScreen {
  public:
    void create(App *app);
    void set_status(const Status &s);
    void set_chats(const std::vector<Chat> &chats);
    void show_chat(const std::string &id);
    // force: rehacer aunque no haya cambiado nada (p. ej. un mensaje editado)
    void refresh_conversation(bool force = true);
    void focus_input();
    std::string draft() const;                 // lo escrito sin enviar (sobrevive a rehacer la pantalla)
    void set_draft(const std::string &text);
    lv_obj_t *conversation() const { return conv_; }
    lv_obj_t *screen = nullptr;
    lv_group_t *group = nullptr;
    std::string current;

  private:
    struct Row {
        lv_obj_t *row, *name, *badge, *badge_lbl, *meta;
    };
    void restyle_rows();
    void rebuild_focus();
    void update_counter();
    static void on_row(lv_event_t *e);
    static void on_send(lv_event_t *e);
    static void on_text(lv_event_t *e);

    App *app_ = nullptr;
    lv_obj_t *time_, *pct_, *list_, *conv_title_, *conv_meta_, *conv_ = nullptr, *input_, *counter_, *send_, *qr_;
    std::vector<Row> rows_;
    std::vector<Chat> chats_;
    void add_bubble(const Message &m);
    void trim_bubbles(size_t keep);
    size_t bubble_count() const;
    std::vector<std::pair<lv_obj_t *, lv_obj_t *>> bubble_btns_; // (burbuja, botón dentro)
    lv_obj_t *esc_ = nullptr, *more_btn_ = nullptr, *del_btn_ = nullptr, *del_lbl_ = nullptr;
    bool del_armed_ = false;
    std::string fitted_title_; // nombre para el que se ajustó la cabecera
    uint32_t del_armed_at_ = 0;
    void disarm_delete();
    std::string shown_chat_; // lo dibujado, para no rehacerlo si no cambió
    uint32_t shown_rev_ = 0, shown_added_ = 0;
    size_t show_n_ = CHAT_SHOW_MESSAGES; // burbujas del chat abierto ("Mensajes anteriores" suma más)
};

// ------------------------------------------------- Respuestas rápidas
class QuickScreen {
  public:
    void create(App *app);
    void load(const QuickReplies &q);
    // Esc con cambios sin guardar: la primera vez avisa (true), la segunda deja salir
    bool handle_esc();
    lv_obj_t *screen = nullptr;
    lv_group_t *group = nullptr;

  private:
    void refresh();
    static void on_change(lv_event_t *e);
    static void on_reset(lv_event_t *e);
    static void on_save(lv_event_t *e);

    App *app_ = nullptr;
    lv_obj_t *inputs_[4]{}, *counters_[4]{}, *preview_[4]{}, *status_;
    bool loading_ = false, dirty_ = false, warned_ = false;
};

// ---------------------------------------------------------------- Equipo
class TeamScreen {
  public:
    void create(App *app);
    void set_nodes(const std::vector<TeamNode> &nodes);
    void show_list();
    void show_detail(uint32_t num);
    void set_status(uint32_t num, const std::string &text, uint32_t color);
    // nota temporal en la franja de resumen (p. ej. "Respondieron 2 de 3")
    void set_note(const std::string &text, uint32_t color);
    uint32_t detail_num() const { return detail_num_; }
    lv_group_t *active_group() const { return detail_num_ ? detail_group_ : group; }
    bool showing_all() const { return show_all_; }
    void show_all(bool all) { set_filter(all); }
    // antes de rehacer la interfaz: sus grupos van a "groups" (se borran después) y los
    // temporizadores se detienen
    void release(std::vector<lv_group_t *> &groups);
    lv_obj_t *screen = nullptr;
    lv_group_t *group = nullptr;

  private:
    struct Row {
        lv_obj_t *row, *dot, *name, *lname, *heard, *bars, *dist, *alt, *bat, *pct, *hops;
    };
    void rebuild_list();
    void refresh_detail();
    void set_filter(bool all);
    const TeamNode *find(uint32_t num) const;

    App *app_ = nullptr;
    std::vector<Row> rows_;
    std::vector<uint32_t> row_nums_;
    bool show_all_ = false, dirty_ = false;
    lv_obj_t *seg_[2]{}, *seg_lbl_[2]{}, *summary_ = nullptr, *list_ = nullptr;
    lv_obj_t *ask_all_ = nullptr, *ask_all_lbl_ = nullptr, *more_ = nullptr;
    std::string note_;
    uint32_t note_color_ = 0;
    lv_timer_t *note_timer_ = nullptr, *ask_timer_ = nullptr;
    // ficha
    lv_obj_t *detail_ = nullptr, *d_dot_ = nullptr, *d_name_ = nullptr, *d_role_ = nullptr, *d_status_ = nullptr;
    lv_obj_t *d_val_[8]{}, *d_pos_lbl_ = nullptr;
    lv_obj_t *d_btn_[6]{}, *d_btn_lbl_[6]{};
    lv_group_t *detail_group_ = nullptr;
    uint32_t detail_num_ = 0;
    std::string status_text_;
    uint32_t status_num_ = 0, status_color_ = 0;
};


// --------------------------------------------------------------- Ajustes
class SettingsScreen {
  public:
    enum Section { S_SCREEN, S_GPS, S_ALERTS, S_MESSAGES, S_MAP, S_CHANNEL, S_SOS, S_NODE, S_RADIO, S_COUNT };
    void create(App *app);
    void show_home();
    void show_section(int s);
    int section() const { return section_; }
    void refresh(); // tras un cambio de ajustes, del equipo o del canal
    int focused_row() const;  // índice en el grupo (para volver al mismo lugar)
    void focus_row(int index);
    lv_obj_t *screen = nullptr;
    lv_group_t *group = nullptr;

  private:
    enum Kind { TOGGLE, CHOICE, TEXT, ACTION, STATIC, MEMBER, NOTE, CAPTION };
    struct Row {
        Kind kind = STATIC;
        bool team = false;
        lv_obj_t *obj = nullptr, *title = nullptr, *desc = nullptr, *value = nullptr, *pill = nullptr, *knob = nullptr;
        std::function<bool()> get;                  // interruptor
        std::function<bool(bool)> set;              // interruptor; false = no se permite
        std::function<std::string()> text;          // valor a la derecha
        std::function<std::string()> detail;        // segunda línea (si cambia)
        std::function<void(int, bool)> step;        // opción: dir ±1; wrap = vuelta al inicio (al tocar)
        std::function<std::string()> heading;       // título si cambia
        std::function<void()> act;                  // acción
        std::function<bool()> enabled;
        std::function<uint32_t()> color;            // miembro: color del punto y del estado
        std::function<void(const std::string &)> commit; // texto
        std::function<std::string()> current;       // texto: valor guardado
        bool armed = false;                         // acciones que piden confirmar
        bool confirm = false;
        std::string flash;                          // aviso temporal en la segunda línea
    };
    Row &add(Kind k, const char *title, const char *desc = "", bool team = false);
    void build_section(int s);
    void sec_screen();
    void sec_gps();
    void sec_alerts();
    void sec_messages();
    void sec_map();
    void sec_channel();
    void sec_sos();
    void sec_node();
    void sec_radio();
    void refresh_rows();
    void refresh_tiles();
    void rebuild_group(int focus_index = -1);
    void commit_texts();
    std::string channel_signature() const;
    void apply(const Settings &s);
    static void on_row(lv_event_t *e);
    static void on_row_key(lv_event_t *e);
    static void on_text(lv_event_t *e);

    App *app_ = nullptr;
    int section_ = -1;
    lv_obj_t *esc_ = nullptr, *title_ = nullptr, *grid_ = nullptr, *page_ = nullptr;
    lv_obj_t *tiles_[S_COUNT]{}, *tile_l1_[S_COUNT]{}, *tile_l2_[S_COUNT]{};
    std::vector<Row> rows_;
    std::string sig_; // Canal privado: se reconstruye si cambia el canal o el equipo
    // Avanzado: los cambios de radio esperan "Aplicar"
    uint8_t p_region_ = 0, p_preset_ = 0, p_hops_ = 3;
};

// -------------------------------------------------------- Aplicación
class App {
  public:
    // Ganchos hacia el firmware (los llena el adaptador o el simulador).
    std::function<void(const std::string &chat_id, const std::string &text)> on_send_text;
    std::function<void(const QuickReplies &)> on_save_quick;
    std::function<void(bool recording)> on_recording;
    // Escribir un archivo en la tarjeta SD (GPX de rutas y puntos)
    std::function<bool(const std::string &path, const std::string &content)> on_save_file;
    std::function<bool(const std::string &path)> on_delete_file;
    // Leer un archivo de la tarjeta SD (puntos de una ruta guardada, índice de rutas)
    std::function<bool(const std::string &path, std::string &content)> on_read_file;
    // Enviar un punto (en Meshtastic: paquete de waypoint a un canal o a un nodo)
    std::function<void(const Waypoint &, const Recipient &)> on_share_waypoint;
    // Equipo: pedir posición y ruta de señal (traceroute) a un nodo; cambios en "mi equipo"
    std::function<void(uint32_t num)> on_request_position;
    std::function<void(uint32_t num)> on_traceroute;
    std::function<void(const std::vector<uint32_t> &team)> on_team_changed;
    // SOS: activo/cancelado, texto (posición, hora, nombre), canales por los que se difunde
    // y nodos a los que va directo pidiendo confirmación (el firmware avisa con sos_ack).
    std::function<void(bool active, const std::string &text, const std::vector<std::string> &chat_ids,
                       const std::vector<uint32_t> &direct)>
        on_sos;
    // Ajustes: guardar en la memoria y aplicar en el firmware lo que corresponda. Se agrupa:
    // se llama 2 s después del último cambio (o al salir de Ajustes), para no gastar la flash
    // con cada toque de "‹ ›". Meshtastic también escribe su configuración en flash.
    std::function<void(const Settings &)> on_settings;
    // Aplicar ya, sin guardar (brillo, volumen: se ve o se oye al tocar)
    std::function<void(const Settings &)> on_settings_preview;
    // Agregar al final de un archivo (respaldo de la ruta en curso)
    std::function<bool(const std::string &path, const std::string &content)> on_append_file;
    // Sonido de aviso
    std::function<void(Alert, const std::string &what)> on_alert;
    // Se borró el historial de un chat (el adaptador borra lo que guarde). remove: el chat
    // directo desaparece de la lista; si no, es un canal que queda vacío.
    std::function<void(const std::string &chat_id, bool remove)> on_clear_chat;
    // Canal privado: crear / cambiar clave o nombre / unirse (name vacío = quitarlo)
    std::function<void(const std::string &name, const ChannelKey &key)> on_channel_set;
    // Invitación: mensaje directo cifrado con la clave pública del nodo (puerto privado de la app)
    std::function<void(uint32_t num, const std::string &name, const ChannelKey &key)> on_channel_invite;
    // Avisar a quien invitó que ya nos unimos
    std::function<void(uint32_t num)> on_channel_join_ack;
    // Bytes aleatorios para claves (en la T-Deck: esp_fill_random)
    std::function<void(uint8_t *buf, size_t n)> random_bytes;

    void init();
    void show(ScreenId id);
    void back();
    ScreenId current() const { return current_; }
    lv_group_t *active_group() const;

    // Datos entrantes
    void set_status(const Status &s);
    void set_team(const std::vector<TeamNode> &team);
    void set_chats(const std::vector<Chat> &chats);
    void set_messages(const std::string &chat_id, const std::vector<Message> &msgs);
    void add_message(const std::string &chat_id, const Message &m, bool alert = true);
    void set_quick(const QuickReplies &q);
    void set_position(const Position &p);
    void set_waypoints(const std::vector<Waypoint> &w);
    void set_track(const Track &t);
    void set_map_layers(const std::vector<TileSource *> &layers);
    void set_settings(const Settings &s); // al arrancar (no llama a on_settings)
    // Tema (claro, oscuro o automático de noche) e idioma: si cambian, se rehacen todas las
    // pantallas con los colores y textos nuevos, volviendo al mismo lugar
    bool wants_dark() const;
    void apply_look();
    void flush_settings();                // guardar ya lo pendiente (antes de apagar)
    // Al arrancar: si quedó una ruta en curso sin guardar (se apagó el equipo), se recupera
    bool recover_track(const std::string &csv);
    bool take_recovered(); // el mapa pregunta qué hacer con ella (una vez)
    static constexpr const char *TRACK_BACKUP = "/tfield/rutas/_en_curso.csv";

    // Ajustes
    const Settings &settings() const { return settings_; }
    void update_settings(const Settings &s); // aplica, refresca pantallas y llama a on_settings
    void show_settings_section(int section);
    bool is_team_chat(const Chat &ch) const;
    bool chat_muted(const Chat &ch) const; // sin sonido por los ajustes (fuera del equipo)
    void test_alert();
    // Canal privado del equipo
    void create_channel();
    void rename_channel(const std::string &name);
    void rekey_channel();
    void remove_channel();
    int invite_team();                  // devuelve a cuántos se invitó
    int pending_invites() const;        // miembros que aún pueden ser invitados
    void channel_joined(uint32_t num);  // el firmware recibió "me uní"
    void receive_channel_invite(uint32_t from, const std::string &name, const ChannelKey &key);
    bool accept_invite(uint32_t invite_id);
    // Mi equipo, sin repetidores (personas)
    std::vector<const TeamNode *> team_people() const;

    // Acciones desde la interfaz
    void send(const std::string &chat_id, const std::string &text);
    void save_quick(const QuickReplies &q);
    void open_chat(const std::string &chat_id);
    // Borrar: un canal queda vacío; un chat directo desaparece (vuelve al escribirle desde
    // Equipo › ficha › Mensaje, o cuando el otro escriba)
    void delete_chat(const std::string &chat_id);
    void clear_all_chats();
    void purge_old_messages(); // los de más de Ajustes › Mensajes › días (se llama solo)
    // Rutas
    void start_recording();
    void stop_recording();   // pausa; la pantalla pregunta si guardar o desechar
    void resume_recording();
    std::string save_track(const std::string &name, const std::string &note); // devuelve la ruta del archivo
    void discard_track();
    void delete_track(uint32_t id);
    bool update_track(uint32_t id, const std::string &name, const std::string &note); // false: no se pudo escribir
    // Lista de rutas (solo datos; si trae puntos, se calculan los datos y los puntos se sueltan:
    // al mostrar una ruta se leen de su archivo)
    void set_saved_tracks(const std::vector<SavedTrack> &t);
    // Al arrancar: la lista de rutas desde el índice en la SD (dos copias que se alternan,
    // por si se corta la energía mientras se escribe una). false si no hay índice.
    bool load_routes();
    static constexpr const char *ROUTE_INDEX_A = "/tfield/rutas/indice-a.csv";
    static constexpr const char *ROUTE_INDEX_B = "/tfield/rutas/indice-b.csv";
    // Guarda una ruta ya hecha (la en curso al terminar, o una importada): GPX + índice
    std::string add_saved_track(const std::string &name, const std::string &note, const Track &t);
    // Puntos de una ruta guardada (se leen de la SD la primera vez; se guarda una sola)
    const std::vector<TrackPoint> *route_points(uint32_t id);
    void add_track_point(const TrackPoint &p); // el firmware llama al recibir posición
    std::string next_track_name() const { return "R" + std::to_string(next_track_id_); }
    const std::vector<SavedTrack> &saved_tracks() const { return saved_tracks_; }
    // Puntos
    std::string next_point_name() const;
    Waypoint draft_point() const; // punto en la posición actual, sin guardar
    void add_waypoint(const Waypoint &w);
    void update_waypoint(uint32_t id, const std::string &name, const std::string &note);
    void delete_waypoint(uint32_t id);
    // devuelve los nombres a quienes se envió, separados por coma
    std::string share_waypoint(uint32_t id, const std::vector<Recipient> &to);
    std::vector<Recipient> share_recipients() const; // nodos del equipo, chats directos y canales
    void show_waypoint(uint32_t id); // abre el mapa centrado en el punto
    // Equipo
    const std::vector<TeamNode> &nodes() const { return team_; }
    void set_in_team(uint32_t num, bool in);
    void show_node(uint32_t num);          // ficha del nodo
    void show_node_on_map(uint32_t num);   // mapa centrado en el nodo, con su rastro
    void navigate_to_node(uint32_t num);   // el nodo queda como destino
    void open_direct_chat(uint32_t num);
    void request_position(uint32_t num);
    void position_received(uint32_t num);  // respuesta del firmware
    int request_team_positions();          // a todo mi equipo de una vez
    void traceroute(uint32_t num);
    void traceroute_result(uint32_t num, const std::string &route);
    void add_trail_point(uint32_t num, const TrackPoint &p);
    const std::vector<TrackPoint> &trail(uint32_t num);
    const std::vector<std::string> &last_share() const { return last_share_; }
    // punto recibido de otro equipo por la red
    void receive_waypoint(const Waypoint &w, const std::string &from_chat_id);
    uint32_t accept_waypoint(uint32_t pending_id); // "Guardar punto"; devuelve el id guardado
    // SOS
    void trigger_sos();
    void cancel_sos();
    void sos_ack(uint32_t num); // un miembro confirmó que recibió el SOS directo
    bool sos_active() const { return sos_active_; }
    std::string sos_text() const;
    // "20 nov 14:32" en hora local
    std::string local_date_time(uint32_t epoch) const;
    std::string local_hhmm(uint32_t epoch) const;
    const std::vector<Waypoint> &waypoints() const { return waypoints_; }
    void refresh_input_group();
    const Track &track() const { return track_; }
    const Position &position() const { return position_; }

    // Teclado físico: atajos que no dependen del objeto enfocado.
    // Devuelve true si la tecla se consumió aquí.
    bool handle_key(uint32_t key);

    const QuickReplies &quick() const { return quick_; }
    const QuickReplies &quick_defaults() const; // las del idioma elegido
    const std::vector<Chat> &chats() const { return chats_; }
    const std::vector<Message> &messages(const std::string &chat_id) { return messages_[chat_id]; }
    uint32_t messages_rev(const std::string &chat_id) const
    {
        auto it = msg_rev_.find(chat_id);
        return it == msg_rev_.end() ? 0 : it->second;
    }
    // mensajes agregados al final (si todo cambio fue agregar, la conversación no se rehace)
    uint32_t messages_added(const std::string &chat_id) const
    {
        auto it = msg_added_.find(chat_id);
        return it == msg_added_.end() ? 0 : it->second;
    }
    const Status &status() const { return status_; }

    // para la hora en "Enviado 14:32"
    std::string now_hhmm() const;

    HomeScreen home;
    MessagesScreen messages_screen;
    QuickScreen quick_screen;
    MapScreen map_screen;
    TeamScreen team_screen;
    SettingsScreen settings_screen;

  private:
    void publish_chats();
    std::string direct_chat_id(uint32_t num, bool create);
    std::string ensure_channel_chat(const std::string &name);
    std::string track_file(const std::string &name, uint32_t id) const;
    bool write_track_file(const std::string &file, const std::string &name, const std::string &note, const Track &t);
    void save_route_index();
    uint32_t route_gen_ = 0;
    uint32_t loaded_route_ = 0;            // ruta cuyos puntos están en memoria
    std::vector<TrackPoint> loaded_pts_;
    ChannelKey new_key();
    void sos_targets(std::vector<std::string> &chats, std::vector<uint32_t> &direct) const;
    void alert_incoming(const Chat &ch, const Message &m);
    void go(const View &v, bool push, bool restoring);
    View snapshot() const;
    ScreenId current_ = ScreenId::Home;
    std::vector<View> history_;
    Status status_;
    std::vector<Chat> chats_;
    std::map<std::string, std::vector<Message>> messages_;
    std::map<std::string, uint32_t> msg_rev_;   // cambia con cada mensaje nuevo o editado
    std::map<std::string, uint32_t> msg_added_; // cuenta solo los agregados al final
    uint32_t last_purge_ = 0;
    void forget_chat(const std::string &chat_id, bool remove);
    Position position_;
    std::vector<Waypoint> waypoints_;
    Track track_;
    std::vector<SavedTrack> saved_tracks_;
    std::vector<TeamNode> team_;
    std::map<uint32_t, std::vector<TrackPoint>> trails_;
    std::string force_chat_; // chat a abrir en la próxima visita a Mensajes
    std::vector<std::string> last_share_; // ids elegidos la última vez
    uint32_t next_track_id_ = 1;
    bool export_waypoints();
    void build_sos();
    void show_sos(bool overlay);
    bool sos_active_ = false;
    uint32_t sos_since_ = 0, sos_sent_ = 0;
    std::vector<std::string> sos_chats_;
    std::vector<std::pair<uint32_t, uint32_t>> sos_acks_; // nodo, hora de confirmación (0 = esperando)
    lv_obj_t *sos_direct_ = nullptr;
    Settings settings_;
    std::map<uint32_t, Waypoint> pending_wps_;
    uint32_t next_pending_ = 1;
    struct Invite {
        uint32_t from;
        std::string name;
        ChannelKey key;
    };
    std::map<uint32_t, Invite> invites_;
    uint32_t next_invite_ = 1;
    std::vector<uint32_t> pos_wait_; // pedidos de posición a todo el equipo
    size_t pos_total_ = 0;
    bool settings_pending_ = false;
    lv_timer_t *settings_timer_ = nullptr;
    std::vector<uint32_t> members_; // "mi equipo" visto la última vez (para no rehacer chats)
    std::string backup_buf_;        // puntos de la ruta aún no escritos en la SD
    double dist_acc_ = 0, gain_acc_ = 0; // totales de la ruta en curso, sin redondear cada paso
    float alt_ref_ = 0;
    void restat_track();            // recalcula los totales desde los puntos
    bool recovered_ = false;
    void flush_backup();
    lv_timer_t *sos_timer_ = nullptr;
    lv_group_t *sos_group_ = nullptr;
    lv_obj_t *sos_banner_ = nullptr, *sos_banner_lbl_ = nullptr, *sos_overlay_ = nullptr, *sos_info_ = nullptr,
             *sos_count_ = nullptr, *sos_cancel_lbl_ = nullptr, *sos_cancel_fill_ = nullptr;
    QuickReplies quick_;
    void rebuild_ui();
    bool ui_busy() const;     // algo a medio escribir: el cambio automático de tema espera
    bool look_pending_ = false;
};

// Barra superior compartida por las pantallas secundarias (Esc + título).
lv_obj_t *header_bar(lv_obj_t *scr, App *app, lv_group_t *g, const char *title);

} // namespace tfield
