#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_console.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "settings.h"
#include "audio.h"
#include "wm8960.h"
#include "net.h"
#include "rtp.h"
#include "nettest.h"

static void save(void)
{
    if (settings_save() == ESP_OK) printf("gespeichert\n");
    else printf("Speichern fehlgeschlagen\n");
}

void console_print_status(void)
{
    net_info_t ni;
    audio_stats_t as;
    rtp_stats_t rs;
    net_get_info(&ni);
    audio_get_stats(&as);
    rtp_get_stats(&rs);
    printf("\n=== NR Bodypack (ESP32-C5) ===\n");
    printf("WLAN : %s", ni.connected ? "verbunden" : "nicht verbunden");
    if (ni.connected) {
        printf(" '%s' IP %s, %s Kanal %d, RSSI %d dBm, AP %02x:%02x:%02x:%02x:%02x:%02x",
               ni.ssid, ni.ip, ni.band_5g ? "5 GHz" : "2,4 GHz", ni.channel, ni.rssi,
               ni.bssid[0], ni.bssid[1], ni.bssid[2], ni.bssid[3], ni.bssid[4], ni.bssid[5]);
    }
    printf("\n       Abbrueche %lu, AP-Wechsel %lu, Antenne %s\n", (unsigned long)ni.disconnects,
           (unsigned long)ni.roams, g_set.ant_ext ? "extern" : "intern");
    printf("Audio: Modus %s, Mic %s (PGA %+d dB, Boost %d), Kopfhoerer %d%%, Mithoerton %d%%, Talk %s\n",
           mode_name(g_set.mode), g_set.mic == MIC_ONBOARD ? "Board" : "Buchse", g_set.pga_db,
           g_set.boost, g_set.hp_vol, g_set.sidetone, g_talk ? "AN" : "aus");
    printf("       Pegel Mic %.1f dBFS, Ausgang %.1f dBFS, Bloecke %lu, I2S-Fehler %lu, max. %lu us/Block\n",
           as.mic_peak_db, as.out_peak_db, (unsigned long)as.blocks, (unsigned long)as.i2s_errors,
           (unsigned long)as.proc_us_max);
    printf("RTP  : Ziel %s:%u, lokal %u, Paket %d ms, Jitterbuffer Ziel %d ms (aktuell %lu ms)\n",
           g_set.host[0] ? g_set.host : "-", g_set.port, g_set.local_port, g_set.frame_ms, g_set.jb_ms,
           (unsigned long)rs.jb_depth_ms);
    printf("       TX %lu, RX %lu, verloren %lu, zu spaet %lu, verworfen %lu, Resync %lu, Jitter %.2f ms\n",
           (unsigned long)rs.tx_packets, (unsigned long)rs.rx_packets, (unsigned long)rs.rx_lost,
           (unsigned long)rs.rx_late, (unsigned long)rs.rx_dropped, (unsigned long)rs.rx_resyncs, rs.jitter_ms);
    printf("       Unterlauf %lu, Sendepuffer voll %lu\n", (unsigned long)as.play_underruns,
           (unsigned long)as.tx_overflows);
    printf("RAM  : frei %u Byte, minimal %u Byte\n\n", (unsigned)esp_get_free_heap_size(),
           (unsigned)esp_get_minimum_free_heap_size());
}

static int cmd_status(int argc, char **argv)
{
    console_print_status();
    return 0;
}

static int cmd_wifi(int argc, char **argv)
{
    if (argc < 2) {
        printf("wifi <ssid> [passwort]\n");
        return 1;
    }
    strlcpy(g_set.ssid, argv[1], sizeof(g_set.ssid));
    strlcpy(g_set.pass, argc > 2 ? argv[2] : "", sizeof(g_set.pass));
    save();
    net_connect();
    return 0;
}

static int cmd_band(int argc, char **argv)
{
    if (argc < 2) { printf("band auto|5g|2g\n"); return 1; }
    if (!strcmp(argv[1], "5g")) g_set.band = BAND_5G;
    else if (!strcmp(argv[1], "2g")) g_set.band = BAND_2G;
    else g_set.band = BAND_AUTO;
    save();
    net_connect();
    return 0;
}

static int cmd_ant(int argc, char **argv)
{
    if (argc < 2) { printf("ant ext|int\n"); return 1; }
    g_set.ant_ext = !strcmp(argv[1], "ext");
    net_set_antenna(g_set.ant_ext);
    save();
    return 0;
}

static int cmd_scan(int argc, char **argv)
{
    net_scan();
    return 0;
}

static int cmd_mode(int argc, char **argv)
{
    if (argc < 2) { printf("mode off|loop|tone|rtp\n"); return 1; }
    for (int m = 0; m < MODE_COUNT; m++) {
        if (!strcmp(argv[1], mode_name(m))) {
            g_set.mode = m;
            audio_set_mode(m);
            save();
            return 0;
        }
    }
    printf("unbekannter Modus\n");
    return 1;
}

static int cmd_mic(int argc, char **argv)
{
    if (argc < 2) {
        printf("mic board|jack [pga_db -17..30] [boost 0..3]\n");
        printf("  boost: 0 = 0 dB, 1 = +13 dB, 2 = +20 dB, 3 = +29 dB\n");
        return 1;
    }
    g_set.mic = !strcmp(argv[1], "board") ? MIC_ONBOARD : MIC_JACK;
    if (argc > 2) g_set.pga_db = (int8_t)atoi(argv[2]);
    if (argc > 3) g_set.boost = (uint8_t)atoi(argv[3]);
    wm8960_set_mic(g_set.mic, g_set.pga_db, g_set.boost);
    save();
    return 0;
}

