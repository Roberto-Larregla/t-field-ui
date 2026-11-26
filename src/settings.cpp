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
    // POSIX (Meshtastic: device.tzdef). Se guardan por código: el orden se puede cambiar.
    static const std::vector<Option> v = {
        {"<-04>4<-03>,M9.1.6/24,M4.1.6/24", "Chile continental", "UTC\xE2\x88\x92" "4, y \xE2\x88\x92" "3 en horario de verano"},
        {"<-03>3", "UTC\xE2\x88\x92" "3", "Magallanes, Argentina, Uruguay, Brasilia"},
        {"<-06>6<-05>,M9.1.6/22,M4.1.6/22", "Isla de Pascua", "UTC\xE2\x88\x92" "6, y \xE2\x88\x92" "5 en horario de verano"},
        {"UTC0", "UTC", "Hora universal, sin horario de verano"},
        {"<-04>4", "UTC\xE2\x88\x92" "4", "Bolivia, Venezuela"},
        {"<-05>5", "UTC\xE2\x88\x92" "5", "Colombia, Per\xC3\xBA, Ecuador"},
        {"CST6", "M\xC3\xA9xico (centro)", "Ciudad de M\xC3\xA9xico, sin horario de verano"},
        {"EST5EDT,M3.2.0,M11.1.0", "EE. UU. este", "Nueva York, Toronto"},
        {"CST6CDT,M3.2.0,M11.1.0", "EE. UU. centro", "Chicago, Winnipeg"},
        {"MST7MDT,M3.2.0,M11.1.0", "EE. UU. monta\xC3\xB1" "a", "Denver, Calgary"},
        {"MST7", "Arizona", "Sin horario de verano"},
        {"PST8PDT,M3.2.0,M11.1.0", "EE. UU. pac\xC3\xAD" "fico", "Los \xC3\x81ngeles, Vancouver"},
        {"GMT0BST,M3.5.0/1,M10.5.0", "Reino Unido", "Londres, Dubl\xC3\xADn"},
        {"WET0WEST,M3.5.0/1,M10.5.0", "Portugal", "Lisboa, Canarias"},
        {"CET-1CEST,M3.5.0,M10.5.0/3", "Europa central", "Madrid, Par\xC3\xADs, Berl\xC3\xADn, Roma"},
        {"EET-2EEST,M3.5.0/3,M10.5.0/4", "Europa del este", "Atenas, Helsinki, Bucarest"},
        {"IST-5:30", "India", "UTC+5:30"},
        {"JST-9", "Jap\xC3\xB3n", "UTC+9"},
        {"AEST-10AEDT,M10.1.0,M4.1.0/3", "Australia este", "S\xC3\xAD" "dney, Melbourne"},
        {"NZST-12NZDT,M9.5.0,M4.1.0/3", "Nueva Zelanda", "Auckland, Wellington"},
    };
    return v;
}

