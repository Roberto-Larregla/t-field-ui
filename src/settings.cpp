// Ajustes: listas de opciones y guardado en texto.
#include "tfield/settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace tfield {

ChannelMember *Settings::member(uint32_t num)
{
    for (auto &m : members)
        if (m.num == num)
            return &m;
    return nullptr;
}

const ChannelMember *Settings::member(uint32_t num) const
{
    for (auto &m : members)
        if (m.num == num)
            return &m;
    return nullptr;
}

const std::vector<Option> &time_zones()
{
    static const std::vector<Option> v = {
        {"<-04>4<-03>,M9.1.6/24,M4.1.6/24", "Chile continental", "UTC\xE2\x88\x92" "4, y \xE2\x88\x92" "3 en horario de verano"},
        {"<-03>3", "Magallanes", "UTC\xE2\x88\x92" "3 todo el a\xC3\xB1o"},
        {"<-06>6<-05>,M9.1.6/22,M4.1.6/22", "Isla de Pascua", "UTC\xE2\x88\x92" "6, y \xE2\x88\x92" "5 en horario de verano"},
        {"UTC0", "UTC", "Hora universal, sin horario de verano"},
    };
    return v;
}

const std::vector<Option> &lora_regions()
{
    static const std::vector<Option> v = {
        {"ANZ", "ANZ", "915\xE2\x80\x93" "928 MHz \xC2\xB7 la que corresponde a Chile"},
        {"US", "US", "902\xE2\x80\x93" "928 MHz \xC2\xB7 Estados Unidos y Canad\xC3\xA1"},
    };
    return v;
}

const std::vector<Option> &lora_presets()
{
    static const std::vector<Option> v = {
        {"LONG_FAST", "LongFast", "Por defecto \xC2\xB7 largo alcance, velocidad media"},
        {"LONG_MODERATE", "LongModerate", "M\xC3\xA1s alcance, m\xC3\xA1s lento"},
        {"MEDIUM_SLOW", "MediumSlow", "Alcance medio"},
        {"MEDIUM_FAST", "MediumFast", "Alcance medio, m\xC3\xA1s r\xC3\xA1pido"},
        {"SHORT_SLOW", "ShortSlow", "Corto alcance"},
        {"SHORT_FAST", "ShortFast", "Corto alcance, r\xC3\xA1pido"},
        {"SHORT_TURBO", "ShortTurbo", "Muy corto, el m\xC3\xA1s r\xC3\xA1pido (500 kHz)"},
    };
    return v;
}

// ------------------------------------------------------------- base64
static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const uint8_t *d, size_t n)
{
    std::string out;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)d[i] << 16;
        if (i + 1 < n)
            v |= (uint32_t)d[i + 1] << 8;
        if (i + 2 < n)
            v |= d[i + 2];
        out += B64[(v >> 18) & 63];
        out += B64[(v >> 12) & 63];
        out += i + 1 < n ? B64[(v >> 6) & 63] : '=';
        out += i + 2 < n ? B64[v & 63] : '=';
    }
    return out;
}

bool base64_decode(const std::string &s, std::vector<uint8_t> &out)
{
    out.clear();
    uint32_t v = 0;
    int bits = 0;
    for (char ch : s) {
        if (ch == '=' || ch == '\r' || ch == '\n' || ch == ' ')
            continue;
        const char *p = std::strchr(B64, ch);
        if (!p || !*p)
            return false;
        v = (v << 6) | (uint32_t)(p - B64);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)(v >> bits));
        }
    }
    return true;
}

std::string key_fingerprint(const ChannelKey &key)
{
    uint32_t h = 2166136261u; // FNV-1a: no revela bytes de la clave
    for (uint8_t b : key) {
        h ^= b;
        h *= 16777619u;
    }
    char buf[12];
    snprintf(buf, sizeof(buf), "%04X %04X", (unsigned)(h >> 16), (unsigned)(h & 0xFFFF));
    return buf;
}

std::string utf8_truncate_bytes(const std::string &s, size_t max_bytes)
{
    if (s.size() <= max_bytes)
        return s;
    size_t n = max_bytes;
    while (n > 0 && ((uint8_t)s[n] & 0xC0) == 0x80)
        n--; // no cortar un carácter de varios bytes
    return s.substr(0, n);
}

// ------------------------------------------------------- texto clave=valor
std::string settings_to_text(const Settings &s)
{
    std::string t = "# T-Field UI \xC2\xB7 ajustes\n";
    auto put = [&](const char *k, long v) { t += std::string(k) + "=" + std::to_string(v) + "\n"; };
    auto puts = [&](const char *k, const std::string &v) { t += std::string(k) + "=" + v + "\n"; };
    put("brightness", s.brightness);
    put("screen_off_s", s.screen_off_s);
    put("kbd_light", s.kbd_light);
    put("theme", s.theme);
    put("lang", s.lang);
    put("gps", s.gps);
    put("pos_every_s", s.pos_every_s);
    put("pos_smart", s.pos_smart);
    put("track_every_s", s.track_every_s);
    put("volume", s.volume);
    put("snd_team", s.snd_team);
    put("snd_others", s.snd_others);
    put("snd_points", s.snd_points);
    put("team_first", s.team_first);
    put("map_team_only", s.map_team_only);
    put("pts_team", (int)s.pts_team);
    put("pts_others", (int)s.pts_others);
    put("share_all_team", s.share_all_team);
    put("sos_direct", s.sos_direct);
    put("sos_public", s.sos_public);
    put("sos_team_ch", s.sos_team_ch);
    put("sos_every_min", s.sos_every_min);
    std::string team;
    for (uint32_t n : s.team)
        team += (team.empty() ? "" : ",") + std::to_string(n);
    puts("team", team);
    put("channel_on", s.channel.on);
    puts("channel_name", s.channel.name);
    puts("channel_chat", s.channel.chat_id);
    puts("channel_key", base64_encode(s.channel.key.data(), s.channel.key.size()));
    for (auto &m : s.members)
        puts("member", std::to_string(m.num) + "," + std::to_string((int)m.state) + "," + std::to_string(m.epoch));
    puts("short_name", s.short_name);
    puts("long_name", s.long_name);
    puts("tz", time_zones()[s.tz < time_zones().size() ? s.tz : 0].code);
    puts("region", lora_regions()[s.region < lora_regions().size() ? s.region : 0].code);
    puts("preset", lora_presets()[s.preset < lora_presets().size() ? s.preset : 0].code);
    put("hops", s.hops);
    return t;
}

