// Pruebas de funciones puras: formato, geodesia, sol, GPX, ajustes.
#include "tfield/format.h"
#include "tfield/gpx.h"
#include "tfield/settings.h"
#include "harness.h"

#include <cstdlib>

using namespace tfield;

TEST(formato_distancia)
{
    CHECK_EQ(format_distance(-1), std::string("\xE2\x80\x94"));
    CHECK_EQ(format_distance(0), std::string("0 m"));
    CHECK_EQ(format_distance(4), std::string("0 m"));
    CHECK_EQ(format_distance(5), std::string("10 m"));
    CHECK_EQ(format_distance(994), std::string("990 m"));
    CHECK_EQ(format_distance(995), std::string("1,0 km")); // no "1000 m"
    CHECK_EQ(format_distance(1000), std::string("1,0 km"));
    CHECK_EQ(format_distance(1049), std::string("1,0 km"));
    CHECK_EQ(format_distance(1050), std::string("1,1 km")); // redondea, no trunca
    CHECK_EQ(format_distance(1960), std::string("2,0 km"));
    CHECK_EQ(format_distance(9949), std::string("9,9 km"));
    CHECK_EQ(format_distance(9950), std::string("10 km")); // no "10,0 km"
    CHECK_EQ(format_distance(12345), std::string("12 km"));
    CHECK_EQ(format_distance(2000000000), std::string("2000000 km"));
}

TEST(formato_varios)
{
    CHECK_EQ(format_alt_diff(0), std::string("+0 m"));
    CHECK_EQ(format_alt_diff(38), std::string("+38 m"));
    CHECK_EQ(format_alt_diff(-95), std::string("\xE2\x88\x92" "95 m"));
    const char *exp[][2] = {{"0", "N"},    {"22", "N"},   {"23", "NE"}, {"45", "NE"}, {"90", "E"},  {"135", "SE"},
                            {"180", "S"},  {"225", "SO"}, {"270", "O"}, {"315", "NO"}, {"337", "NO"}, {"338", "N"},
                            {"359", "N"},  {"360", "N"},  {"-1", "N"},  {"-90", "O"},  {"725", "N"}};
    for (auto &e : exp)
        CHECK_EQ(std::string(cardinal(std::atoi(e[0]))), std::string(e[1]));
    CHECK_EQ(format_hops(-1), std::string(""));
    CHECK_EQ(format_hops(0), std::string("directo"));
    CHECK_EQ(format_hops(1), std::string("1 salto"));
    CHECK_EQ(format_hops(3), std::string("3 saltos"));
    CHECK_EQ(format_ago(0), std::string("ahora"));
    CHECK_EQ(format_ago(59), std::string("ahora"));
    CHECK_EQ(format_ago(60), std::string("1 min"));
    CHECK_EQ(format_ago(3599), std::string("59 min"));
    CHECK_EQ(format_ago(3600), std::string("1 h"));
    CHECK_EQ(format_ago(86399), std::string("23 h"));
    CHECK_EQ(format_ago(86400), std::string("1 d"));
    CHECK_EQ(format_ago(4000000000u), std::string("46296 d"));
    CHECK_EQ(format_daylight(-1), std::string("\xE2\x80\x94"));
    CHECK_EQ(format_daylight(0), std::string("0 min"));
    CHECK_EQ(format_daylight(45), std::string("45 min"));
    CHECK_EQ(format_daylight(369), std::string("6 h 09"));
}

TEST(utf8)
{
    std::string s = "\xC3\xB1" "and\xC3\xBA"; // "ñandú"
    CHECK_EQ(utf8_length(s), (size_t)5);
    CHECK_EQ(utf8_truncate(s, 3), std::string("\xC3\xB1" "an"));
    CHECK_EQ(utf8_truncate(s, 0), std::string(""));
    CHECK_EQ(utf8_truncate(s, 99), s);
    CHECK_EQ(utf8_truncate_bytes(s, 1), std::string(""));
    CHECK_EQ(utf8_truncate_bytes(s, 2), std::string("\xC3\xB1"));
    CHECK_EQ(utf8_truncate_bytes(s, 3), std::string("\xC3\xB1" "a"));
    CHECK_EQ(utf8_truncate_bytes(s, 6), std::string("\xC3\xB1" "and"));
    CHECK_EQ(utf8_truncate_bytes("\xF0\x9F\x98\x80" "x", 3), std::string("")); // emoji de 4 bytes
    CHECK_EQ(utf8_truncate_bytes("", 4), std::string(""));
}

