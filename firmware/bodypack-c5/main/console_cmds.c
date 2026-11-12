#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_console.h"
#include "soc/lp_aon_reg.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "settings.h"
#include "audio.h"
#include "wm8960.h"
#include "net.h"
#include "rtp.h"
#include "nettest.h"
#include "com.h"
#include "ota.h"
#include "esp_ota_ops.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

extern i2c_master_bus_handle_t g_i2c_bus;

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
    const esp_app_desc_t *ad = esp_app_get_description();
    const esp_partition_t *run = esp_ota_get_running_partition();
    printf("\n=== NR Bodypack (ESP32-C5) === Firmware %s, gebaut %s %s, Laufzeit %llu s, Bereich %s, S3 %s\n",
           ad->version, ad->date, ad->time, (unsigned long long)(esp_timer_get_time() / 1000000),
           run ? run->label : "?", ota_s3_version()[0] ? ota_s3_version() : "?");
    printf("WLAN : %s", ni.connected ? "verbunden" : "nicht verbunden");
    if (ni.connected) {
        printf(" '%s' IP %s, %s Kanal %d, RSSI %d dBm, AP %02x:%02x:%02x:%02x:%02x:%02x",
               ni.ssid, ni.ip, ni.band_5g ? "5 GHz" : "2,4 GHz", ni.channel, ni.rssi,
               ni.bssid[0], ni.bssid[1], ni.bssid[2], ni.bssid[3], ni.bssid[4], ni.bssid[5]);
    }
    printf("\n       Abbrueche %lu, AP-Wechsel %lu, Antenne %s\n", (unsigned long)ni.disconnects,
           (unsigned long)ni.roams, g_set.ant_ext ? "extern" : "intern");
    printf("Codec: WM8960 %s (I2C SDA=GP%d, SCL=GP%d, %lu kHz), Schreibfehler %lu, Wiederholungen %lu\n",
           wm8960_present() ? "gefunden + eingestellt" : "NICHT gefunden",
           CONFIG_NRB_PIN_I2C_SDA, CONFIG_NRB_PIN_I2C_SCL, (unsigned long)(wm8960_get_i2c_hz() / 1000),
           (unsigned long)wm8960_write_errors(), (unsigned long)wm8960_i2c_retries());
    printf("Audio: Modus %s, Mic %s (PGA %+d dB, Boost %d), Kopfhoerer %d%% (+%d dB Empfang), Mithoerton %d%%, Talk %s\n",
           mode_name(g_set.mode), g_set.mic == MIC_ONBOARD ? "links (LINPUT1)" : g_set.mic == MIC_JACK ? "rechts (RINPUT1)" : "beide (Test)", g_set.pga_db,
           g_set.boost, g_set.hp_vol, g_set.rx_gain_db, g_set.sidetone, g_talk ? "AN" : "aus");
    printf("       Pegel Mic %.1f dBFS, Ausgang %.1f dBFS, Bloecke %lu, I2S-Fehler %lu\n",
           as.mic_peak_db, as.out_peak_db, (unsigned long)as.blocks, (unsigned long)as.i2s_errors);
    printf("       Effektivpegel rechts (RINPUT1) %.1f dBFS, links (LINPUT1) %.1f dBFS, Gate %s (%.0f dB)\n",
           as.rms_jack_db, as.rms_board_db,
           g_set.gate_db < 0 ? "an" : "aus", as.gate_gain_db);
    printf("       Uebersteuert %lu ms, I2S-Aussetzer Eingang %lu / Ausgang %lu\n", (unsigned long)as.clips,
           (unsigned long)as.rx_overflows, (unsigned long)as.tx_underruns);
    char tgt[24];
    rtp_target_str(tgt, sizeof(tgt));
    printf("RTP  : Ziel %s, lokal %u, Codec %s, Paket %d ms, Jitterbuffer Ziel %d ms (aktuell %lu ms)\n",
           tgt, g_set.local_port,
           g_set.codec == CODEC_OPUS ? "Opus" : "L16", g_set.frame_ms, g_set.jb_ms,
           (unsigned long)rs.jb_depth_ms);
    if (g_set.codec == CODEC_OPUS) {
        printf("       Opus %d kbit/s, Komplexitaet %d, %s\n", g_set.opus_kbps, g_set.opus_cplx,
               g_set.opus_voip ? "voip+FEC" : "lowdelay");
    }
    printf("       TX %lu (%lu Byte), RX %lu, verloren %lu (FEC %lu), zu spaet %lu, verworfen %lu, Resync %lu, fehlerhaft %lu\n",
           (unsigned long)rs.tx_packets, (unsigned long)rs.tx_bytes, (unsigned long)rs.rx_packets,
           (unsigned long)rs.rx_lost, (unsigned long)rs.rx_fec, (unsigned long)rs.rx_late,
           (unsigned long)rs.rx_dropped, (unsigned long)rs.rx_resyncs, (unsigned long)rs.rx_bad);
    printf("       Empfang: Pegel %.1f dBFS, Paketgroesse im Mittel %lu Byte, davon Stille-Pakete %lu (ohne Decoder)\n",
           rs.rx_peak_db, (unsigned long)(rs.rx_packets ? rs.rx_payload_bytes / rs.rx_packets : 0),
           (unsigned long)rs.rx_silence);
    printf("       Strom SSRC %08lx, Stromwechsel %lu, fremde Pakete %lu (zweiter Strom!), RTCP %lu\n",
           (unsigned long)rs.rx_ssrc, (unsigned long)rs.rx_ssrc_changes, (unsigned long)rs.rx_foreign,
           (unsigned long)rs.rx_rtcp);
    printf("       Jitter %.2f ms, Unterlauf %lu, Sendepuffer voll %lu\n", rs.jitter_ms,
           (unsigned long)as.play_underruns, (unsigned long)as.tx_overflows);
    extern volatile int g_cpu_load;
    printf("CPU  : Last %d %%, Opus-Encoder %lu us (max %lu), Decoder %lu us (max %lu) pro Paket, Audio max %lu us/ms\n",
           g_cpu_load, (unsigned long)rs.enc_us_avg, (unsigned long)rs.enc_us_max, (unsigned long)rs.dec_us_avg,
           (unsigned long)rs.dec_us_max, (unsigned long)as.proc_us_max);
    printf("Stack: Codec-Task frei %lu Byte (von 32 KB)\n", (unsigned long)rs.codec_stack_free);
    printf("RAM  : frei %u Byte, minimal %u Byte\n", (unsigned)esp_get_free_heap_size(),
           (unsigned)esp_get_minimum_free_heap_size());
    com_status_t cs;
    com_get_status(&cs);
    printf("Com  : %s%s%s | Server %s:%u (%s), PIN %s, Benutzer '%s', %d Kanaele\n", com_state_name(cs.state),
           cs.msg[0] ? " - " : "", cs.msg, g_set.com_host, g_set.com_port, cs.server_ip[0] ? cs.server_ip : "-",
           g_set.com_pin[0] ? "gesetzt" : "FEHLT", cs.user, cs.nch);
    com_channel_t ch[COM_MAX_CH];
    int n = com_get_channels(ch, COM_MAX_CH);
    for (int i = 0; i < n; i++) {
        if (!ch[i].joined && !ch[i].talkers) continue;   // nur Kanaele mit etwas Interessantem
        printf("       %2d %-24s %s %s Lautst. %3d%%%s\n", i, ch[i].name, ch[i].joined ? "drin" : "    ",
               ch[i].talk ? "TALK" : "    ", ch[i].volume, ch[i].talkers ? "  (jemand spricht)" : "");
    }
    printf("\n");
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
        printf("mic board|jack|both [pga_db -17..30] [boost 0..3]\n");
        printf("  board = LINPUT1 (Waveshare: Mikrofon 'Left'), jack = RINPUT1 (Waveshare: Mikrofon 'Right',\n");
        printf("          eigene Platine: Headset), both = beide, im Loop links = LINPUT1, rechts = RINPUT1\n");
        printf("  Hinweis: Die Buchse des Waveshare-Boards hat KEINEN Mikrofon-Anschluss.\n");
        printf("  boost: 0 = 0 dB, 1 = +13 dB, 2 = +20 dB, 3 = +29 dB\n");
        return 1;
    }
    g_set.mic = !strcmp(argv[1], "board") ? MIC_ONBOARD : !strcmp(argv[1], "both") ? MIC_STEREO : MIC_JACK;
    if (argc > 2) g_set.pga_db = (int8_t)atoi(argv[2]);
    if (argc > 3) g_set.boost = (uint8_t)atoi(argv[3]);
    wm8960_set_mic(g_set.mic, g_set.pga_db, g_set.boost);
    save();
    return 0;
}

