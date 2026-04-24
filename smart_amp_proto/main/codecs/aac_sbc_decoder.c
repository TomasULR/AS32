/* DEPRECATED v1.1.
 *
 * V Phase 1 přijímal Bluedroid PCM frames z BT A2DP a forwardoval je do
 * i2s_dma. v1.1 nahradil ESP32-S3 native A2DP externím BK3266 BT receiverem
 * (viz network/bt_a2dp_sink.c) — BT audio nyní teče přes hardware MUX přímo
 * do PCM5102A a nikdy se nedostane do ESP32. Tento modul tedy nikdo nevolá.
 *
 * Funkce zůstávají jako bezpečné no-op stuby kvůli ABI a případným externím
 * konzumentům. Vyloučí se z buildu po jednom dalším release cyklu.
 */

#include "aac_sbc_decoder.h"
#include "esp_log.h"

static const char *TAG = "bt_sink_legacy";

esp_err_t bt_pcm_sink_init(void) { return ESP_OK; }

esp_err_t bt_pcm_sink_push(const uint8_t *pcm, size_t len, uint32_t sample_rate, uint8_t channels)
{
    (void)pcm; (void)len; (void)sample_rate; (void)channels;
    static bool warned = false;
    if (!warned) {
        ESP_LOGW(TAG, "bt_pcm_sink_push() volán — v1.1 BT teče HW MUX-em mimo ESP32");
        warned = true;
    }
    return ESP_OK;
}