static uint8_t index_of(const std::vector<Option> &opts, const std::string &code)
{
    for (size_t i = 0; i < opts.size(); i++)
        if (code == opts[i].code)
            return (uint8_t)i;
    return 0;
}

// número entero dentro de [lo, hi]; si no lo es (texto, vacío, fuera de rango) se deja el valor por defecto
static bool num(const std::string &v, long lo, long hi, long &out)
{
    if (v.empty())
        return false;
    char *end = nullptr;
    long n = std::strtol(v.c_str(), &end, 10);
    if (end == v.c_str() || *end || n < lo || n > hi)
        return false;
    out = n;
    return true;
}

Settings settings_from_text(const std::string &text)
{
    Settings s;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        size_t eq = line.find('=');
        if (line.empty() || line[0] == '#' || eq == std::string::npos)
            continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        long n = 0;
        auto u8 = [&](uint8_t &f, long lo, long hi) {
            if (num(v, lo, hi, n))
                f = (uint8_t)n;
        };
        auto u16 = [&](uint16_t &f, long lo, long hi) {
            if (num(v, lo, hi, n))
                f = (uint16_t)n;
        };
        auto flag = [&](bool &f) {
            if (num(v, 0, 1, n))
                f = n != 0;
        };
        auto policy = [&](PointPolicy &f) {
            if (num(v, 0, 2, n))
                f = (PointPolicy)n;
        };
        if (k == "brightness") u8(s.brightness, 10, 100);
        else if (k == "screen_off_s") u16(s.screen_off_s, 0, 3600);
        else if (k == "kbd_light") flag(s.kbd_light);
        else if (k == "theme") u8(s.theme, 0, 2);
        else if (k == "lang") u8(s.lang, 0, 1);
        else if (k == "gps") flag(s.gps);
        else if (k == "pos_every_s") u16(s.pos_every_s, 30, 43200);
        else if (k == "pos_smart") flag(s.pos_smart);
        else if (k == "track_every_s") u8(s.track_every_s, 1, 255);
        else if (k == "volume") u8(s.volume, 0, 100);
        else if (k == "snd_team") flag(s.snd_team);
        else if (k == "snd_others") flag(s.snd_others);
        else if (k == "snd_points") flag(s.snd_points);
        else if (k == "team_first") flag(s.team_first);
        else if (k == "map_team_only") flag(s.map_team_only);
        else if (k == "pts_team") policy(s.pts_team);
        else if (k == "pts_others") policy(s.pts_others);
        else if (k == "share_all_team") flag(s.share_all_team);
        else if (k == "sos_direct") flag(s.sos_direct);
        else if (k == "sos_public") flag(s.sos_public);
        else if (k == "sos_team_ch") flag(s.sos_team_ch);
        else if (k == "sos_every_min") u8(s.sos_every_min, 1, 60);
        else if (k == "team") {
            s.team.clear();
            const char *p = v.c_str();
            while (*p) {
                char *e = nullptr;
                unsigned long t = std::strtoul(p, &e, 10);
                if (e == p)
                    break;
                if (t && std::find(s.team.begin(), s.team.end(), (uint32_t)t) == s.team.end())
                    s.team.push_back((uint32_t)t);
                p = *e == ',' ? e + 1 : e;
            }
        } else if (k == "channel_on") flag(s.channel.on);
        else if (k == "channel_name") {
            if (!v.empty())
                s.channel.name = utf8_truncate_bytes(v, 11);
        } else if (k == "channel_chat") s.channel.chat_id = v;
        else if (k == "channel_key") {
            std::vector<uint8_t> raw;
            if (base64_decode(v, raw) && raw.size() == 32)
                std::memcpy(s.channel.key.data(), raw.data(), 32);
        } else if (k == "member") {
            ChannelMember m;
            unsigned long num_ = 0, st = 0, ep = 0;
            char tail = 0;
            if (std::sscanf(v.c_str(), "%lu,%lu,%lu%c", &num_, &st, &ep, &tail) == 3 && num_ && st <= 2) {
                m.num = (uint32_t)num_;
                m.state = (ChannelMember::State)st;
                m.epoch = (uint32_t)ep;
                s.members.push_back(m);
            }
        } else if (k == "short_name") {
            if (!v.empty())
                s.short_name = utf8_truncate_bytes(v, 4);
        } else if (k == "long_name") {
            if (!v.empty())
                s.long_name = utf8_truncate_bytes(v, 39);
        } else if (k == "tz") s.tz = index_of(time_zones(), v);
        else if (k == "region") s.region = index_of(lora_regions(), v);
        else if (k == "preset") s.preset = index_of(lora_presets(), v);
        else if (k == "hops") u8(s.hops, 1, 7);
    }
    // un canal sin clave no sirve: queda apagado
    bool keyed = false;
    for (uint8_t b : s.channel.key)
        keyed |= b != 0;
    if (!keyed)
        s.channel.on = false;
    return s;
}

} // namespace tfield
