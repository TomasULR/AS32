#include "source_manager.h"
#include "amp_control.h"
#include "i2s_dma.h"
#include "nvs_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Forward refs to network enablers — implemented in their respective modules. */
extern void udp_receiver_enable(bool enable);
extern void bt_a2dp_sink_enable(bool enable);

static const char *TAG = "src";
static audio_source_t s_current = SRC_NONE;

esp_err_t source_manager_init(void)
{
    s_current = (audio_source_t)nvs_cfg_get_last_source();
    if (s_current >= SRC_COUNT) s_current = SRC_NONE;
    ESP_LOGI(TAG, "boot src=%s", source_manager_name(s_current));
    /* Apply initial source after audio pipeline is already running. */
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

    amp_control_set_mute(true);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Disable the old source so it stops pushing into the ringbuffer. */
    switch (s_current) {
        case SRC_WIFI_UDP: udp_receiver_enable(false); break;
        case SRC_BT_A2DP:  bt_a2dp_sink_enable(false); break;
        default: break;
    }
    i2s_dma_flush();

    /* Enable the new source. */
    switch (src) {
        case SRC_WIFI_UDP: udp_receiver_enable(true); break;
        case SRC_BT_A2DP:  bt_a2dp_sink_enable(true); break;
        default: break;
    }

    s_current = src;
    nvs_cfg_set_last_source((uint8_t)src);

    vTaskDelay(pdMS_TO_TICKS(20));
    if (src != SRC_NONE) {
        amp_control_set_mute(false);
    }
    return ESP_OK;
}

void source_manager_cycle(void)
{
    audio_source_t next = (audio_source_t)((s_current + 1) % SRC_COUNT);
    source_manager_select(next);
}
