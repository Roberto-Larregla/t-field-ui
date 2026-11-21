# T-Field UI · Plan de implementación en la T-Deck Plus

Última actualización: 20 nov 2026

## 1. Qué vamos a construir

No es un sistema operativo. La T-Deck ejecuta un solo programa, el **firmware de Meshtastic**. Nuestra interfaz es una **capa gráfica** sobre ese firmware. La radio, la malla, el GPS, el cifrado y los mensajes los sigue manejando Meshtastic sin cambios. Los dos equipos siguen siendo nodos Meshtastic normales: hablan con cualquier otro nodo, con la app del teléfono y con repetidores estándar.

## 2. Arquitectura

| Pieza | Qué es | Qué hacemos |
|---|---|---|
| `meshtastic/firmware` | Firmware completo. El entorno de compilación de la T-Deck con pantalla gráfica es `t-deck-tft` (usa `VIEW_320x240`). | No se modifica, salvo una línea: apuntar `device-ui` a nuestra copia. |
| `meshtastic/device-ui` | Librería de la interfaz gráfica (MUI). C++20 + LVGL. Licencia GPL-3. | **Copia propia (fork).** Aquí va todo nuestro trabajo. |
| `TFTView_320x240` | La vista actual de la MUI (~9.300 líneas). Incluye inicio, mensajes, nodos, mapa y ajustes. | Se mantiene como base. Reemplazamos pantallas una a una, sin romper el resto. |
| `MeshtasticView` | Interfaz con eventos del firmware: `updatePosition`, `updateMetrics`, `updateSignalStrength`, `newMessage`, `updateTime`, etc. | Nuestras pantallas se alimentan de estos eventos. |
| `ViewController` | Envía acciones al firmware: `sendTextMessage`, configuración, etc. | Lo usamos para enviar mensajes, respuestas rápidas y ajustes. |

Nuestro código nuevo vive en una carpeta aparte (`source/graphics/TFT/tfield/`) y se construye directamente en C++ con LVGL. No usa el editor EEZ Studio. Así los cambios quedan aislados y es más fácil incorporar actualizaciones de Meshtastic. La interfaz no depende de Meshtastic: un **adaptador** llena sus datos y responde a sus "ganchos" (`on_send_text`, `on_sos`, `on_settings`, etc.).

Para cambiar la librería, en la configuración del firmware (`platformio.ini`, sección `[device-ui_base]`) se reemplaza la URL de `meshtastic/device-ui` por la de nuestra copia.

## 3. Fases

| Fase | Contenido | Resultado verificable |
|---|---|---|
| 0. Entorno | Instalar PlatformIO, compilar `t-deck-tft` **sin cambios** y cargarlo en ambas T-Deck. | Las dos T-Deck con la MUI oficial, compilada por ti. |
| 1. Base visual | Paleta "Claro", fuente Atkinson Hyperlegible, barra inferior de 4 pestañas (Mensajes, Mapa, Equipo, Ajustes). | La MUI con nuestro aspecto y navegación. |
| 2. Inicio | Dashboard: reloj, sol, batería y GPS propios, lista de equipo, últimos chats, respuestas rápidas. | La pantalla de inicio diseñada, con datos reales. |
| 3. Respuestas rápidas | 4 textos de máx. 25 caracteres, guardados en el equipo, editables desde Mensajes. | Editar, guardar y enviar desde el inicio. |
| 4. Mensajes | Chats, conversación, contador de caracteres; botones dentro de la burbuja (ver punto, guardar punto, unirse a canal). | |
| 5. Mapa | Mapa base, equipo, puntos, rutas GPX, regla, destino, compartir. | |
| 6. Equipo | Lista "Mi equipo / Toda la red", ficha del nodo, pedir posiciones a todos. | |
| 7. Ajustes | 9 secciones (ver punto 10), canal privado e invitaciones. | |

Cada fase deja los equipos funcionando. Si algo falla, se vuelve a cargar la versión anterior. **Esc** siempre vuelve a la pantalla anterior, con su estado, y en último caso al inicio.

## 4. De dónde sale cada dato del inicio

