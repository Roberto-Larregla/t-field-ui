// T-Field UI — idioma de la interfaz (español o inglés).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tfield {

enum class Lang : uint8_t { Es = 0, En = 1 };
extern Lang g_lang;
inline bool english() { return g_lang == Lang::En; }

// Texto de la interfaz en el idioma elegido. La clave es el texto en español tal cual está en
// el código; sin traducción, queda en español. Los datos (nombres, mensajes) no pasan por aquí.
const char *tr(const char *es);
inline const char *tr(const std::string &es) = delete; // solo literales: un dato no se traduce

// Para las pruebas: textos pedidos en inglés que no tienen traducción
std::vector<std::string> &tr_missing();

} // namespace tfield
