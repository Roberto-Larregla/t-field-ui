// T-Field UI — modo oscuro: cada color de la paleta clara tiene su par oscuro.
#include "tfield/theme.h"

namespace tfield {

bool g_dark = false;

static uint32_t mix(uint32_t a, uint32_t b, float t) // t = cuánto de b
{
    auto ch = [&](int s) {
        int x = (a >> s) & 0xFF, y = (b >> s) & 0xFF;
        return (uint32_t)(x + (y - x) * t + 0.5f) << s;
    };
    return ch(16) | ch(8) | ch(0);
}

uint32_t dark_of(uint32_t h)
{
    switch (h) {
    case col::BG: return 0x0F151A;
    case col::SURF: return 0x182128;
    case col::INK: return 0xE3EBF0;
    case col::INK2: return 0xA3B2BD;
    case col::LINE: return 0x3A4852;
    case col::LINE2: return 0x28333C;
    case col::MUTED: return 0x75858F;
    case col::BLUE: return 0x5C9CE6;
    case col::BLUE_DK: return 0x4785CF;
    case col::BLUE_BG: return 0x1C3350;
    case col::GREEN: return 0x67B56F;
    case col::PURPLE: return 0x9D90F0;
    case col::SLATE: return 0x9FB1BF;
    case col::AMBER: return 0xE2AC45;
    case col::AMBER_BG: return 0x352A12;
    case col::RED: return 0xF06B60;
    case 0xF4F2EC: return 0x161B1F; // mapa sin mapa base
    case 0xD8D3C4: return 0x2F3A41; // su cuadrícula
    }
    // otros (colores de cada nodo, rojo del SOS, marcadores): los claros pasan a un tono oscuro
    // y los demás se aclaran, para leer texto oscuro sobre ellos y verlos sobre el fondo oscuro
    int r = (h >> 16) & 0xFF, g = (h >> 8) & 0xFF, b = h & 0xFF;
    float luma = (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.0f;
    if (luma > 0.8f)
        return mix(h, 0x182128, 0.86f);
    return mix(h, 0xFFFFFF, 0.38f);
}

} // namespace tfield