const std::vector<Option> &lora_regions()
{
    // Config.LoRaConfig.RegionCode, la que corresponde al país (meshtastic.org/docs/configuration/region-by-country)
    static const std::vector<Option> v = {
        {"ANZ", "ANZ", "915\xE2\x80\x93" "928 MHz \xC2\xB7 Australia, Nueva Zelanda, Chile, Argentina, Colombia"},
        {"US", "US", "902\xE2\x80\x93" "928 MHz \xC2\xB7 Estados Unidos y Canad\xC3\xA1"},
        {"EU_868", "EU_868", "869,4\xE2\x80\x93" "869,65 MHz \xC2\xB7 Europa"},
        {"EU_433", "EU_433", "433\xE2\x80\x93" "434 MHz \xC2\xB7 Europa"},
        {"BR_902", "BR_902", "902\xE2\x80\x93" "907,5 MHz \xC2\xB7 Brasil"},
        {"NZ_865", "NZ_865", "864\xE2\x80\x93" "868 MHz \xC2\xB7 Nueva Zelanda"},
        {"ANZ_433", "ANZ_433", "433\xE2\x80\x93" "434,8 MHz \xC2\xB7 Australia y Nueva Zelanda"},
        {"CN", "CN", "470\xE2\x80\x93" "510 MHz \xC2\xB7 China"},
        {"JP", "JP", "920,5\xE2\x80\x93" "923,5 MHz \xC2\xB7 Jap\xC3\xB3n"},
        {"KR", "KR", "920\xE2\x80\x93" "923 MHz \xC2\xB7 Corea del Sur"},
        {"TW", "TW", "920\xE2\x80\x93" "925 MHz \xC2\xB7 Taiw\xC3\xA1n"},
        {"RU", "RU", "868,7\xE2\x80\x93" "869,2 MHz \xC2\xB7 Rusia"},
        {"IN", "IN", "865\xE2\x80\x93" "867 MHz \xC2\xB7 India"},
        {"TH", "TH", "920\xE2\x80\x93" "925 MHz \xC2\xB7 Tailandia"},
        {"UA_433", "UA_433", "433\xE2\x80\x93" "434,7 MHz \xC2\xB7 Ucrania"},
        {"MY_433", "MY_433", "433\xE2\x80\x93" "435 MHz \xC2\xB7 Malasia"},
        {"MY_919", "MY_919", "919\xE2\x80\x93" "924 MHz \xC2\xB7 Malasia"},
        {"SG_923", "SG_923", "917\xE2\x80\x93" "925 MHz \xC2\xB7 Singapur"},
        {"PH_433", "PH_433", "433\xE2\x80\x93" "434,7 MHz \xC2\xB7 Filipinas"},
        {"PH_868", "PH_868", "868\xE2\x80\x93" "869,4 MHz \xC2\xB7 Filipinas"},
        {"PH_915", "PH_915", "915\xE2\x80\x93" "918 MHz \xC2\xB7 Filipinas"},
        {"KZ_433", "KZ_433", "433\xE2\x80\x93" "434,8 MHz \xC2\xB7 Kazajist\xC3\xA1n"},
        {"KZ_863", "KZ_863", "863\xE2\x80\x93" "868 MHz \xC2\xB7 Kazajist\xC3\xA1n"},
        {"NP_865", "NP_865", "865\xE2\x80\x93" "868 MHz \xC2\xB7 Nepal"},
        {"LORA_24", "LORA_24", "2,4 GHz \xC2\xB7 todo el mundo"},
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
    put("msg_keep_days", s.msg_keep_days);
    put("gps", s.gps);
    put("pos_every_s", s.pos_every_s);
    put("pos_smart", s.pos_smart);
    put("track_every_s", s.track_every_s);
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
    std::string kept;
    for (uint32_t n : s.kept_chats)
        kept += (kept.empty() ? "" : ",") + std::to_string(n);
    puts("chats", kept);
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
        else if (k == "msg_keep_days") u8(s.msg_keep_days, 0, 30);
        else if (k == "gps") flag(s.gps);
        else if (k == "pos_every_s") u16(s.pos_every_s, 30, 43200);
        else if (k == "pos_smart") flag(s.pos_smart);
        else if (k == "track_every_s") u8(s.track_every_s, 1, 255);
        else if (k == "team_first") flag(s.team_first);
        else if (k == "map_team_only") flag(s.map_team_only);
        else if (k == "pts_team") policy(s.pts_team);
        else if (k == "pts_others") policy(s.pts_others);
        else if (k == "share_all_team") flag(s.share_all_team);
        else if (k == "sos_direct") flag(s.sos_direct);
        else if (k == "sos_public") flag(s.sos_public);
        else if (k == "sos_team_ch") flag(s.sos_team_ch);
        else if (k == "sos_every_min") u8(s.sos_every_min, 1, 60);
        else if (k == "team" || k == "chats") {
            std::vector<uint32_t> &list = k == "team" ? s.team : s.kept_chats;
            list.clear();
            const char *p = v.c_str();
            while (*p) {
                char *e = nullptr;
                unsigned long t = std::strtoul(p, &e, 10);
                if (e == p)
                    break;
                if (t && std::find(list.begin(), list.end(), (uint32_t)t) == list.end())
                    list.push_back((uint32_t)t);
                p = *e == ',' ? e + 1 : e;
            }
            if (k == "chats" && list.size() > 32)
                list.erase(list.begin(), list.end() - 32);
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
