/* LDAC / aptX / aptX HD: STRICTLY DISABLED — proprietary codecs with
 * royalty requirements incompatible with this project's license stance. */

#include "flac_decoder.h"
#include "i2s_dma.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "decoder";

static stream_codec_t s_codec = STREAM_CODEC_RAW_PCM;
static i2s_dma_fmt_t  s_fmt   = { .sample_rate = 44100, .channels = 2, .bits_per_sample = 16 };

esp_err_t stream_decoder_init(void)
{
    ESP_LOGI(TAG, "stream decoder ready (raw PCM online; FLAC/Opus require ADF pipeline)");
    return ESP_OK;
}

esp_err_t stream_decoder_begin(stream_codec_t codec, uint32_t sample_rate, uint8_t channels)
{
    s_codec = codec;
    s_fmt.sample_rate = sample_rate;
    s_fmt.channels    = channels;
    s_fmt.bits_per_sample = 16;

    switch (codec) {
        case STREAM_CODEC_RAW_PCM:
            ESP_LOGI(TAG, "begin RAW_PCM %u Hz %u ch", (unsigned)sample_rate, channels);
            return i2s_dma_reconfigure(&s_fmt);

        case STREAM_CODEC_FLAC:
        case STREAM_CODEC_OPUS:
            /* ADF pipeline wiring is intentionally deferred for Phase 1.
             * Reference setup (when adding): audio_pipeline_init() →
             *   raw_stream_init(AUDIO_STREAM_WRITER, &in_stream) →
             *   flac_decoder_init() / opus_decoder_init() →
             *   raw_stream_init(AUDIO_STREAM_READER, &out_stream) →
             *   audio_pipeline_register/link/run, then pump PCM from
             *   out_stream into i2s_dma_push() on a dedicated task.
             * See esp-adf/examples/player/pipeline_http_mp3_decoder/. */
            ESP_LOGW(TAG, "codec %d requires ADF pipeline — not enabled in Phase 1", codec);
            return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t stream_decoder_feed(const uint8_t *data, size_t len)
{
    if (!data || !len) return ESP_ERR_INVALID_ARG;

    if (s_codec == STREAM_CODEC_RAW_PCM) {
        size_t pushed = 0;
        while (pushed < len) {
            size_t got = i2s_dma_push(data + pushed, len - pushed, 50);
            if (!got) {
                ESP_LOGD(TAG, "push stalled at %u/%u", (unsigned)pushed, (unsigned)len);
                return ESP_ERR_TIMEOUT;
            }
            pushed += got;
        }
        return ESP_OK;
    }
    /* FLAC/Opus path disabled until ADF pipeline is wired — see begin(). */
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t stream_decoder_end(void)
{
    ESP_LOGD(TAG, "end stream");
    return ESP_OK;
}
