#include "udp_receiver.h"
#include "flac_decoder.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdatomic.h>

static const char *TAG = "udp_rx";
#define RX_BUF 1600

static TaskHandle_t   s_task = NULL;
static atomic_bool    s_enabled = ATOMIC_VAR_INIT(false);
static int            s_sock = -1;
static volatile uint32_t s_packet_loss = 0;
static uint16_t        s_last_seq = 0;
static bool            s_seq_seen = false;

static void rx_task(void *arg)
{
    (void)arg;
    uint8_t *buf = malloc(RX_BUF);
    if (!buf) { vTaskDelete(NULL); return; }

    while (1) {
        if (!atomic_load(&s_enabled)) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (s_sock < 0) {
            s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
            if (s_sock < 0) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }
            struct sockaddr_in a = { 0 };
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = htonl(INADDR_ANY);
            a.sin_port = htons(CONFIG_SOKOL_UDP_PORT);
            if (bind(s_sock, (struct sockaddr *)&a, sizeof(a)) < 0) {
                ESP_LOGE(TAG, "bind failed errno=%d", errno);
                close(s_sock); s_sock = -1; vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            struct timeval tv = { .tv_sec = 0, .tv_usec = 200000 };
            setsockopt(s_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            ESP_LOGI(TAG, "listening on UDP/%d", CONFIG_SOKOL_UDP_PORT);
        }

        int n = recv(s_sock, buf, RX_BUF, 0);
        if (n < 11) continue;

        uint32_t magic;
        memcpy(&magic, buf, 4);
        if (magic != SOKOL_UDP_MAGIC) {
            ESP_LOGD(TAG, "bad magic 0x%08X", (unsigned)magic);
            continue;
        }
        uint8_t  codec    = buf[4];
        uint32_t rate     = (uint32_t)buf[5] | ((uint32_t)buf[6] << 8) | ((uint32_t)buf[7] << 16);
        uint8_t  channels = buf[8];
        uint16_t paylen;
        memcpy(&paylen, &buf[9], 2);
        if (11 + paylen > n) continue;

        /* Best-effort loss detection: header carries no seq; we approximate
         * by checking monotonic arrival count in the first two payload bytes
         * when codec == RAW PCM (upper layer may insert its own seq). */
        if (codec == STREAM_CODEC_RAW_PCM && paylen >= 2) {
            uint16_t seq;
            memcpy(&seq, &buf[11], 2);
            if (s_seq_seen && (uint16_t)(seq - s_last_seq) > 1) {
                s_packet_loss += (uint16_t)(seq - s_last_seq - 1);
            }
            s_last_seq = seq; s_seq_seen = true;
        }

        stream_decoder_begin((stream_codec_t)codec, rate, channels);
        stream_decoder_feed(&buf[11], paylen);
    }
}

esp_err_t udp_receiver_init(void)
{
    BaseType_t ok = xTaskCreatePinnedToCore(rx_task, "udp_rx", 4096, NULL, 8, &s_task, 0);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_FAIL, TAG, "task");
    return ESP_OK;
}

void udp_receiver_enable(bool enable)
{
    atomic_store(&s_enabled, enable);
    if (!enable && s_sock >= 0) {
        close(s_sock); s_sock = -1;
    }
    s_seq_seen = false;
    ESP_LOGI(TAG, "%s", enable ? "enabled" : "disabled");
}

uint32_t udp_receiver_get_packet_loss(void) { return s_packet_loss; }