static int cmd_gate(int argc, char **argv)
{
    if (argc < 2) {
        printf("gate <schwelle dBFS -80..-20> | gate off   (aktuell: %s%d)\n",
               g_set.gate_db < 0 ? "" : "aus ", g_set.gate_db);
        printf("  Unter der Schwelle wird das Mikrofon um 30 dB leiser (Sprechpausen ohne Rauschen).\n");
        printf("  Schwelle finden: 'pegel' still messen, Gate ca. 6-10 dB darueber setzen.\n");
        return 1;
    }
    if (!strcmp(argv[1], "off")) {
        g_set.gate_db = 0;
    } else {
        int v = atoi(argv[1]);
        g_set.gate_db = (int8_t)(v < -80 ? -80 : v > -20 ? -20 : v);
    }
    save();
    return 0;
}

static int cmd_pegel(int argc, char **argv)
{
    int secs = argc > 1 ? atoi(argv[1]) : 5;
    if (secs < 1) secs = 1;
    if (secs > 30) secs = 30;
    printf("Pegelanzeige %d s (Effektivwert, ~0,5 s gemittelt). Erst still sein, dann sprechen.\n", secs);
    printf("   rechts    links   Spitze   Gate   Uebersteuert  Aussetzer\n");
    for (int i = 0; i < secs * 2; i++) {
        vTaskDelay(pdMS_TO_TICKS(500));
        audio_stats_t as;
        audio_get_stats(&as);
        printf("  %6.1f   %6.1f   %6.1f   %4.0f dB  %6lu ms   %lu/%lu\n", as.rms_jack_db, as.rms_board_db,
               as.mic_peak_db, as.gate_gain_db, (unsigned long)as.clips, (unsigned long)as.rx_overflows,
               (unsigned long)as.tx_underruns);
    }
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

static int cmd_rxgain(int argc, char **argv)
{
    if (argc < 2) { printf("rxgain <0..24>  Hoerverstaerkung fuer den Intercom-Empfang in dB (aktuell %d)\n", g_set.rx_gain_db); return 1; }
    int v = atoi(argv[1]);
    g_set.rx_gain_db = v < 0 ? 0 : v > 24 ? 24 : v;
    audio_set_rx_gain(g_set.rx_gain_db);
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
        printf("rtp <ziel-ip> [zielport]   Ziel setzen (127.0.0.1 %u = lokaler Codec-Test)\n", g_set.local_port);
        printf("rtp codec opus|l16         Codec\n");
        printf("rtp frame 5|10|20          Paketlaenge in ms (Opus; L16 immer 5)\n");
        printf("rtp jb <ms>                Ziel-Jitterbuffer\n");
        printf("rtp latch 0|1              Taste 1: halten (0) oder umschalten (1)\n");
        printf("rtp reset                  Statistik zuruecksetzen\n");
        return 1;
    }
    if (!strcmp(argv[1], "codec") && argc > 2) {
        g_set.codec = !strcmp(argv[2], "l16") ? CODEC_L16 : CODEC_OPUS;
        rtp_codec_changed();
    } else if (!strcmp(argv[1], "frame") && argc > 2) {
        int f = atoi(argv[2]);
        g_set.frame_ms = (f == 5 || f == 20) ? f : 10;
        rtp_codec_changed();
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

static int cmd_opus(int argc, char **argv)
{
    if (argc < 2) {
        printf("opus <kbit/s 8..128> [komplexitaet 0..10] [lowdelay|voip]\n");
        printf("  lowdelay: nur CELT, geringste Latenz | voip: SILK/Hybrid mit FEC (robuster bei Verlust)\n");
        return 1;
    }
    int k = atoi(argv[1]);
    g_set.opus_kbps = k < 8 ? 8 : k > 128 ? 128 : k;
    if (argc > 2) {
        int c = atoi(argv[2]);
        g_set.opus_cplx = c < 0 ? 0 : c > 10 ? 10 : c;
    }
    if (argc > 3) g_set.opus_voip = !strcmp(argv[3], "voip");
    rtp_codec_changed();
    rtp_reset_stats();
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

static int cmd_i2cscan(int argc, char **argv)
{
    if (!g_i2c_bus) { printf("I2C-Bus nicht aktiv\n"); return 1; }
    printf("I2C-Scan (SDA=GP%d, SCL=GP%d):\n", CONFIG_NRB_PIN_I2C_SDA, CONFIG_NRB_PIN_I2C_SCL);
    int found = 0;
    for (int a = 0x08; a < 0x78; a++) {
        if (i2c_master_probe(g_i2c_bus, a, 20) == ESP_OK) {
            printf("  0x%02X%s\n", a, a == 0x1A ? "  <- WM8960" : "");
            found++;
        }
    }
    if (!found) {
        printf("  keine Geraete gefunden: SDA/SCL vertauscht oder nicht verbunden, 3V3/GND am WM8960 pruefen\n");
    } else if (!wm8960_present()) {
        printf("  WM8960 antwortet jetzt -> 'codec' eingeben (stellt ihn ohne Neustart ein).\n");
    }
    if (!found) {
        printf("  Tipp: 'i2cdiag' prueft Spannung und vertauschte Leitungen.\n");
    }
    return 0;
}

// WM8960 (erneut) einstellen, ohne Neustart
static bool codec_setup(void)
{
    if (wm8960_init(g_i2c_bus) != ESP_OK) {
        printf("WM8960 antwortet nicht -> 'i2cdiag' eingeben\n");
        return false;
    }
    wm8960_set_mic(g_set.mic, g_set.pga_db, g_set.boost);
    wm8960_set_hp_volume(g_set.hp_vol);
    printf("WM8960 eingestellt%s. Jetzt sollte er den I2S-Takt liefern -> 'status'.\n",
           wm8960_present() ? "" : " (mit Schreibfehlern!)");
    return wm8960_present();
}

static int scan_bus(int sda, int scl, bool *wm)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0, .sda_io_num = sda, .scl_io_num = scl, .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    if (i2c_new_master_bus(&cfg, &bus) != ESP_OK) return -1;
    int n = 0;
    *wm = false;
    for (int a = 0x08; a < 0x78; a++) {
        if (i2c_master_probe(bus, a, 20) == ESP_OK) {
            n++;
            if (a == 0x1A) *wm = true;
        }
    }
    i2c_del_master_bus(bus);
    return n;
}

static int cmd_i2cdiag(int argc, char **argv)
{
    const int sda = CONFIG_NRB_PIN_I2C_SDA, scl = CONFIG_NRB_PIN_I2C_SCL;
    if (wm8960_present()) {
        printf("WM8960 wurde beim Start gefunden, I2C ist in Ordnung.\n");
        return 0;
    }
    printf("I2C-Diagnose (SDA=GP%d, SCL=GP%d)\n", sda, scl);
    // 1) Ruhepegel ohne interne Pull-ups: das WM8960-Board hat eigene 10-k-Pull-ups nach 3,3 V
    wm8960_detach();
    i2c_del_master_bus(g_i2c_bus);
    g_i2c_bus = NULL;
    gpio_config_t io = { .pin_bit_mask = (1ULL << sda) | (1ULL << scl), .mode = GPIO_MODE_INPUT,
                         .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_ENABLE };
    gpio_config(&io);
    vTaskDelay(pdMS_TO_TICKS(20));
    int l_sda = gpio_get_level(sda), l_scl = gpio_get_level(scl);
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io);
    printf("1) Pegel ohne interne Pull-ups (schwacher Pull-down aktiv):\n");
    printf("   SDA (GP%d): %s\n", sda, l_sda ? "HIGH -> Leitung haengt am WM8960-Board, Board hat Strom" :
                                              "LOW  -> nicht verbunden ODER WM8960-Board ohne 3,3 V");
    printf("   SCL (GP%d): %s\n", scl, l_scl ? "HIGH -> Leitung haengt am WM8960-Board, Board hat Strom" :
                                              "LOW  -> nicht verbunden ODER WM8960-Board ohne 3,3 V");
    // 2) normal und vertauscht scannen
    bool wm;
    int n1 = scan_bus(sda, scl, &wm);
    printf("2) Scan SDA=GP%d / SCL=GP%d: %d Geraet(e)%s\n", sda, scl, n1, wm ? ", WM8960 (0x1A) gefunden" : "");
    bool wm2;
    int n2 = scan_bus(scl, sda, &wm2);
    printf("3) Scan VERTAUSCHT SDA=GP%d / SCL=GP%d: %d Geraet(e)%s\n", scl, sda, n2,
           wm2 ? ", WM8960 (0x1A) gefunden" : "");
    printf("Ergebnis: ");
    if (wm) printf("WM8960 antwortet.\n");
    else if (wm2) printf("SDA und SCL sind VERTAUSCHT -> die zwei Draehte an GP9/GP10 tauschen, dann 'restart'.\n");
    else if (!l_sda && !l_scl) printf("keine Spannung auf SDA/SCL -> 3V3/GND des WM8960 und die Pins GP9/GP10 pruefen.\n");
    else printf("Leitungen haben Spannung, aber keine Antwort -> SDA/SCL am WM8960 nach Aufdruck pruefen "
                "(nicht nach Nummer 1-16).\n");
    // urspruenglichen Bus wiederherstellen
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0, .sda_io_num = sda, .scl_io_num = scl, .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true,
    };
    wm8960_i2c_recover(sda, scl);
    i2c_new_master_bus(&cfg, &g_i2c_bus);
    if (wm) codec_setup();
    return 0;
}