TEST(geodesia_y_utm)
{
    // Referencias calculadas con pyproj (EPSG:327xx / 326xx)
    struct R {
        double lat, lon;
        int zone;
        double e, n;
    } refs[] = {
        {-33.64, -70.35, 19, 374802.46, 6276941.77},     {-33.4489, -70.6693, 19, 344846.72, 6297700.16},
        {-53.1638, -70.9171, 19, 371836.86, 4107791.47}, {-27.1127, -109.3497, 12, 663584.60, 7000008.23},
        {-18.4783, -70.3126, 19, 361414.41, 7956392.69}, {0.0, 0.0, 31, 166021.44, 0.00},
        {-33.0, -72.0, 19, 219700.01, 6344714.15},       {10.5, -66.9, 19, 729838.39, 1161462.93},
    };
    for (auto &r : refs) {
        Utm u = to_utm(r.lat, r.lon);
        CHECK_EQ(u.zone, r.zone);
        CHECK_NEAR(u.e, r.e, 1.0); // 1 m: más fino que el GPS
        CHECK_NEAR(u.n, r.n, 1.0);
    }
    CHECK_EQ(to_utm(-33.64, -70.35).band, 'H');
    CHECK_EQ(to_utm(-53.16, -70.92).band, 'F');
    CHECK_EQ(to_utm(10.5, -66.9).band, 'P');
    CHECK_EQ(format_utm(to_utm(-33.64, -70.35)), std::string("19H 374 802 E  6 276 942 N"));
    // Santiago – Valparaíso ~ 98 km, rumbo ~ ONO
    double d = haversine_m(-33.4489, -70.6693, -33.0472, -71.6127);
    CHECK_NEAR(d, 97800, 1500);
    double b = bearing_deg(-33.4489, -70.6693, -33.0472, -71.6127);
    CHECK(b > 270 && b < 300);
    CHECK_NEAR(haversine_m(-33.64, -70.35, -33.64, -70.35), 0, 1e-6);
    CHECK_NEAR(bearing_deg(0, 0, 1, 0), 0, 1e-9);
    CHECK_NEAR(bearing_deg(0, 0, 0, 1), 90, 1e-9);
    // Mercator: ida y vuelta, y sin valores infinitos en los polos
    double x, y, la, lo;
    mercator_px(-33.64, -70.35, 15, x, y);
    mercator_latlon(x, y, 15, la, lo);
    CHECK_NEAR(la, -33.64, 1e-9);
    CHECK_NEAR(lo, -70.35, 1e-9);
    mercator_px(-90, 0, 10, x, y);
    CHECK(std::isfinite(y));
    mercator_px(90, 0, 10, x, y);
    CHECK(std::isfinite(y));
}

TEST(sol)
{
    // Referencias de la librería astral (minutos UTC); tolerancia 3 min
    struct R {
        double lat, lon;
        int y, m, d;
        double rise, set;
    } refs[] = {
        {-33.64, -70.35, 2026, 11, 20, 567.6, 1406.9}, {-33.64, -70.35, 2026, 6, 21, 706.0, 1300.6},
        {-53.1638, -70.9171, 2026, 11, 20, 508.5, 29.4}, {-53.1638, -70.9171, 2026, 6, 21, 780.0, 1231.1},
        {-18.4783, -70.3126, 2026, 11, 20, 595.7, 1378.2},
    };
    for (auto &r : refs) {
        double rise, set;
        CHECK(sun_times_utc(r.y, r.m, r.d, r.lat, r.lon, rise, set));
        auto wrap = [](double v) { return std::fmod(std::fmod(v, 1440.0) + 1440.0, 1440.0); };
        CHECK_NEAR(wrap(rise), r.rise, 3);
        CHECK_NEAR(wrap(set), r.set, 3);
    }
    double rise, set;
    CHECK(!sun_times_utc(2026, 6, 21, 80.0, 0.0, rise, set)); // sol de medianoche: sin puesta
}

