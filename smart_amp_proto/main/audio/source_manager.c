#include "source_manager.h"
#include "amp_control.h"
#include "i2s_dma.h"
#include "nvs_config.h"
#include "i2s_mux.h"
#include "bt_a2dp_sink.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Forward refs to network enablers — implemented in their respective modules. */
extern void udp_receiver_enable(bool enable);

static const char *TAG = "src";
static audio_source_t s_current = SRC_NONE;

/* AVRCP události z BK3266 modulu (přes UART parser v bt_a2dp_sink.c). */
static void on_avrcp(bt_avrcp_event_t ev, void *ctx)
{
    (void)ctx;
    switch (ev) {
        case BT_AVRCP_CONNECTED:
            ESP_LOGI(TAG, "AVRCP: telefon připojen");
            /* Auto-switch na BT pokud aktuálně nehraje WiFi UDP */
            if (s_current == SRC_NONE) source_manager_select(SRC_BT_A2DP);
            break;
        case BT_AVRCP_DISCONNECTED:
            ESP_LOGI(TAG, "AVRCP: telefon odpojen");
            if (s_current == SRC_BT_A2DP) source_manager_select(SRC_NONE);
            break;
        case BT_AVRCP_PLAY:
            ESP_LOGI(TAG, "AVRCP: PLAY → BT zdroj");
            if (s_current != SRC_BT_A2DP) source_manager_select(SRC_BT_A2DP);
            amp_control_set_mute(false);
            break;
        case BT_AVRCP_PAUSE:
            ESP_LOGI(TAG, "AVRCP: PAUSE");
            /* Pauza nemění zdroj — jen logujeme. AMP necháme un-mute,
             * BK3266 stejně přestane vyplivovat I²S data. */
            break;
        case BT_AVRCP_NEXT: ESP_LOGI(TAG, "AVRCP: NEXT track"); break;
        case BT_AVRCP_PREV: ESP_LOGI(TAG, "AVRCP: PREV track"); break;
        case BT_AVRCP_VOL_UP:
        case BT_AVRCP_VOL_DOWN:
            ESP_LOGI(TAG, "AVRCP: vol %s", ev == BT_AVRCP_VOL_UP ? "UP" : "DOWN");
            break;
    }
}

esp_err_t source_manager_init(void)
{
    s_current = (audio_source_t)nvs_cfg_get_last_source();
    if (s_current >= SRC_COUNT) s_current = SRC_NONE;

    /* Zaregistruj AVRCP handler — bt_a2dp_sink_init() musí proběhnout dřív. */
    bt_a2dp_sink_on_avrcp(on_avrcp, NULL);

    ESP_LOGI(TAG, "boot src=%s", source_manager_name(s_current));
    source_manager_select(s_current);
    return ESP_OK;
}

const char *source_manager_name(audio_source_t src)
{
    switch (src) {
        case SRC_WIFI_UDP: return "WiFi";
        case SRC_BT_A2DP:  return "BT";
        case SRC_NONE:
        default:           return "OFF";
    }
}

audio_source_t source_manager_get(void) { return s_current; }

esp_err_t source_manager_select(audio_source_t src)
{
    if (src >= SRC_COUNT) return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "switch %s -> %s",
             source_manager_name(s_current), source_manager_name(src));

    /* 1) MUTE před přepnutím MUX (jinak slyšíš lupnutí). */
    amp_control_set_mute(true);
    vTaskDelay(pdMS_TO_TICKS(15));

    /* 2) Vypni starý zdroj (logicky — produkci dat). */
    switch (s_current) {
        case SRC_WIFI_UDP: udp_receiver_enable(false); break;
        case SRC_BT_A2DP:  bt_a2dp_sink_enable(false); break;
        default: break;
    }
    i2s_dma_flush();

    /* 3) Přepni hardware MUX na nový zdroj. */
    switch (src) {
        case SRC_BT_A2DP:
            i2s_mux_select(I2S_MUX_BT);
            bt_a2dp_sink_enable(true);
            break;
        case SRC_WIFI_UDP:
            i2s_mux_select(I2S_MUX_ESP);
            udp_receiver_enable(true);
            break;
        case SRC_NONE:
        default:
            i2s_mux_select(I2S_MUX_ESP);  /* default zem-pull, klidný stav */
            break;
    }

    s_current = src;
    nvs_cfg_set_last_source((uint8_t)src);

    /* 4) Krátká pauza, ať se MUX a downstream DAC stabilizují, pak un-mute. */
    vTaskDelay(pdMS_TO_TICKS(25));
    if (src != SRC_NONE) amp_control_set_mute(false);
    return ESP_OK;
}

void source_manager_cycle(void)
{
    audio_source_t next = (audio_source_t)((s_current + 1) % SRC_COUNT);
    source_manager_select(next);
}
