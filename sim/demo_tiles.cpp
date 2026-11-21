// Mapa base de ejemplo para el simulador: un valle sintético con curvas de nivel,
// sombreado, río y camino, generado en el momento (no son datos reales).
#include "demo.h"
#include "tfield/i18n.h"
#include "tfield/format.h"

#include <cmath>
#include <map>
#include <memory>

namespace demo {

using namespace tfield;

static const double LAT0 = MY_LAT, LON0 = MY_LON;
static const double M_LAT = 111320.0;
static const double M_LON = 111320.0 * std::cos(MY_LAT * 3.14159265358979 / 180.0);

static double valley_axis(double e) { return 0.75 * e + 180.0 * std::sin(e / 700.0) - 120.0; }

double elevation(double e, double n)
{
    double d = n - valley_axis(e);
    double ad = std::fabs(d);
    double walls = ad < 800 ? 0.25 * ad : 200 + 0.08 * (ad - 800);
    double floor_ = 1360 + 0.05 * (e * 0.6 + n * 0.8);
    double hills = 55 * std::sin(e / 430.0 + 1.3) * std::cos(n / 510.0 - 0.4) + 22 * std::sin(e / 170.0 + n / 230.0);
    double fade = std::min(1.0, ad / 120.0); // el fondo del valle es plano
    return floor_ + walls + hills * fade;
}

void local_to_latlon(double e, double n, double &lat, double &lon)
{
    lat = LAT0 + n / M_LAT;
    lon = LON0 + e / M_LON;
}

static inline uint16_t rgb565(int r, int g, int b)
{
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

struct Tile {
    std::vector<uint16_t> px;
    lv_image_dsc_t dsc{};
};

class DemoTopo : public TileSource {
  public:
    const char *name() const override { return tfield::english() ? "Topographic (sample)" : "Topogr\xC3\xA1" "fico (ejemplo)"; }
    const char *attribution() const override { return tfield::english() ? "sample map" : "mapa de ejemplo"; }
    uint8_t min_zoom() const override { return 11; }
    uint8_t max_zoom() const override { return 17; }

    size_t cached() const { return cache_.size(); }

    const lv_image_dsc_t *tile(uint8_t z, uint32_t x, uint32_t y) override
    {
        uint64_t k = key(z, x, y);
        auto it = cache_.find(k);
        if (it != cache_.end())
            return &it->second->dsc;
        auto t = std::make_unique<Tile>();
        render(*t, z, x, y);
        const lv_image_dsc_t *d = &t->dsc;
        cache_[k] = std::move(t);
        return d;
    }

    // Como en la T-Deck: una tesela de 256×256 en RGB565 son 128 KB; la caché guarda las que se
    // ven (hasta 6) y un par más, ~1 MB de PSRAM como máximo
    void retain(const std::vector<uint64_t> &keys) override
    {
        if (cache_.size() <= 8)
            return;
        for (auto it = cache_.begin(); it != cache_.end() && cache_.size() > 8;) {
            bool keep = false;
            for (auto kk : keys)
                keep |= kk == it->first;
            if (keep)
                ++it;
            else
                it = cache_.erase(it);
        }
    }

  private:
    void render(Tile &t, uint8_t z, uint32_t tx, uint32_t ty)
    {
        const int N = 256, G = 65, S = 4; // rejilla de elevación cada 4 px
        static double hgrid[G * G];
        double mpp = meters_per_px(LAT0, z);
        for (int j = 0; j < G; j++)
            for (int i = 0; i < G; i++) {
                double lat, lon;
                mercator_latlon(tx * 256.0 + i * S, ty * 256.0 + j * S, z, lat, lon);
                hgrid[j * G + i] = elevation((lon - LON0) * M_LON, (lat - LAT0) * M_LAT);
            }
        auto H = [&](int px, int py) {
            int i = px / S, j = py / S;
            double fx = (px % S) / (double)S, fy = (py % S) / (double)S;
            double a = hgrid[j * G + i], b = hgrid[j * G + i + 1], c2 = hgrid[(j + 1) * G + i], d = hgrid[(j + 1) * G + i + 1];
            return (a * (1 - fx) + b * fx) * (1 - fy) + (c2 * (1 - fx) + d * fx) * fy;
        };
        double interval = z >= 16 ? 10 : z == 15 ? 20 : z == 14 ? 40 : z == 13 ? 100 : 200;
        double river_w = std::max(1.2 * mpp, 6.0), road_w = std::max(1.0 * mpp, 3.5);

        t.px.resize(N * N);
        std::vector<double> row(N + 1), next(N + 1);
        for (int py = 0; py < N; py++) {
            for (int px = 0; px <= N; px++) {
                row[px] = H(std::min(px, 255), py);
                next[px] = H(std::min(px, 255), std::min(py + 1, 255));
            }
            for (int px = 0; px < N; px++) {
                double h = row[px];
                double gx = (row[px + 1] - h) / mpp, gy = (next[px] - h) / mpp;
                // luz desde el noroeste
                double shade = 0.5 + std::clamp((-gx - gy) * 0.9, -0.45, 0.45);
                double lat, lon;
                mercator_latlon(tx * 256.0 + px, ty * 256.0 + py, z, lat, lon);
                double e = (lon - LON0) * M_LON, n = (lat - LAT0) * M_LAT;
                double d = n - valley_axis(e);

                // papel + sombreado + vegetación en el fondo del valle
                double base_r = 243, base_g = 240, base_b = 230;
                if (std::fabs(d) < 260) {
                    double v = 1 - std::fabs(d) / 260;
                    base_r -= 18 * v;
                    base_g -= 6 * v;
                    base_b -= 22 * v;
                }
                double k = 0.80 + 0.40 * shade;
                int r = (int)(base_r * k), g = (int)(base_g * k), b = (int)(base_b * k);

                // curvas de nivel
                long lv = (long)std::floor(h / interval);
                long lr = (long)std::floor(row[px + 1] / interval), ld = (long)std::floor(next[px] / interval);
                if (lv != lr || lv != ld) {
                    long top = std::max(lv, std::max(lr, ld));
                    bool index = (top % 5) == 0;
                    if (index) {
                        r = 150, g = 104, b = 60;
                    } else {
                        r = (r + 190) / 2, g = (g + 150) / 2, b = (b + 105) / 2;
                    }
                }
                // río y camino
                if (std::fabs(d) < river_w / 2)
                    r = 74, g = 144, b = 200;
                else if (std::fabs(d - 45) < road_w / 2)
                    r = 205, g = 120, b = 40;
                else if (std::fabs(d - 45) < road_w / 2 + 0.8 * mpp)
                    r = 120, g = 80, b = 40;
                t.px[py * N + px] = rgb565(r, g, b);
            }
        }
        t.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        t.dsc.header.cf = LV_COLOR_FORMAT_RGB565;
        t.dsc.header.w = N;
        t.dsc.header.h = N;
        t.dsc.header.stride = N * 2;
        t.dsc.data_size = N * N * 2;
        t.dsc.data = reinterpret_cast<const uint8_t *>(t.px.data());
    }

    std::map<uint64_t, std::unique_ptr<Tile>> cache_;
};

TileSource *topo_layer()
{
    static DemoTopo topo;
    return &topo;
}

size_t tiles_cached() { return static_cast<DemoTopo *>(topo_layer())->cached(); }

} // namespace demo
