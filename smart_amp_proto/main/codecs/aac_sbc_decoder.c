/* LDAC / aptX / aptX HD: STRICTLY DISABLED — proprietary. */

#include "aac_sbc_decoder.h"
#include "i2s_dma.h"
#include "esp_log.h"

static const char *TAG = "bt_sink";

static uint32_t s_last_rate = 0;
static uint8_t  s_last_ch   = 0;

esp_err_t bt_pcm_sink_init(void) { return ESP_OK; }

esp_err_t bt_pcm_sink_push(const uint8_t *pcm, size_t len, uint32_t sample_rate, uint8_t channels)
{
    if (!pcm || !len) return ESP_ERR_INVALID_ARG;

    if (sample_rate != s_last_rate || channels != s_last_ch) {
        i2s_dma_fmt_t fmt = { .sample_rate = sample_rate, .channels = channels, .bits_per_sample = 16 };
        esp_err_t e = i2s_dma_reconfigure(&fmt);
        if (e != ESP_OK) {
            ESP_LOGE(TAG, "reconfig %u Hz / %u ch failed: %s",
                     (unsigned)sample_rate, channels, esp_err_to_name(e));
            return e;
        }
        s_last_rate = sample_rate;
        s_last_ch   = channels;
        ESP_LOGI(TAG, "A2DP format -> %u Hz %u ch", (unsigned)sample_rate, channels);
    }

    size_t pushed = 0;
    while (pushed < len) {
        size_t got = i2s_dma_push(pcm + pushed, len - pushed, 50);
        if (!got) return ESP_ERR_TIMEOUT;
        pushed += got;
    }
    return ESP_OK;
}