static int hhmm(const std::string &s) { return s.size() == 5 ? std::atoi(s.c_str()) * 60 + std::atoi(s.c_str() + 3) : -9999; }

TEST(luz_del_dia)
{
    // Lo que muestra el inicio: minutos de luz y hora de puesta en hora local
    DaylightInfo d = daylight_info(epoch_from_utc(2026, 11, 20, 17, 32, 0), -3 * 60, -33.64, -70.35);
    CHECK(d.valid);
    CHECK_NEAR(hhmm(d.sunset), 20 * 60 + 27, 2);
    CHECK_NEAR(d.minutes_left, 20 * 60 + 27 - (14 * 60 + 32), 2);
    // Punta Arenas: la puesta cae después de medianoche UTC
    d = daylight_info(epoch_from_utc(2026, 11, 20, 20, 0, 0), -3 * 60, -53.1638, -70.9171);
    CHECK(d.valid);
    CHECK_NEAR(hhmm(d.sunset), 21 * 60 + 29, 3);
    CHECK_NEAR(d.minutes_left, 4 * 60 + 29, 3);
    // de noche: 0 min de luz
    d = daylight_info(epoch_from_utc(2026, 11, 21, 3, 0, 0), -3 * 60, -33.64, -70.35);
    CHECK(d.valid);
    CHECK_EQ(d.minutes_left, 0);
}

TEST(gpx_y_fechas)
{
    CHECK_EQ(iso8601(0), std::string("1970-01-01T00:00:00Z"));
    CHECK_EQ(iso8601(epoch_from_utc(2026, 11, 20, 17, 32, 5)), std::string("2026-11-20T17:32:05Z"));
    CHECK_EQ(iso8601(epoch_from_utc(2024, 2, 29, 23, 59, 59)), std::string("2024-02-29T23:59:59Z"));
    CHECK_EQ(iso8601(epoch_from_utc(2100, 3, 1, 0, 0, 0)), std::string("2100-03-01T00:00:00Z"));
    CHECK_EQ(xml_escape("<a&b>\"c'"), std::string("&lt;a&amp;b&gt;&quot;c&apos;"));
    CHECK_EQ(safe_filename("R1 Subida mirador"), std::string("R1_Subida_mirador"));
    CHECK_EQ(safe_filename("Quebrada \xC3\x91uble / d\xC3\xAD" "a 2"), std::string("Quebrada_Nuble_dia_2"));
    CHECK_EQ(safe_filename(""), std::string("ruta"));
    CHECK_EQ(safe_filename("///"), std::string("ruta"));
    CHECK_EQ(safe_filename("\xC2\xB0\xC2\xB7"), std::string("ruta"));
    std::string longname = safe_filename(std::string(39, 'a') + " bb");
    CHECK(longname.size() <= 40);
    CHECK(longname.back() != '_');
    Track t;
    t.pts.push_back({-33.64, -70.35, 1500.5f, epoch_from_utc(2026, 11, 20, 17, 0, 0)});
    t.pts.push_back({-33.63, -70.34, 1510.0f, 0});
    std::string g = gpx_track("R1 <prueba>", "nota & más", t);
    CHECK(g.find("<name>R1 &lt;prueba&gt;</name>") != std::string::npos);
    CHECK(g.find("<desc>nota &amp; más</desc>") != std::string::npos);
    CHECK(g.find("<trkpt lat=\"-33.6400000\" lon=\"-70.3500000\"><ele>1500.5</ele><time>2026-11-20T17:00:00Z</time>") !=
          std::string::npos);
    CHECK(g.find("</gpx>") != std::string::npos);
    // caracteres de control fuera del XML (lo invalidarían)
    CHECK_EQ(xml_escape(std::string("a\x01" "b")), std::string("ab"));
}