static int cmd_codec(int argc, char **argv)
{
    if (!g_i2c_bus) { printf("I2C-Bus nicht aktiv\n"); return 1; }
    return codec_setup() ? 0 : 1;
}

static int cmd_i2ctest(int argc, char **argv)
{
    if (!g_i2c_bus) { printf("I2C-Bus nicht aktiv\n"); return 1; }
    const int n = 100;
    wm8960_detach();
    printf("I2C-Test: je %d x Adresse 0x1A abfragen und %d x schreiben\n", n, n);
    const uint32_t speeds[] = { 100000, 50000, 20000, 10000 };
    uint32_t best = 0;
    int best_w = -1;
    for (size_t i = 0; i < sizeof(speeds) / sizeof(speeds[0]); i++) {
        int p = 0;
        int w = wm8960_i2c_test(g_i2c_bus, speeds[i], n, &p);
        printf("  %3lu kHz: Abfrage %3d/%d OK, Schreiben %3d/%d OK\n",
               (unsigned long)(speeds[i] / 1000), p, n, w, n);
        if (w > best_w) { best_w = w; best = speeds[i]; }
    }
    if (best_w == n) {
        printf("-> Leitung fehlerfrei.\n");
    } else if (best_w >= 50) {
        printf("-> Leitung gestoert (%d%% Fehler), mit Wiederholungen aber nutzbar. Ursache trotzdem suchen.\n",
               n - best_w);
    } else {
        printf("-> Leitung zu schlecht: Versorgung/GND/Steckkontakte am WM8960 pruefen.\n");
        return 0;
    }
    printf("   nehme %lu kHz und stelle den WM8960 ein:\n", (unsigned long)(best / 1000));
    wm8960_set_i2c_hz(best);
    codec_setup();
    return 0;
}