| Dato | Fuente | Frecuencia de actualización en pantalla |
|---|---|---|
| Hora y fecha | `updateTime` (hora GPS/red) + zona horaria del equipo | 1 vez por minuto |
| Luz restante, salida y puesta de sol | Cálculo local (algoritmo NOAA) con posición propia + fecha | Cada 10 min |
| Batería propia y autonomía | `updateMetrics` del nodo propio. La autonomía se estima con la pendiente de los últimos 30–60 min. | Al llegar el dato |
| GPS (fijación, satélites) | `updatePosition` del nodo propio | Al llegar el dato |
| Equipo: distancia, rumbo | Haversine entre la posición propia y la del otro nodo | Al llegar una posición |
| Equipo: diferencia de altura | Altitud del otro nodo − altitud propia | Ídem |
| Equipo: batería | `updateMetrics` (telemetría del otro nodo) | Al llegar el dato |
| Equipo: señal | `updateSignalStrength` (RSSI/SNR) + `updateHopsAway`. Si llega por un repetidor, la señal es la del último salto. | Al llegar un paquete |
| Equipo: última vez escuchado | `updateLastHeard` | Texto relativo, 1 vez por minuto |
| Orden de la lista | Por distancia; nodos sin posición al final | Al cambiar una distancia |
| Chats y no leídos | Historial de mensajes de la MUI; con "equipo primero", los del equipo arriba | Al llegar un mensaje |

## 5. Batería: reglas de diseño del código

- **Sin animaciones, sombras ni degradados.** Solo rellenos planos.
- **Redibujar solo lo que cambia.** Nuestro código solo toca una etiqueta cuando su valor cambia de verdad.
- **Relojes lentos.** Hora y "hace X min" cada 60 s; sol cada 10 min.
- **Pantalla apagada = cero trabajo de interfaz.**
- La mayor parte del consumo es la retroiluminación, luego la radio y el GPS. Brillo y apagado de pantalla se cambian en Ajustes › Pantalla.
- Pedir posiciones a todo el equipo está limitado a una vez por minuto (cada pedido ocupa la radio una vez por nodo).

## 6. Fuente

- Atkinson Hyperlegible (licencia SIL OFL) convertida con `lv_font_conv --no-compress --bpp 4`.
- Tamaños 12, 12 negrita, 14, 14 negrita y 24 negrita (este último solo dígitos, `:`, `-` y A–Z).
- ASCII + Latin-1 (español completo) + `· ‹ › − – …`. Sin flechas ni ✓: el tic de las casillas usa el símbolo de Montserrat de LVGL.

## 7. Paleta "Claro"

- La T-Deck Plus tiene pantalla **IPS LCD**: la retroiluminación ilumina toda la pantalla por igual, así que **los colores no cambian el consumo**. Lo que ahorra batería es **bajar el brillo** y **apagar la pantalla antes**.
- Fondo `#EEF3F6`, superficie `#FFFFFF`, texto `#10212B` / `#4A5A66`, líneas `#C9D3DA` / `#DCE3E8`, desactivado `#8796A1`.
- Un color por sección: Mensajes azul `#1F63B5`, Mapa verde `#2E6B34`, Equipo morado `#3B2E8F`, Ajustes pizarra `#3F4E59`. Ámbar `#8A5A00` para datos antiguos y rojo `#C2271D` para SOS y acciones que borran.
- **Morado = "mi equipo"**: las opciones de Ajustes que dependen del equipo llevan una barra morada a la izquierda.

## 7b. Mapas base (pantalla Mapa)

- La MUI ya lee mapas desde la tarjeta SD y nuestra pantalla usa ese mismo sistema: teselas PNG de 256×256 px en `/maps/<nombre>/z/x/y.png`, o un archivo `/maps/<nombre>/<nombre>.pmtiles`. Cada carpeta es una capa que se elige en el botón de capas.
- Solo PNG: los .pmtiles con JPG o vectoriales no funcionan.
- Mapas propios desde QGIS: Procesamiento › Herramientas ráster › *Generar teselas XYZ (Directorio)*, formato PNG, zoom 10–17.
- Sin mapa base: cuadrícula UTM con equipo, puntos y ruta.
- Rutas en `/tfield/rutas/<nombre>.gpx` y puntos en `/tfield/puntos.gpx` (GPX 1.1, se abren en QGIS). En la misma carpeta: el índice de rutas (`indice-a.csv` e `indice-b.csv`, se alternan) y el respaldo de la ruta en curso (`_en_curso.csv`).

## 8. Pendientes y riesgos

