#include "tfield/gpx.h"
#include "tfield/format.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace tfield {

// Días desde 1970-01-01 (algoritmo de Howard Hinnant), sin depender de gmtime.
static int64_t days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

uint32_t epoch_from_utc(int year, int month, int day, int hour, int minute, int second)
{
    return (uint32_t)(days_from_civil(year, (unsigned)month, (unsigned)day) * 86400 + hour * 3600 + minute * 60 + second);
}

std::string iso8601(uint32_t epoch)
{
    int64_t z = epoch / 86400 + 719468;
    uint32_t secs = epoch % 86400;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int y = (int)(yoe + era * 400);
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp < 10 ? mp + 3 : mp - 9;
    y += m <= 2;
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02u-%02uT%02u:%02u:%02uZ", y, m, d, secs / 3600, (secs / 60) % 60, secs % 60);
    return buf;
}

std::string xml_escape(const std::string &s)
{
    std::string o;
    for (char ch : s) {
        switch (ch) {
        case '&': o += "&amp;"; break;
        case '<': o += "&lt;"; break;
        case '>': o += "&gt;"; break;
        case '"': o += "&quot;"; break;
        case '\'': o += "&apos;"; break;
        default:
            if ((unsigned char)ch < 0x20 && ch != '\n' && ch != '\t' && ch != '\r')
                break; // los caracteres de control dejarían el GPX inválido
            o += ch;
        }
    }
    return o;
}

std::string safe_filename(const std::string &name)
{
    std::string o;
    for (size_t i = 0; i < name.size(); i++) {
        unsigned char ch = (unsigned char)name[i];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_')
            o += (char)ch;
        else if (ch == 0xC3 && i + 1 < name.size()) { // vocales con tilde y ñ -> sin tilde
            unsigned char n = (unsigned char)name[++i];
            const char *map = nullptr;
            switch (n) {
            case 0xA1: map = "a"; break; case 0xA9: map = "e"; break; case 0xAD: map = "i"; break;
            case 0xB3: map = "o"; break; case 0xBA: map = "u"; break; case 0xBC: map = "u"; break;
            case 0xB1: map = "n"; break; case 0x81: map = "A"; break; case 0x89: map = "E"; break;
            case 0x8D: map = "I"; break; case 0x93: map = "O"; break; case 0x9A: map = "U"; break;
            case 0x91: map = "N"; break;
            }
            if (map)
                o += map;
        } else if (!o.empty() && o.back() != '_')
            o += '_';
    }
    if (o.size() > 40)
        o.resize(40);
    while (!o.empty() && o.back() == '_')
        o.pop_back();
    return o.empty() ? "ruta" : o;
}

static const char *HEAD = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                          "<gpx version=\"1.1\" creator=\"T-Field UI\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n";

std::string gpx_track_head(const std::string &name, const std::string &note, const Track &t)
{
    std::string o = HEAD;
    o += "<metadata><name>" + xml_escape(name) + "</name>";
    if (!t.pts.empty() && t.pts.front().epoch)
        o += "<time>" + iso8601(t.pts.front().epoch) + "</time>";
    o += "</metadata>\n<trk><name>" + xml_escape(name) + "</name>";
    if (!note.empty())
        o += "<desc>" + xml_escape(note) + "</desc>";
    o += "<trkseg>\n";
    return o;
}

void gpx_track_point(std::string &o, const TrackPoint &p)
{
    char buf[160];
    snprintf(buf, sizeof(buf), "<trkpt lat=\"%.7f\" lon=\"%.7f\"><ele>%.1f</ele>", p.lat, p.lon, p.alt);
    o += buf;
    if (p.epoch)
        o += "<time>" + iso8601(p.epoch) + "</time>";
    o += "</trkpt>\n";
}

std::string gpx_track_tail() { return "</trkseg></trk>\n</gpx>\n"; }

std::string gpx_track(const std::string &name, const std::string &note, const Track &t)
{
    std::string o = gpx_track_head(name, note, t);
    for (const TrackPoint &p : t.pts)
        gpx_track_point(o, p);
    return o + gpx_track_tail();
}

uint32_t parse_iso8601(const char *s)
{
    int y, mo, d, h, mi;
    double sec = 0;
    if (std::sscanf(s, "%4d-%2d-%2dT%2d:%2d:%lf", &y, &mo, &d, &h, &mi, &sec) < 5)
        return 0;
    if (y < 1970 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59)
        return 0;
    return epoch_from_utc(y, mo, d, h, mi, (int)sec);
}