static int cmd_com(int argc, char **argv)
{
    if (argc < 2) {
        com_status_t st;
        com_get_status(&st);
        printf("Intercom: %s %s | Server %s:%u (%s), PIN %s, %d Kanaele, Benutzer '%s'\n",
               com_state_name(st.state), st.msg, g_set.com_host, g_set.com_port,
               st.server_ip[0] ? st.server_ip : "-", g_set.com_pin[0] ? "gesetzt" : "FEHLT", st.nch, st.user);
        com_channel_t ch[COM_MAX_CH];
        int n = com_get_channels(ch, COM_MAX_CH);
        for (int i = 0; i < n; i++) {
            printf("  %2d %-24s %s %s Lautst. %3d%%%s\n", i, ch[i].name, ch[i].joined ? "drin" : "    ",
                   ch[i].talk ? "TALK" : "    ", ch[i].volume, ch[i].talkers ? "  (spricht jemand)" : "");
        }
        printf("com find | com server auto | com on|off | com server <name/ip> [port] | com pin <pin> | com join|leave|talk|untalk <nr> | com vol <nr> <0..100>\n");
        return 0;
    }
    if (!strcmp(argv[1], "find")) {
        com_server_t sv[6];
        int n = com_discover(sv, 6, 1600);
        printf("%d Server gefunden\n", n);
        for (int i = 0; i < n; i++) printf("  %s:%u  %s  (%s)\n", sv[i].ip, sv[i].port, sv[i].name, sv[i].project);
        return 0;
    }
    if (!strcmp(argv[1], "on") || !strcmp(argv[1], "off")) {
        g_set.com_on = !strcmp(argv[1], "on");
    } else if (!strcmp(argv[1], "server") && argc > 2) {
        strlcpy(g_set.com_host, argv[2], sizeof(g_set.com_host));
        if (argc > 3) g_set.com_port = (uint16_t)atoi(argv[3]);
    } else if (!strcmp(argv[1], "pin") && argc > 2) {
        strlcpy(g_set.com_pin, argv[2], sizeof(g_set.com_pin));
    } else if (argc > 2 && (!strcmp(argv[1], "join") || !strcmp(argv[1], "leave"))) {
        com_join(atoi(argv[2]), argv[1][0] == 'j');
        return 0;
    } else if (argc > 2 && (!strcmp(argv[1], "talk") || !strcmp(argv[1], "untalk"))) {
        com_talk(atoi(argv[2]), argv[1][0] == 't');
        return 0;
    } else if (argc > 3 && !strcmp(argv[1], "vol")) {
        com_volume(atoi(argv[2]), atoi(argv[3]));
        return 0;
    } else {
        printf("unbekannt, 'com' ohne Argument zeigt die Hilfe\n");
        return 1;
    }
    save();
    com_restart();
    return 0;
}