- **Mantenimiento.** Cuando Meshtastic publique una versión nueva, habrá que traer sus cambios a nuestra copia. Por eso el código nuevo vive aislado.
- **Licencia GPL-3.** Uso personal o de la fundación sin problema. Si se distribuye el firmware, el código debe publicarse.
- **Brillo al sol.** La pantalla IPS de la T-Deck se lee mal a pleno sol; el alto contraste ayuda.
- **Funciones solo entre equipos con esta interfaz:** "Ver en el mapa" en puntos, invitación al canal privado y confirmación de unión. Con nodos normales, el punto llega igual como waypoint estándar.

## 9. Qué se necesita para empezar

1. Computador con Windows (o el PC con Linux Mint) con **VS Code + extensión PlatformIO**.
2. Cuenta de GitHub `Roberto-Larregla`: ahí vivirá la copia propia (fork) de `meshtastic/device-ui`.
3. Cable USB-C de datos para cargar el firmware en las T-Deck.

## 10. Ajustes y "mi equipo"

Los ajustes se guardan como texto `clave=valor` (`settings_to_text`) en la memoria interna, por ejemplo `/tfield/ajustes.txt`. Cada cambio llama a `on_settings_preview`, que aplica al tiro lo que se ve o se oye (brillo, volumen). Dos segundos después del último cambio, o al salir de Ajustes, llama a `on_settings`: el adaptador guarda el archivo y aplica en el firmware solo lo que cambió. Así, recorrer opciones con la bola no escribe la flash con cada toque.

| Sección | Opciones | Dónde se aplica |
|---|---|---|
| Pantalla | Brillo (20–100 %), apagar tras (15 s – 10 min, nunca), luz del teclado, tema (claro, oscuro, automático), idioma (español, inglés) | Firmware/MUI: brillo, `display.screen_on_secs`, teclado; tema e idioma los usa la interfaz |
| GPS y posición | GPS sí/no, enviar posición cada (1 min – 1 h), enviar antes si me muevo, punto de ruta cada (5 s – 1 min) | `position.gps_mode`, `position_broadcast_secs`, `position_broadcast_smart_enabled`; el intervalo de ruta lo usa la interfaz |
| Avisos | Volumen, probar, sonido para mi equipo / para otros / para puntos; SOS de otros siempre suena | Interfaz (`on_alert`) + sonido del equipo |
| Mensajes | Chats de mi equipo primero; editar respuestas rápidas; borrar mensajes de más de 1, 7 o 30 días; borrar todos los chats | Interfaz (`on_clear_chat` para el historial guardado) |
| Mapa y puntos | Nodos en el mapa (todos / mi equipo, también en Capas); puntos de mi equipo (guardar solos / preguntar); puntos de otros (preguntar / guardar / ignorar); «Todo mi equipo» al compartir | Interfaz |
| Canal privado | Crear, nombre (máx. 11 bytes), huella de la clave, estado de cada miembro, invitar, cambiar clave, quitar | Canal secundario de Meshtastic (`on_channel_set`) |
| SOS | Directo a cada miembro con confirmación, canal público, canal privado, repetir cada 1/2/5 min | Interfaz (`on_sos` con canales y nodos; `sos_ack` al confirmar) |
| Nodo y hora | Nombre corto (4), nombre largo, zona horaria (Chile continental, Magallanes, Isla de Pascua, UTC) | `setOwner`, `device.tzdef` (zona POSIX) |
| Avanzado | Región LoRa (Chile = ANZ), preset, saltos máximos (1–7). Se aplica con doble confirmación; reinicia la radio | `lora.region`, `lora.modem_preset`, `lora.hop_limit` |

Reglas de seguridad:

- El SOS nunca queda sin destino: si se intenta apagar el último, la interfaz no lo permite; y si aun así faltara, va por el canal principal.
- Las opciones de radio no se aplican al tocarlas: quedan pendientes hasta "Aplicar" + "Confirmar".

### Canal privado e invitación

