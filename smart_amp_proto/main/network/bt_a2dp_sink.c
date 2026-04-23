/* LDAC / aptX / aptX HD: STRICTLY DISABLED — SBC only. */

#include "bt_a2dp_sink.h"
#include "aac_sbc_decoder.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_log.h"
#include "esp_check.h"
#include <stdatomic.h>
#include <string.h>

static const char *TAG = "bt_a2dp";
static const char *DEV_NAME = "SokolAudio";

static atomic_bool s_enabled = ATOMIC_VAR_INIT(false);
static atomic_bool s_connected = ATOMIC_VAR_INIT(false);
static uint32_t    s_rate = 44100;
static uint8_t     s_channels = 2;

static void a2dp_sink_cb(esp_a2d_cb_event_t ev, esp_a2d_cb_param_t *p)
{
    switch (ev) {
        case ESP_A2D_CONNECTION_STATE_EVT:
            atomic_store(&s_connected,
                         p->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED);
            ESP_LOGI(TAG, "A2DP conn state = %d", p->conn_stat.state);
            break;
        case ESP_A2D_AUDIO_STATE_EVT:
            ESP_LOGI(TAG, "audio state = %d", p->audio_stat.state);
            break;
        case ESP_A2D_AUDIO_CFG_EVT: {
            /* SBC config — extract sample rate. */
            uint8_t oct0 = p->audio_cfg.mcc.cie.sbc[0];
            s_channels = 2;
            if (oct0 & (0x01 << 6)) s_rate = 32000;
            else if (oct0 & (0x01 << 5)) s_rate = 44100;
            else if (oct0 & (0x01 << 4)) s_rate = 48000;
            else s_rate = 16000;
            ESP_LOGI(TAG, "SBC cfg: %u Hz, %u ch", (unsigned)s_rate, s_channels);
            break;
        }
        default: break;
    }
}

static void a2dp_data_cb(const uint8_t *data, uint32_t len)
{
    if (!atomic_load(&s_enabled)) return;
    bt_pcm_sink_push(data, len, s_rate, s_channels);
}

static void avrc_ct_cb(esp_avrc_ct_cb_event_t ev, esp_avrc_ct_cb_param_t *p)
{
    (void)ev; (void)p; /* play/pause/next wiring out-of-scope for Phase 1 */
}

esp_err_t bt_a2dp_sink_init(void)
{
    ESP_RETURN_ON_ERROR(esp_bt_controller_mem_release(ESP_BT_MODE_BLE), TAG, "ble mem release");

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_bt_controller_init(&bt_cfg), TAG, "ctrl init");
    ESP_RETURN_ON_ERROR(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT), TAG, "ctrl enable");
    ESP_RETURN_ON_ERROR(esp_bluedroid_init(), TAG, "bluedroid init");
    ESP_RETURN_ON_ERROR(esp_bluedroid_enable(), TAG, "bluedroid enable");

    ESP_RETURN_ON_ERROR(esp_bt_gap_set_device_name(DEV_NAME), TAG, "dev name");
    ESP_RETURN_ON_ERROR(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE,
                                                 ESP_BT_GENERAL_DISCOVERABLE),
                        TAG, "scan mode");

    ESP_RETURN_ON_ERROR(esp_a2d_register_callback(a2dp_sink_cb), TAG, "a2d cb");
    ESP_RETURN_ON_ERROR(esp_a2d_sink_register_data_callback(a2dp_data_cb), TAG, "data cb");
    ESP_RETURN_ON_ERROR(esp_a2d_sink_init(), TAG, "a2d sink init");

    ESP_RETURN_ON_ERROR(esp_avrc_ct_init(), TAG, "avrc init");
    ESP_RETURN_ON_ERROR(esp_avrc_ct_register_callback(avrc_ct_cb), TAG, "avrc cb");

    bt_pcm_sink_init();
    ESP_LOGI(TAG, "BT A2DP sink '%s' up (SBC only)", DEV_NAME);
    return ESP_OK;
}

void bt_a2dp_sink_enable(bool enable)
{
    atomic_store(&s_enabled, enable);
    ESP_LOGI(TAG, "%s", enable ? "enabled" : "disabled");
}

bool bt_a2dp_sink_is_connected(void) { return atomic_load(&s_connected); }
