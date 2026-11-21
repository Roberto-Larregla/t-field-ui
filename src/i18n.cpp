// T-Field UI — tabla de traducción al inglés (las claves son los textos en español del código).
#include "tfield/i18n.h"

#include <algorithm>
#include <cstring>

namespace tfield {

Lang g_lang = Lang::Es;

static const char *const TABLE[][2] = {
#include "i18n_en.inc"
};
static constexpr size_t N = sizeof(TABLE) / sizeof(TABLE[0]);

std::vector<std::string> &tr_missing()
{
    static std::vector<std::string> v;
    return v;
}

const char *tr(const char *es)
{
    if (g_lang == Lang::Es || !es || !*es)
        return es;
    // índice ordenado (una vez): búsqueda binaria sin copiar textos
    static std::vector<uint16_t> idx;
    if (idx.empty()) {
        idx.resize(N);
        for (size_t i = 0; i < N; i++)
            idx[i] = (uint16_t)i;
        std::sort(idx.begin(), idx.end(), [](uint16_t a, uint16_t b) { return std::strcmp(TABLE[a][0], TABLE[b][0]) < 0; });
    }
    auto it = std::lower_bound(idx.begin(), idx.end(), es, [](uint16_t i, const char *k) { return std::strcmp(TABLE[i][0], k) < 0; });
    if (it != idx.end() && std::strcmp(TABLE[*it][0], es) == 0)
        return TABLE[*it][1];
    auto &miss = tr_missing();
    if (miss.size() < 64 && std::find(miss.begin(), miss.end(), es) == miss.end())
        miss.push_back(es);
    return es;
}

} // namespace tfield
