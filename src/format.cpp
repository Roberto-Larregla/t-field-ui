#include "tfield/format.h"
#include "tfield/i18n.h"

#include <cmath>
#include <cstdio>

namespace tfield {

static constexpr double PI = 3.14159265358979323846;
static inline double rad(double d) { return d * PI / 180.0; }
static inline double deg(double r) { return r * 180.0 / PI; }

std::string format_distance(int32_t m)
{
    char buf[24];
    if (m < 0)
        return "\xE2\x80\x94"; // —
    if (m < 995) // se redondea a 10 m; 995 m ya es "1,0 km"
        snprintf(buf, sizeof(buf), "%d m", (int)((m + 5) / 10 * 10));
    else if (m < 9950) { // redondeo a 100 m
        int hm = (m + 50) / 100;
        snprintf(buf, sizeof(buf), english() ? "%d.%d km" : "%d,%d km", hm / 10, hm % 10);
    } else
        snprintf(buf, sizeof(buf), "%d km", (int)(((int64_t)m + 500) / 1000));
    return buf;
}

std::string format_alt_diff(int32_t m)
{
    char buf[16];
    if (m >= 0)
        snprintf(buf, sizeof(buf), "+%d m", (int)m);
    else
        snprintf(buf, sizeof(buf), "\xE2\x88\x92%d m", (int)-m); // U+2212 signo menos
    return buf;
}

const char *cardinal(int b)
{
    static const char *names[] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
    static const char *names_en[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    b %= 360;
    if (b < 0)
        b += 360;
    // un puntero, no "(c ? a : b)[i]": GCC 13 con UBSan compila mal ese subíndice sobre arreglos
    const char *const *tbl = english() ? names_en : names;
    return tbl[((b * 2 + 45) / 90) % 8];
}

std::string format_hops(int hops)
{
    if (hops < 0)
        return "";
    if (hops == 0)
        return english() ? "direct" : "directo";
    if (english())
        return hops == 1 ? "1 hop" : std::to_string(hops) + " hops";
    return hops == 1 ? "1 salto" : std::to_string(hops) + " saltos";
}

std::string format_ago(uint32_t s)
{
    char buf[16];
    if (s < 60)
        return english() ? "now" : "ahora";
    if (s < 3600)
        snprintf(buf, sizeof(buf), "%u min", (unsigned)(s / 60));
    else if (s < 86400)
        snprintf(buf, sizeof(buf), "%u h", (unsigned)(s / 3600));
    else
        snprintf(buf, sizeof(buf), "%u d", (unsigned)(s / 86400));
    return buf;
}

std::string ago_text(uint32_t s)
{
    if (s < 60)
        return format_ago(s);
    return english() ? format_ago(s) + " ago" : "hace " + format_ago(s);
}

std::string format_daylight(int32_t minutes)
{
    char buf[16];
    if (minutes < 0)
        return "—";
    if (minutes < 60)
        snprintf(buf, sizeof(buf), "%d min", (int)minutes);
    else
        snprintf(buf, sizeof(buf), "%d h %02d", (int)(minutes / 60), (int)(minutes % 60));
    return buf;
}

size_t utf8_length(const std::string &s)
{
    size_t n = 0;
    for (unsigned char ch : s)
        if ((ch & 0xC0) != 0x80)
            n++;
    return n;
}

std::string utf8_truncate(const std::string &s, size_t max_chars)
{
    size_t n = 0, i = 0;
    for (; i < s.size(); i++) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
            if (n == max_chars)
                break;
            n++;
        }
    }
    return s.substr(0, i);
}

double haversine_m(double lat1, double lon1, double lat2, double lon2)
{
    const double R = 6371000.0;
    double dlat = rad(lat2 - lat1), dlon = rad(lon2 - lon1);
    double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
               std::cos(rad(lat1)) * std::cos(rad(lat2)) * std::sin(dlon / 2) * std::sin(dlon / 2);
    return 2 * R * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}

