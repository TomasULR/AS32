/* FLAC + Opus dekódování přes ESP-ADF audio_pipeline.
 *
 * Pipeline shape:
 *
 *     UDP receiver ─► raw_in (writer) ─► flac_decoder
 *                                        opus_decoder ─► raw_out (reader) ─► pump task ─► i2s_dma
 *
 * Každé volání stream_decoder_begin() teardownu předchozí pipeline a postaví
 * novou pro daný kodek (přepínání FLAC/Opus za běhu je vzácné, zato čistý
 * teardown brání memory leaks v decoder elementech).
 *
 * Pump task konzumuje PCM z raw_out a tlačí ho do i2s_dma_push() přes
 * existující ringbuffer + volume_ctrl pipeline (bezešvý interop s WiFi UDP
 * RAW_PCM cestou — tatáž volume control + DMA driver).
 *
 * LDAC / aptX / aptX HD: STRICTLY DISABLED — proprietary codecs.
 * Opus a FLAC jsou BSD licencované, oba jsou v ESP-ADF audio_codec library.
 */

#include "stream_decoder.h"
#include "i2s_dma.h"
#include "volume_ctrl.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "audio_pipeline.h"
#include "audio_element.h"
#include "raw_stream.h"
#include "flac_decoder.h"      /* ADF: flac_decoder_init / DEFAULT_FLAC_DECODER_CONFIG */
#include "opus_decoder.h"      /* ADF: opus_decoder_init / DEFAULT_OPUS_DECODER_CONFIG */

static const char *TAG = "decoder";

#define PUMP_TASK_STACK   4096
#define PUMP_BUF_BYTES    2048
#define PIPELINE_RINGBUF  (8 * 1024)

static stream_codec_t s_codec     = STREAM_CODEC_RAW_PCM;
static i2s_dma_fmt_t  s_fmt       = { .sample_rate = 44100, .channels = 2, .bits_per_sample = 16 };

static audio_pipeline_handle_t s_pipe   = NULL;
static audio_element_handle_t  s_raw_in = NULL;
static audio_element_handle_t  s_dec    = NULL;
static audio_element_handle_t  s_raw_out = NULL;

static TaskHandle_t   s_pump_task = NULL;
static volatile bool  s_pump_run  = false;