- Un canal secundario con clave AES-256 aleatoria (`esp_fill_random` en la T-Deck). El nombre forma parte del canal: cambiarlo o cambiar la clave obliga a invitar de nuevo.
- **Invitación:** mensaje directo con nombre + clave, por un puerto privado de la app (`PRIVATE_APP`), **solo a nodos con clave pública conocida** (cifrado PKI de extremo a extremo). Sin clave pública no se envía, porque iría cifrado con la clave del canal principal, que es pública.
- Quien recibe ve en el chat «Te invita al canal privado …» con el botón **Unirse**. Al unirse, su equipo responde "me uní" y en el que invitó el miembro pasa a **unido**.
- La **huella** (FNV-1a de la clave, `A3F2 19C0`) permite comparar a simple vista que ambos equipos tienen la misma clave.
- Los repetidores retransmiten el canal sin necesitar la clave.

### Funciones de equipo

- **Compartir:** fila «Todo mi equipo (N)» que marca a todos los miembros (sin repetidores).
- **Mapa:** «Solo mi equipo» oculta los nodos ajenos.
- **Mensajes:** chats del equipo arriba; los chats ajenos sin sonido marcan los nuevos con número gris.
- **Puntos recibidos:** del equipo se guardan solos; de otros, botón «Guardar punto» en el chat.
- **Pedir posiciones:** botón en Equipo; muestra «2 de 3 · faltan VEH1».
- **SOS:** canales + directo a cada miembro; la pantalla muestra quién lo recibió y quién falta, y los reenvíos directos van solo a los que faltan.

## 11. Límites del equipo y pruebas

Las pruebas automáticas (46, más una prueba al azar de 5000 pasos con 12 semillas) corren con las cifras de la T-Deck Plus: ESP32-S3 a 240 MHz con coma flotante solo simple, 8 MB de PSRAM (5 MB para LVGL, como en Meshtastic), pantalla de 320×240 por SPI a 40 MHz (~31 ms por pantalla completa), hasta 250 nodos, teclado sin tecla mantenida ni Esc ni flechas, y bola con flechas y clic (4 movimientos por segundo). También pasan con LVGL 9.3.0, la versión de device-ui.

**Teclado y bola**
- La bola entra como flechas + Enter por `App::handle_key`. ↑/↓ recorren los elementos; sobre el mapa, Enter lo "toma" y las flechas lo mueven.
- *Borrar* fuera de un texto es "volver". *Shift+$* rota la tilde (i→í, n→ñ, ?→¿).
- Las acciones de "mantener" (SOS) se hacen manteniendo el clic de la bola, o con 3 Enter en 2 s desde el teclado.
- Si una pantalla aparece con la tecla aún apretada, el "soltar" no cuenta. Antes, el SOS hecho con la bola se cerraba solo al soltar.

**Pantalla (SPI y batería)**
- Solo se redibuja lo que cambia. Las líneas del mapa se dibujan en trozos de 24 puntos: un punto nuevo en la ruta redibuja 5,5 % de la pantalla, no 77 %.
- El orden de las capas del mapa solo se toca si cambió: moverlas obligaba a LVGL a redibujar el mapa entero.
- Topes medidos: minuto nuevo < 6 %, mismos datos 0 %, fix de GPS en el mapa < 6 %.

**CPU**
- Las rutas se proyectan una vez, en enteros. Los puntos a menos de 2 px se saltan y el recorte es cercano a la vista.
- Marcadores solo para lo que se ve: mi equipo, 40 nodos y 40 puntos, con nombre los 12 más cercanos.
- Mensajes: la burbuja nueva se agrega sin rehacer la conversación. Al abrir un chat se ven 15 y "Mensajes anteriores" carga de a 15.
- "Toda la red" muestra los 50 nodos ajenos más cercanos. Al compartir, el equipo y los 20 más cercanos.

**Memoria**
- Día pesado (250 nodos, 40 chats, 300 puntos, 10 rutas guardadas, 12 h grabando): 875 KB de datos, 668 KB máximo de LVGL y 8 teselas en caché (1 MB).
- Topes: 60 mensajes por chat y 600 en total; rastros de 200 puntos por compañero y 10 por nodo ajeno.
- Las rutas guardadas quedan en la SD: en memoria solo sus datos, y los puntos se leen al mostrar una.

**Tarjeta SD y flash**
- Ajustes se guardan 2 s después del último cambio, o al salir de Ajustes.
- La ruta en curso se respalda cada ~6 puntos (una escritura por minuto). Si se acaba la batería, al encender se recupera.
- El GPX se escribe por partes de 8 KB. Cambiar el nombre reescribe desde el archivo, sin perder puntos.
- El índice de rutas tiene dos copias, por si se corta la energía mientras se escribe.
- Sin tarjeta, nada se pierde: queda en memoria para reintentar.