double bearing_deg(double lat1, double lon1, double lat2, double lon2)
{
    double y = std::sin(rad(lon2 - lon1)) * std::cos(rad(lat2));
    double x = std::cos(rad(lat1)) * std::sin(rad(lat2)) - std::sin(rad(lat1)) * std::cos(rad(lat2)) * std::cos(rad(lon2 - lon1));
    double b = deg(std::atan2(y, x));
    return b < 0 ? b + 360 : b;
}

Utm to_utm(double lat, double lon)
{
    const double a = 6378137.0, f = 1 / 298.257223563, k0 = 0.9996;
    const double e2 = f * (2 - f), ep2 = e2 / (1 - e2);
    int zone = (int)std::floor((lon + 180) / 6) + 1;
    if (zone > 60)
        zone = 60;
    double lon0 = rad((zone - 1) * 6 - 180 + 3);
    double phi = rad(lat), lam = rad(lon);
    double N = a / std::sqrt(1 - e2 * std::sin(phi) * std::sin(phi));
    double T = std::tan(phi) * std::tan(phi);
    double C = ep2 * std::cos(phi) * std::cos(phi);
    double A = std::cos(phi) * (lam - lon0);
    double e4 = e2 * e2, e6 = e4 * e2;
    double M = a * ((1 - e2 / 4 - 3 * e4 / 64 - 5 * e6 / 256) * phi - (3 * e2 / 8 + 3 * e4 / 32 + 45 * e6 / 1024) * std::sin(2 * phi) +
                    (15 * e4 / 256 + 45 * e6 / 1024) * std::sin(4 * phi) - (35 * e6 / 3072) * std::sin(6 * phi));
    Utm u;
    u.zone = zone;
    u.e = k0 * N * (A + (1 - T + C) * A * A * A / 6 + (5 - 18 * T + T * T + 72 * C - 58 * ep2) * std::pow(A, 5) / 120) + 500000.0;
    u.n = k0 * (M + N * std::tan(phi) *
                        (A * A / 2 + (5 - T + 9 * C + 4 * C * C) * std::pow(A, 4) / 24 +
                         (61 - 58 * T + T * T + 600 * C - 330 * ep2) * std::pow(A, 6) / 720));
    if (lat < 0)
        u.n += 10000000.0;
    static const char bands[] = "CDEFGHJKLMNPQRSTUVWXX";
    int bi = (int)std::floor((lat + 80) / 8);
    u.band = (bi >= 0 && bi < 21) ? bands[bi] : 'Z';
    return u;
}

static std::string group3(long v)
{
    std::string s = std::to_string(v), out;
    int n = 0;
    for (int i = (int)s.size() - 1; i >= 0; i--) {
        out.insert(out.begin(), s[i]);
        if (++n % 3 == 0 && i > 0)
            out.insert(out.begin(), ' ');
    }
    return out;
}

std::string format_utm(const Utm &u)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "%d%c %s E  %s N", u.zone, u.band, group3(std::lround(u.e)).c_str(),
             group3(std::lround(u.n)).c_str());
    return buf;
}

void mercator_px(double lat, double lon, int zoom, double &x, double &y)
{
    lat = std::fmax(-85.05112878, std::fmin(85.05112878, lat)); // límite de Web Mercator
    double world = 256.0 * std::pow(2.0, zoom);
    x = (lon + 180.0) / 360.0 * world;
    double s = std::sin(rad(lat));
    y = (0.5 - std::log((1 + s) / (1 - s)) / (4 * PI)) * world;
}

void mercator_latlon(double x, double y, int zoom, double &lat, double &lon)
{
    double world = 256.0 * std::pow(2.0, zoom);
    lon = x / world * 360.0 - 180.0;
    double n = PI - 2.0 * PI * y / world;
    lat = deg(std::atan(std::sinh(n)));
}