TEST(ajustes_texto)
{
    Settings s;
    s.brightness = 40;
    s.screen_off_s = 0;
    s.kbd_light = false;
    s.pos_every_s = 120;
    s.volume = 0;
    s.snd_others = true;
    s.pts_others = PointPolicy::Ignore;
    s.sos_direct = false;
    s.sos_every_min = 5;
    s.channel.on = true;
    s.channel.name = "Cumbre \xC3\xB1";
    s.channel.chat_id = "canal:Cumbre";
    for (size_t i = 0; i < 32; i++)
        s.channel.key[i] = (uint8_t)(255 - i * 7);
    s.members = {{2, ChannelMember::Joined, 1795000000}, {3, ChannelMember::Invited, 1795000100}};
    s.team = {2, 3, 4};
    s.short_name = "\xC3\x91" "U";
    s.long_name = "Geolog\xC3\xAD" "a = 1";
    s.tz = 2;
    s.region = 1;
    s.preset = 3;
    s.hops = 5;
    Settings r = settings_from_text(settings_to_text(s));
    CHECK_EQ((int)r.brightness, 40);
    CHECK_EQ((int)r.screen_off_s, 0);
    CHECK_EQ(r.kbd_light, false);
    CHECK_EQ((int)r.pos_every_s, 120);
    CHECK_EQ((int)r.volume, 0);
    CHECK_EQ(r.snd_others, true);
    CHECK_EQ((int)r.pts_others, (int)PointPolicy::Ignore);
    CHECK_EQ(r.sos_direct, false);
    CHECK_EQ((int)r.sos_every_min, 5);
    CHECK_EQ(r.channel.on, true);
    CHECK_EQ(r.channel.name, s.channel.name);
    CHECK_EQ(r.channel.chat_id, s.channel.chat_id);
    CHECK(r.channel.key == s.channel.key);
    CHECK_EQ(r.members.size(), (size_t)2);
    CHECK_EQ((int)r.members[1].state, (int)ChannelMember::Invited);
    CHECK_EQ(r.members[1].epoch, (uint32_t)1795000100);
    CHECK_EQ(r.team.size(), (size_t)3);
    CHECK_EQ(r.short_name, s.short_name);
    CHECK_EQ(r.long_name, s.long_name); // con "=" dentro del valor
    CHECK_EQ((int)r.tz, 2);
    CHECK_EQ((int)r.region, 1);
    CHECK_EQ((int)r.preset, 3);
    CHECK_EQ((int)r.hops, 5);
    // basura: no se cae y quedan los valores por defecto
    Settings d;
    Settings g = settings_from_text("brightness=abc\n=\nsin_igual\nhops=99\nvolume=-5\nmember=x,y\nchannel_key=@@@\n\r\n");
    CHECK_EQ((int)g.hops, (int)d.hops); // 99 está fuera de rango: queda el valor por defecto
    CHECK_EQ((int)g.volume, (int)d.volume);
    CHECK_EQ((int)g.brightness, (int)d.brightness);
    CHECK(g.members.empty());
    Settings w = settings_from_text("brightness=70\r\nshort_name=GEO9\r\nchannel_on=1\r\n");
    CHECK_EQ(w.short_name, std::string("GEO9"));
    CHECK(!w.channel.on); // canal sin clave: apagado
}

TEST(base64_y_huella)
{
    for (size_t n = 0; n < 40; n++) {
        std::vector<uint8_t> in(n), out;
        for (size_t i = 0; i < n; i++)
            in[i] = (uint8_t)(i * 37 + n);
        CHECK(base64_decode(base64_encode(in.data(), in.size()), out));
        CHECK(out == in);
    }
    std::vector<uint8_t> out;
    CHECK(!base64_decode("ab$c", out));
    ChannelKey a{}, b{};
    b[31] = 1;
    CHECK(key_fingerprint(a) != key_fingerprint(b));
    CHECK_EQ(key_fingerprint(a), key_fingerprint(a));
    CHECK_EQ(key_fingerprint(a).size(), (size_t)9);
}