static int cmd_vol(int argc, char **argv)
{
    if (argc < 2) { printf("vol <0..100>\n"); return 1; }
    int v = atoi(argv[1]);
    g_set.hp_vol = v < 0 ? 0 : v > 100 ? 100 : v;
    wm8960_set_hp_volume(g_set.hp_vol);
    save();
    return 0;
}

static int cmd_side(int argc, char **argv)
{
    if (argc < 2) { printf("side <0..100>  (Mithoerton im RTP-Modus)\n"); return 1; }
    int v = atoi(argv[1]);
    g_set.sidetone = v < 0 ? 0 : v > 100 ? 100 : v;
    save();
    return 0;
}

static int cmd_rtp(int argc, char **argv)
{
    if (argc < 2) {
        printf("rtp <ziel-ip> [zielport] | rtp frame <5|10> | rtp jb <ms> | rtp latch <0|1> | rtp reset\n");
        return 1;
    }
    if (!strcmp(argv[1], "frame") && argc > 2) {
        int f = atoi(argv[2]);
        g_set.frame_ms = (f == 5) ? 5 : 10;
    } else if (!strcmp(argv[1], "jb") && argc > 2) {
        int j = atoi(argv[2]);
        g_set.jb_ms = j < 5 ? 5 : j > 300 ? 300 : j;
    } else if (!strcmp(argv[1], "latch") && argc > 2) {
        g_set.ptt_latch = atoi(argv[2]) ? 1 : 0;
    } else if (!strcmp(argv[1], "reset")) {
        rtp_reset_stats();
        return 0;
    } else {
        strlcpy(g_set.host, argv[1], sizeof(g_set.host));
        if (argc > 2) g_set.port = (uint16_t)atoi(argv[2]);
        rtp_update_target();
    }
    save();
    return 0;
}

static int cmd_talk(int argc, char **argv)
{
    if (argc < 2) g_talk = !g_talk;
    else g_talk = !strcmp(argv[1], "on");
    printf("Talk %s\n", g_talk ? "AN" : "aus");
    return 0;
}

static int cmd_nettest(int argc, char **argv)
{
    if (argc < 2) {
        printf("nettest <pc-ip> [port 5007] [sekunden 10] [intervall_ms 20] [bytes 172]\n");
        printf("nettest stop\n");
        return 1;
    }
    if (!strcmp(argv[1], "stop")) { nettest_stop(); return 0; }
    int port = argc > 2 ? atoi(argv[2]) : 5007;
    int sec = argc > 3 ? atoi(argv[3]) : 10;
    int iv = argc > 4 ? atoi(argv[4]) : 20;
    int sz = argc > 5 ? atoi(argv[5]) : 172;
    esp_err_t err = nettest_start(argv[1], (uint16_t)port, sec, iv, sz);
    if (err != ESP_OK) printf("Start fehlgeschlagen: %s\n", esp_err_to_name(err));
    return 0;
}

static int cmd_reg(int argc, char **argv)
{
    if (argc < 3) { printf("reg <nr hex> <wert hex>  (WM8960 direkt schreiben)\n"); return 1; }
    uint8_t r = (uint8_t)strtol(argv[1], NULL, 16);
    uint16_t v = (uint16_t)strtol(argv[2], NULL, 16);
    return wm8960_write(r, v) == ESP_OK ? 0 : 1;
}

static int cmd_factory(int argc, char **argv)
{
    settings_reset();
    save();
    printf("Werkseinstellungen gespeichert, Neustart ...\n");
    esp_restart();
    return 0;
}

static int cmd_restart(int argc, char **argv)
{
    esp_restart();
    return 0;
}

static int cmd_log(int argc, char **argv)
{
    if (argc < 2) { printf("log on|off\n"); return 1; }
    esp_log_level_set("*", !strcmp(argv[1], "off") ? ESP_LOG_WARN : ESP_LOG_INFO);
    return 0;
}

void console_cmds_register(void)
{
    const esp_console_cmd_t cmds[] = {
        { .command = "status",  .help = "Status: WLAN, Audio, RTP, Speicher", .func = cmd_status },
        { .command = "wifi",    .help = "WLAN setzen: wifi <ssid> [passwort]", .func = cmd_wifi },
        { .command = "band",    .help = "WLAN-Band: band auto|5g|2g", .func = cmd_band },
        { .command = "ant",     .help = "Antenne: ant ext|int", .func = cmd_ant },
        { .command = "scan",    .help = "WLAN-Netze in der Umgebung anzeigen", .func = cmd_scan },
        { .command = "mode",    .help = "Audio-Modus: mode off|loop|tone|rtp", .func = cmd_mode },
        { .command = "mic",     .help = "Mikrofon: mic board|jack [pga_db] [boost 0..3]", .func = cmd_mic },
        { .command = "vol",     .help = "Kopfhoerer-Lautstaerke: vol 0..100", .func = cmd_vol },
        { .command = "side",    .help = "Mithoerton im RTP-Modus: side 0..100", .func = cmd_side },
        { .command = "rtp",     .help = "RTP: rtp <ip> [port] | frame | jb | latch | reset", .func = cmd_rtp },
        { .command = "talk",    .help = "Talk: talk [on|off]", .func = cmd_talk },
        { .command = "nettest", .help = "Funk-Messung: nettest <pc-ip> [port] [s] [ms] [bytes]", .func = cmd_nettest },
        { .command = "reg",     .help = "WM8960-Register schreiben: reg <nr> <wert> (hex)", .func = cmd_reg },
        { .command = "log",     .help = "Log-Ausgaben: log on|off", .func = cmd_log },
        { .command = "factory", .help = "Werkseinstellungen", .func = cmd_factory },
        { .command = "restart", .help = "Neustart", .func = cmd_restart },
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
}