double meters_per_px(double lat, int zoom)
{
    return 156543.03392 * std::cos(rad(lat)) / std::pow(2.0, zoom);
}

// NOAA General Solar Position (precisión de ~1-2 min, suficiente para "luz restante").
bool sun_times_utc(int year, int month, int day, double lat, double lon, double &rise, double &set)
{
    static const int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    int doy = cum[month - 1] + day + ((leap && month > 2) ? 1 : 0);
    double g = 2 * PI / (leap ? 366 : 365) * (doy - 1);
    double eqtime = 229.18 * (0.000075 + 0.001868 * std::cos(g) - 0.032077 * std::sin(g) - 0.014615 * std::cos(2 * g) -
                              0.040849 * std::sin(2 * g));
    double decl = 0.006918 - 0.399912 * std::cos(g) + 0.070257 * std::sin(g) - 0.006758 * std::cos(2 * g) +
                  0.000907 * std::sin(2 * g) - 0.002697 * std::cos(3 * g) + 0.00148 * std::sin(3 * g);
    double cosha = std::cos(rad(90.833)) / (std::cos(rad(lat)) * std::cos(decl)) - std::tan(rad(lat)) * std::tan(decl);
    if (cosha < -1 || cosha > 1)
        return false;
    double ha = deg(std::acos(cosha));
    rise = 720 - 4 * (lon + ha) - eqtime;
    set = 720 - 4 * (lon - ha) - eqtime;
    return true;
}

DaylightInfo daylight_info(uint32_t epoch, int tz_offset_min, double lat, double lon)
{
    DaylightInfo d;
    if (!epoch)
        return d;
    // día local de hoy (la puesta que importa es la de la fecha local)
    int64_t local = (int64_t)epoch + (int64_t)tz_offset_min * 60;
    int64_t days = local >= 0 ? local / 86400 : (local - 86399) / 86400;
    // fecha civil desde días (Howard Hinnant)
    int64_t z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097), yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int y = (int)(yoe + era * 400);
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    unsigned dd = doy - (153 * mp + 2) / 5 + 1, mm = mp < 10 ? mp + 3 : mp - 9;
    y += mm <= 2;
    double rise, set;
    if (!sun_times_utc(y, (int)mm, (int)dd, lat, lon, rise, set))
        return d;
    // a minutos locales desde la medianoche local de hoy
    double rise_l = rise + tz_offset_min, set_l = set + tz_offset_min;
    auto hhmm = [](double minutes) {
        int m = (int)std::lround(minutes);
        m = ((m % 1440) + 1440) % 1440;
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d", m / 60, m % 60);
        return std::string(buf);
    };
    d.valid = true;
    d.sunrise = hhmm(rise_l);
    d.sunset = hhmm(set_l);
    double now_l = (double)(local - days * 86400) / 60.0;
    d.minutes_left = (now_l >= rise_l && now_l < set_l) ? (int32_t)std::lround(set_l - now_l) : 0; // de noche: 0
    d.night = d.minutes_left == 0;
    return d;
}

void climb_add(double &gain, float &ref, float alt)
{
    if (alt > ref + CLIMB_STEP_M) {
        gain += alt - ref;
        ref = alt;
    } else if (alt < ref - CLIMB_STEP_M)
        ref = alt;
}

void track_totals(const std::vector<TrackPoint> &pts, int32_t &distance_m, int32_t &gain_m)
{
    double dist = 0, gain = 0;
    float ref = pts.empty() ? 0 : pts[0].alt;
    for (size_t i = 1; i < pts.size(); i++) {
        dist += haversine_m(pts[i - 1].lat, pts[i - 1].lon, pts[i].lat, pts[i].lon);
        climb_add(gain, ref, pts[i].alt);
    }
    distance_m = (int32_t)std::lround(dist);
    gain_m = (int32_t)std::lround(gain);
}

} // namespace tfield
