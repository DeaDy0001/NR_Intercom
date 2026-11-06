#include <string.h>
#include "nettest.h"
#include "net.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#define NT_MAGIC 0x4E525431u   // "NRT1"

typedef struct {
    uint32_t magic;
    uint32_t seq;
    int64_t  t_us;
} __attribute__((packed)) nt_hdr_t;

typedef struct {
    struct sockaddr_in dst;
    int seconds;
    int interval_ms;
    int size;
} nt_args_t;

static volatile bool s_running;
static nt_args_t s_args;

static void nettest_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { s_running = false; vTaskDelete(NULL); return; }
    int tos = 0xB8;
    setsockopt(sock, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
    struct timeval tv = { .tv_sec = 0, .tv_usec = 1000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t buf[1400];
    int size = s_args.size;
    if (size < (int)sizeof(nt_hdr_t)) size = sizeof(nt_hdr_t);
    if (size > (int)sizeof(buf)) size = sizeof(buf);
    memset(buf, 0xA5, sizeof(buf));

    uint32_t seq = 0, tot_sent = 0, tot_recv = 0;
    int64_t next = esp_timer_get_time();
    int64_t end = next + (int64_t)s_args.seconds * 1000000;
    int64_t report = next + 1000000;
    // Werte pro Sekunde
    uint32_t sent = 0, recv_n = 0, late = 0;
    int64_t rtt_min = INT64_MAX, rtt_max = 0, rtt_sum = 0, prev_rtt = -1, jit_sum = 0;
    int64_t g_min = INT64_MAX, g_max = 0, g_sum = 0;

    printf("Netztest: %d s, alle %d ms, %d Byte\n", s_args.seconds, s_args.interval_ms, size);
    printf(" s | gesendet empf. Verlust |  RTT min/avg/max ms | Jitter ms | RSSI\n");
    int sec = 0;
    while (s_running && esp_timer_get_time() < end) {
        int64_t now = esp_timer_get_time();
        if (now >= next) {
            nt_hdr_t h = { .magic = NT_MAGIC, .seq = seq++, .t_us = now };
            memcpy(buf, &h, sizeof(h));
            sendto(sock, buf, size, 0, (struct sockaddr *)&s_args.dst, sizeof(s_args.dst));
            sent++; tot_sent++;
            next += (int64_t)s_args.interval_ms * 1000;
        }
        int r = recv(sock, buf, sizeof(buf), 0);
        if (r >= (int)sizeof(nt_hdr_t)) {
            nt_hdr_t h;
            memcpy(&h, buf, sizeof(h));
            if (h.magic == NT_MAGIC) {
                int64_t rtt = esp_timer_get_time() - h.t_us;
                if (rtt > 1000000) { late++; }
                else {
                    recv_n++; tot_recv++;
                    rtt_sum += rtt; g_sum += rtt;
                    if (rtt < rtt_min) rtt_min = rtt;
                    if (rtt > rtt_max) rtt_max = rtt;
                    if (rtt < g_min) g_min = rtt;
                    if (rtt > g_max) g_max = rtt;
                    if (prev_rtt >= 0) jit_sum += rtt > prev_rtt ? rtt - prev_rtt : prev_rtt - rtt;
                    prev_rtt = rtt;
                }
            }
        }
        if (esp_timer_get_time() >= report) {
            net_info_t ni;
            net_get_info(&ni);
            sec++;
            float loss = sent ? 100.0f * (float)(sent > recv_n ? sent - recv_n : 0) / sent : 0;
            if (recv_n) {
                printf("%2d | %8lu %5lu %6.1f%% | %5.1f %5.1f %6.1f | %8.2f | %d dBm ch%d\n", sec,
                       (unsigned long)sent, (unsigned long)recv_n, loss, rtt_min / 1000.0,
                       rtt_sum / 1000.0 / recv_n, rtt_max / 1000.0,
                       recv_n > 1 ? jit_sum / 1000.0 / (recv_n - 1) : 0.0, ni.rssi, ni.channel);
            } else {
                printf("%2d | %8lu %5lu %6.1f%% |   keine Antwort      |          | %d dBm ch%d\n", sec,
                       (unsigned long)sent, 0UL, loss, ni.rssi, ni.channel);
            }
            sent = recv_n = 0;
            rtt_min = INT64_MAX; rtt_max = 0; rtt_sum = 0; jit_sum = 0; prev_rtt = -1;
            report += 1000000;
        }
    }
    if (tot_recv) {
        printf("Gesamt: %lu gesendet, %lu empfangen (%.2f%% Verlust, %lu verspaetet), RTT %.1f/%.1f/%.1f ms\n",
               (unsigned long)tot_sent, (unsigned long)tot_recv,
               100.0f * (float)(tot_sent - tot_recv) / tot_sent, (unsigned long)late,
               g_min / 1000.0, g_sum / 1000.0 / tot_recv, g_max / 1000.0);
    } else {
        printf("Gesamt: keine Antworten. Laeuft tools/udp_echo.py und stimmt die IP?\n");
    }
    close(sock);
    s_running = false;
    vTaskDelete(NULL);
}

esp_err_t nettest_start(const char *host, uint16_t port, int seconds, int interval_ms, int size)
{
    if (s_running) return ESP_ERR_INVALID_STATE;
    memset(&s_args, 0, sizeof(s_args));
    s_args.dst.sin_family = AF_INET;
    s_args.dst.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &s_args.dst.sin_addr) != 1) return ESP_ERR_INVALID_ARG;
    s_args.seconds = seconds > 0 ? seconds : 10;
    s_args.interval_ms = interval_ms > 0 ? interval_ms : 20;
    s_args.size = size;
    s_running = true;
    xTaskCreate(nettest_task, "nettest", 4096, NULL, 5, NULL);
    return ESP_OK;
}

void nettest_stop(void)
{
    s_running = false;
}