**GPS sin barómetro**
- El desnivel cuenta subidas de más de 4 m: el ruido de altura no suma, y una subida lenta sí cuenta.
- La distancia se acumula sin redondear cada tramo. Antes, subir menos de 1 m por punto daba 0 m de desnivel.

**Para el adaptador (fase de integración)**
- Implementar `on_read_file`, `on_save_file`, `on_append_file` y `on_delete_file` sobre la SD.
- Al arrancar, llamar `load_routes()` y `recover_track()` con el contenido de `_en_curso.csv`.
- Fuente de teselas con caché de hasta 8 (128 KB cada una, en PSRAM).
- En ESP-IDF, las reservas pequeñas van primero a la RAM interna (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL`). Si la RAM interna baja, usar un asignador en PSRAM para las listas grandes.
- Los tiempos en el equipo se estiman como 100 veces los del computador, con prudencia. Hay que medirlos en la T-Deck con un registro de tiempos de redibujo.

## 12. Tema e idioma

- **Tema** en Ajustes › Pantalla: *Claro*, *Oscuro* o *Automático*. El automático usa la puesta y la salida del sol que ya calcula la T-Deck (sin posición, de 21:00 a 7:00). En la pantalla IPS el oscuro no ahorra batería: sirve de noche, para no encandilar.
- **Paleta oscura:** cada color de la paleta clara tiene su par oscuro (`theme.cpp`), con contraste de 4,5:1 o más en el texto normal (algo menos en lo desactivado). El mapa base se atenúa al 50 %.
- **Idioma:** español o inglés. Los textos del código pasan por `tr()`: la clave es el texto en español y la tabla `i18n_en.inc` tiene el inglés. Los datos (nombres, mensajes, puntos) no se traducen.
- También cambian con el idioma: distancias con coma o punto decimal, rumbos (SO → SW), fechas, "hace 5 min" → "5 min ago", y el texto del SOS que se envía por radio.
- **Respuestas rápidas:** si no las editaste, pasan a las del idioma nuevo.
- **Al cambiar de tema o idioma** se rehacen todas las pantallas, volviendo al mismo lugar: la pantalla abierta, el mapa (centro, zoom, destino), lo escrito en Mensajes y la historia de *volver*.
- El cambio automático al anochecer espera un momento tranquilo: en el inicio, o tras un minuto sin tocar nada, y nunca con un diálogo o un texto a medio escribir.
- **Pruebas:** el cambio repetido no pierde memoria; en inglés se recorren todas las pantallas y no queda texto en español.
- **Para el adaptador:** los estados de mensaje y los roles de nodo se entregan en español (*Enviado*, *Entregado*, *cliente*, *repetidor*…): la interfaz los traduce.

## 13. Borrar chats

- **Un chat:** en la conversación, botón *Borrar* arriba a la derecha. La primera pulsación pregunta (*¿Borrar?* en rojo); la segunda borra. Se desarma al mover el foco, cambiar de chat o salir de la pantalla.
- **Canal o directo:** un canal queda en la lista, vacío (sigue suscrito). Un chat directo desaparece.
- **Volver a escribir a alguien:** Equipo › ficha del nodo › *Mensaje* recrea el chat directo. También reaparece solo cuando el otro escribe.
- **Todos:** Ajustes › Mensajes › *Borrar todos los chats* (pide confirmar). Quedan solo los canales, vacíos.
- **Automático:** Ajustes › Mensajes › *Borrar mensajes de más de* Nunca / 1 día / 7 días / 30 días. Se revisa cada 10 minutos y al cambiar el ajuste. Cuenta desde la hora de llegada (`Message.epoch`); los mensajes sin hora conocida (antes de tener GPS o reloj) no se borran por antigüedad. Un directo que queda sin mensajes desaparece; un canal queda vacío.
- **Para el adaptador:** `on_clear_chat(chat_id, remove)` avisa para borrar el historial guardado. `remove` es verdadero si el chat directo desapareció. Si el adaptador conoce la hora real de un mensaje antiguo (al cargar el historial), la pone en `epoch`.
- **Cabecera:** con un nombre largo se corta con "…" y se oculta *mensaje directo*, para no pisar el botón.