/* ----- Pump task: čte PCM z raw_out a tlačí do i2s_dma_push -------------- */
static void pump_task(void *arg)
{
    (void)arg;
    uint8_t *buf = malloc(PUMP_BUF_BYTES);
    if (!buf) { ESP_LOGE(TAG, "pump alloc"); vTaskDelete(NULL); return; }

    while (s_pump_run) {
        int got = raw_stream_read(s_raw_out, (char *)buf, PUMP_BUF_BYTES);
        if (got <= 0) {
            /* AEL_IO_OK = end-of-stream; jiné záporné = chyba dekodéru. */
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        size_t pushed = 0;
        while (pushed < (size_t)got && s_pump_run) {
            size_t w = i2s_dma_push(buf + pushed, (size_t)got - pushed, 50);
            if (!w) { vTaskDelay(pdMS_TO_TICKS(2)); continue; }
            pushed += w;
        }
    }
    free(buf);
    s_pump_task = NULL;
    vTaskDelete(NULL);
}

/* ----- Pipeline lifecycle ------------------------------------------------- */

static void teardown_pipeline(void)
{
    if (s_pump_run) {
        s_pump_run = false;
        /* dej pump tasku čas dosáhnout vTaskDelete */
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (s_pipe) {
        audio_pipeline_stop(s_pipe);
        audio_pipeline_wait_for_stop(s_pipe);
        audio_pipeline_terminate(s_pipe);
        if (s_raw_in)  audio_pipeline_unregister(s_pipe, s_raw_in);
        if (s_dec)     audio_pipeline_unregister(s_pipe, s_dec);
        if (s_raw_out) audio_pipeline_unregister(s_pipe, s_raw_out);
        audio_pipeline_deinit(s_pipe);
        s_pipe = NULL;
    }
    if (s_raw_in)  { audio_element_deinit(s_raw_in);  s_raw_in  = NULL; }
    if (s_dec)     { audio_element_deinit(s_dec);     s_dec     = NULL; }
    if (s_raw_out) { audio_element_deinit(s_raw_out); s_raw_out = NULL; }
}

static esp_err_t build_pipeline(stream_codec_t codec)
{
    audio_pipeline_cfg_t pcfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    s_pipe = audio_pipeline_init(&pcfg);
    ESP_RETURN_ON_FALSE(s_pipe, ESP_FAIL, TAG, "pipeline init");

    /* IN: raw_stream zapisovatelný (sem strkáme komprimovaná data). */
    raw_stream_cfg_t in_cfg = RAW_STREAM_CFG_DEFAULT();
    in_cfg.type = AUDIO_STREAM_WRITER;
    in_cfg.out_rb_size = PIPELINE_RINGBUF;
    s_raw_in = raw_stream_init(&in_cfg);
    ESP_RETURN_ON_FALSE(s_raw_in, ESP_FAIL, TAG, "raw_in");

    /* DECODER */
    if (codec == STREAM_CODEC_FLAC) {
        flac_decoder_cfg_t fcfg = DEFAULT_FLAC_DECODER_CONFIG();
        s_dec = flac_decoder_init(&fcfg);
    } else if (codec == STREAM_CODEC_OPUS) {
        opus_decoder_cfg_t ocfg = DEFAULT_OPUS_DECODER_CONFIG();
        s_dec = opus_decoder_init(&ocfg);
    } else {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_FALSE(s_dec, ESP_FAIL, TAG, "decoder init");

    /* OUT: raw_stream čtený, sem teče dekódované PCM. */
    raw_stream_cfg_t out_cfg = RAW_STREAM_CFG_DEFAULT();
    out_cfg.type = AUDIO_STREAM_READER;
    out_cfg.out_rb_size = PIPELINE_RINGBUF;
    s_raw_out = raw_stream_init(&out_cfg);
    ESP_RETURN_ON_FALSE(s_raw_out, ESP_FAIL, TAG, "raw_out");

    audio_pipeline_register(s_pipe, s_raw_in,  "raw_in");
    audio_pipeline_register(s_pipe, s_dec,     "dec");
    audio_pipeline_register(s_pipe, s_raw_out, "raw_out");

    const char *link[] = { "raw_in", "dec", "raw_out" };
    audio_pipeline_link(s_pipe, link, 3);

    ESP_RETURN_ON_ERROR(audio_pipeline_run(s_pipe), TAG, "pipeline run");

    /* Spusť pump task, který čte z raw_out a posílá do i2s_dma. */
    s_pump_run = true;
    BaseType_t ok = xTaskCreatePinnedToCore(pump_task, "dec_pump",
                                            PUMP_TASK_STACK, NULL, 9, &s_pump_task, 1);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "pump task create");
        s_pump_run = false;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "ADF pipeline up: %s", codec == STREAM_CODEC_FLAC ? "FLAC" : "OPUS");
    return ESP_OK;
}

/* ----- Public API --------------------------------------------------------- */

esp_err_t stream_decoder_init(void)
{
    /* ADF audio_pipeline registrační volání jsou per-instance v build_pipeline().
     * Globální init zde už není potřeba (ADF nemá globální init). */
    ESP_LOGI(TAG, "stream decoder ready: RAW PCM + FLAC + Opus přes ADF pipeline");
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
            teardown_pipeline();
            ESP_LOGI(TAG, "begin RAW_PCM %u Hz %u ch", (unsigned)sample_rate, channels);
            return i2s_dma_reconfigure(&s_fmt);

        case STREAM_CODEC_FLAC:
        case STREAM_CODEC_OPUS:
            /* Pokud už pipeline běží pro stejný kodek, žádný teardown. */
            if (s_pipe && s_codec == codec) return ESP_OK;
            teardown_pipeline();
            return build_pipeline(codec);
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

    /* FLAC / Opus → raw_stream_write blokuje do uvolnění místa v pipeline RB. */
    if (!s_raw_in) return ESP_ERR_INVALID_STATE;
    int wrote = raw_stream_write(s_raw_in, (char *)data, (int)len);
    return (wrote == (int)len) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t stream_decoder_end(void)
{
    /* Konec streamu — necháme decoder vyplavit poslední rámce a tear-down. */
    if (s_pipe) {
        ESP_LOGD(TAG, "end stream → teardown pipeline");
        teardown_pipeline();
    }
    return ESP_OK;
}