bool gpx_parse_track(const std::string &g, std::vector<TrackPoint> &out)
{
    out.clear();
    size_t n = 0; // capacidad justa: un vector que crece al doble reservaría hasta 2×
    for (size_t q = g.find("<trkpt"); q != std::string::npos; q = g.find("<trkpt", q + 6))
        n++;
    out.reserve(n);
    size_t pos = 0;
    while ((pos = g.find("<trkpt", pos)) != std::string::npos) {
        size_t tag_end = g.find('>', pos);
        if (tag_end == std::string::npos)
            break;
        bool self_closed = tag_end > 0 && g[tag_end - 1] == '/';
        size_t close = self_closed ? tag_end : g.find("</trkpt>", tag_end);
        if (close == std::string::npos)
            close = g.size();
        auto attr = [&](const char *key, double &v) {
            size_t a = g.find(key, pos);
            if (a == std::string::npos || a > tag_end)
                return false;
            a += std::strlen(key);
            char q = g[a];
            if (q != '"' && q != '\'')
                return false;
            v = std::strtod(g.c_str() + a + 1, nullptr);
            return true;
        };
        TrackPoint p;
        if (attr(" lat=", p.lat) && attr(" lon=", p.lon) && p.lat >= -90 && p.lat <= 90 && p.lon >= -180 && p.lon <= 180) {
            size_t e = g.find("<ele>", tag_end);
            if (e != std::string::npos && e < close)
                p.alt = (float)std::strtod(g.c_str() + e + 5, nullptr);
            size_t t = g.find("<time>", tag_end);
            if (t != std::string::npos && t < close)
                p.epoch = parse_iso8601(g.c_str() + t + 6);
            out.push_back(p);
        }
        pos = close == g.size() ? g.size() : close + 1;
    }
    return !out.empty();
}

bool gpx_rename(std::string &g, const std::string &name, const std::string &note)
{
    size_t trk = g.find("<trk>");
    if (trk == std::string::npos)
        return false;
    size_t seg = g.find("<trkseg>", trk);
    if (seg == std::string::npos)
        return false;
    std::string head = "<trk><name>" + xml_escape(name) + "</name>";
    if (!note.empty())
        head += "<desc>" + xml_escape(note) + "</desc>";
    size_t meta = g.find("<metadata><name>");
    g.replace(trk, seg - trk, head); // primero lo de más abajo: lo de arriba no se corre
    if (meta != std::string::npos && meta < trk) {
        size_t a = meta + std::strlen("<metadata><name>"), b2 = g.find("</name>", a);
        if (b2 != std::string::npos && b2 < trk)
            g.replace(a, b2 - a, xml_escape(name));
    }
    return true;
}

// en el índice, ';' separa campos: se escapan ';', '%' y saltos de línea
static std::string esc(const std::string &s)
{
    std::string o;
    for (char ch : s) {
        if (ch == ';' || ch == '%' || ch == '\n' || ch == '\r') {
            char b[4];
            snprintf(b, sizeof(b), "%%%02X", (unsigned char)ch);
            o += b;
        } else
            o += ch;
    }
    return o;
}

static std::string unesc(const std::string &s)
{
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size() && std::isxdigit((unsigned char)s[i + 1]) && std::isxdigit((unsigned char)s[i + 2])) {
            o += (char)std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16);
            i += 2;
        } else
            o += s[i];
    }
    return o;
}

std::string route_index(const std::vector<SavedTrack> &routes, uint32_t gen)
{
    std::string o = "tfield-rutas;1\n";
    char buf[320];
    for (auto &r : routes) {
        snprintf(buf, sizeof(buf), ";%u;%.7f;%.7f;%.1f;%u;%ld;%u;%ld;%.7f;%.7f;%.7f;%.7f\n", (unsigned)r.count, r.start.lat,
                 r.start.lon, r.start.alt, (unsigned)r.start.epoch, (long)r.track.distance_m, (unsigned)r.track.duration_s,
                 (long)r.track.gain_m, r.lat0, r.lat1, r.lon0, r.lon1);
        o += std::to_string(r.id) + ";" + esc(r.file) + ";" + esc(r.name) + ";" + esc(r.note) + buf;
    }
    return o + "fin;" + std::to_string(gen) + "\n";
}