// Flash-Modus ohne BOOT-Taste: Download-Boot erzwingen und neu starten (fuer NR-Intercom.bat)
static int cmd_otacode(int argc, char **argv)
{
    if (argc < 2) { printf("otacode <code>  Update-Code fuer Updates ueber WLAN (aktuell '%s')\n", g_set.ota_code); return 1; }
    strlcpy(g_set.ota_code, argv[1], sizeof(g_set.ota_code));
    save();
    return 0;
}

static int cmd_download(int argc, char **argv)
{
    printf("Starte in den Flash-Modus (Download) ...\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));
    REG_SET_FIELD(LP_AON_SYS_CFG_REG, LP_AON_FORCE_DOWNLOAD_BOOT, 1);   // 01 = Download ueber UART/USB
    esp_restart();
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
        { .command = "mic",     .help = "Mikrofon: mic board|jack|both [pga_db] [boost 0..3]", .func = cmd_mic },
        { .command = "gate",    .help = "Mikrofon-Gate: gate <-80..-20 dBFS> | gate off", .func = cmd_gate },
        { .command = "pegel",   .help = "Mikrofonpegel live anzeigen: pegel [sekunden]", .func = cmd_pegel },
        { .command = "vol",     .help = "Kopfhoerer-Lautstaerke: vol 0..100", .func = cmd_vol },
        { .command = "rxgain",  .help = "Hoerverstaerkung Empfang: rxgain 0..24 (dB, mit Begrenzer)", .func = cmd_rxgain },
        { .command = "side",    .help = "Mithoerton im RTP-Modus: side 0..100", .func = cmd_side },
        { .command = "rtp",     .help = "RTP: rtp <ip> [port] | codec | frame | jb | latch | reset", .func = cmd_rtp },
        { .command = "opus",    .help = "Opus: opus <kbit/s> [komplexitaet] [lowdelay|voip]", .func = cmd_opus },
        { .command = "talk",    .help = "Talk: talk [on|off]", .func = cmd_talk },
        { .command = "nettest", .help = "Funk-Messung: nettest <pc-ip> [port] [s] [ms] [bytes]", .func = cmd_nettest },
        { .command = "i2cdiag", .help = "I2C-Diagnose: Pegel messen, SDA/SCL vertauscht testen", .func = cmd_i2cdiag },
        { .command = "com",     .help = "Intercom (NR_Com): com | com server <ip> [port] | com pin <pin> | com on|off | com join <nr>", .func = cmd_com },
        { .command = "otacode", .help = "Update-Code fuer Updates ueber WLAN: otacode <code>", .func = cmd_otacode },
        { .command = "download", .help = "Flash-Modus (Update ohne BOOT-Taste, nutzt NR-Intercom.bat)", .func = cmd_download },
        { .command = "codec",   .help = "WM8960 neu einstellen (ohne Neustart)", .func = cmd_codec },
        { .command = "i2ctest", .help = "I2C-Qualitaet messen (100/50/20/10 kHz)", .func = cmd_i2ctest },
        { .command = "i2cscan", .help = "I2C-Bus absuchen (WM8960 = 0x1A)", .func = cmd_i2cscan },
        { .command = "reg",     .help = "WM8960-Register schreiben: reg <nr> <wert> (hex)", .func = cmd_reg },
        { .command = "log",     .help = "Log-Ausgaben: log on|off", .func = cmd_log },
        { .command = "factory", .help = "Werkseinstellungen", .func = cmd_factory },
        { .command = "restart", .help = "Neustart", .func = cmd_restart },
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
}