bool parse_route_index(const std::string &text, std::vector<SavedTrack> &out, uint32_t &gen)
{
    out.clear();
    if (text.rfind("tfield-rutas;", 0) != 0)
        return false;
    bool done = false;
    size_t pos = text.find('\n');
    while (pos != std::string::npos && pos + 1 < text.size()) {
        size_t end = text.find('\n', pos + 1);
        std::string line = text.substr(pos + 1, end == std::string::npos ? std::string::npos : end - pos - 1);
        pos = end;
        if (line.rfind("fin;", 0) == 0) {
            gen = (uint32_t)std::strtoul(line.c_str() + 4, nullptr, 10);
            done = true;
            break;
        }
        std::vector<std::string> f;
        size_t a = 0;
        for (;;) {
            size_t b2 = line.find(';', a);
            f.push_back(line.substr(a, b2 == std::string::npos ? std::string::npos : b2 - a));
            if (b2 == std::string::npos)
                break;
            a = b2 + 1;
        }
        if (f.size() != 16)
            continue; // línea dañada: se salta
        SavedTrack r;
        r.id = (uint32_t)std::strtoul(f[0].c_str(), nullptr, 10);
        r.file = unesc(f[1]);
        r.name = unesc(f[2]);
        r.note = unesc(f[3]);
        r.count = (uint32_t)std::strtoul(f[4].c_str(), nullptr, 10);
        r.start.lat = std::strtod(f[5].c_str(), nullptr);
        r.start.lon = std::strtod(f[6].c_str(), nullptr);
        r.start.alt = (float)std::strtod(f[7].c_str(), nullptr);
        r.start.epoch = (uint32_t)std::strtoul(f[8].c_str(), nullptr, 10);
        r.track.distance_m = (int32_t)std::strtol(f[9].c_str(), nullptr, 10);
        r.track.duration_s = (uint32_t)std::strtoul(f[10].c_str(), nullptr, 10);
        r.track.gain_m = (int32_t)std::strtol(f[11].c_str(), nullptr, 10);
        r.lat0 = std::strtod(f[12].c_str(), nullptr);
        r.lat1 = std::strtod(f[13].c_str(), nullptr);
        r.lon0 = std::strtod(f[14].c_str(), nullptr);
        r.lon1 = std::strtod(f[15].c_str(), nullptr);
        if (r.id == 0 || r.file.empty())
            continue;
        out.push_back(r);
    }
    return done; // sin "fin": el archivo quedó a medio escribir (se cortó la energía)
}

void route_meta(SavedTrack &st, const std::vector<TrackPoint> &pts)
{
    st.count = (uint32_t)pts.size();
    if (pts.empty())
        return;
    st.start = pts.front();
    st.lat0 = st.lat1 = pts[0].lat;
    st.lon0 = st.lon1 = pts[0].lon;
    for (size_t i = 0; i < pts.size(); i++) {
        st.lat0 = std::min(st.lat0, pts[i].lat), st.lat1 = std::max(st.lat1, pts[i].lat);
        st.lon0 = std::min(st.lon0, pts[i].lon), st.lon1 = std::max(st.lon1, pts[i].lon);
    }
    int32_t dist = 0, gain = 0;
    track_totals(pts, dist, gain);
    if (!st.track.distance_m)
        st.track.distance_m = dist;
    if (!st.track.gain_m)
        st.track.gain_m = gain;
    if (!st.track.duration_s && pts.back().epoch > pts.front().epoch)
        st.track.duration_s = pts.back().epoch - pts.front().epoch;
}

std::string gpx_waypoints(const std::vector<Waypoint> &wps)
{
    std::string o = HEAD;
    char buf[160];
    for (const Waypoint &w : wps) {
        snprintf(buf, sizeof(buf), "<wpt lat=\"%.7f\" lon=\"%.7f\"><ele>%d</ele>", w.lat, w.lon, (int)w.alt);
        o += buf;
        if (w.epoch)
            o += "<time>" + iso8601(w.epoch) + "</time>";
        o += "<name>" + xml_escape(w.name) + "</name>";
        if (!w.note.empty())
            o += "<desc>" + xml_escape(w.note) + "</desc>";
        o += "</wpt>\n";
    }
    o += "</gpx>\n";
    return o;
}

} // namespace tfield
