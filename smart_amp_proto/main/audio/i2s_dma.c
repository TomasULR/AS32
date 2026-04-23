#include "i2s_dma.h"
#include "i2s_pcm5102a.h"
#include "volume_ctrl.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include <math.h>
#include <string.h>

#define FEED_TASK_STACK   4096
#define FEED_TASK_CORE    1
#define FEED_TASK_PRIO    22
#define FEED_CHUNK_BYTES  2048  /* ~11.6 ms @ 44.1 kHz stereo S16 */

static const char *TAG = "i2s_dma";

static RingbufHandle_t s_rb = NULL;
static TaskHandle_t    s_feed_task = NULL;
static i2s_dma_fmt_t   s_fmt = { 44100, 2, 16 };
static volatile uint32_t s_underruns = 0;
static volatile uint32_t s_bytes_pushed = 0;
static volatile uint32_t s_bytes_played = 0;

static IRAM_ATTR void feed_task(void *arg)
{
    (void)arg;
    static int16_t silence[FEED_CHUNK_BYTES / 2];
    memset(silence, 0, sizeof(silence));

    while (1) {
        size_t got = 0;
        void *item = xRingbufferReceiveUpTo(s_rb, &got, pdMS_TO_TICKS(20), FEED_CHUNK_BYTES);
        if (item && got) {
            volume_ctrl_apply_s16((int16_t *)item, got / (s_fmt.channels * 2), s_fmt.channels);
            size_t written = 0;
            i2s_pcm5102a_write(item, got, &written, 100);
            s_bytes_played += written;
            vRingbufferReturnItem(s_rb, item);
        } else {
            /* Underrun — feed silence to keep BCLK/LRCK running. */
            size_t written = 0;
            i2s_pcm5102a_write(silence, FEED_CHUNK_BYTES, &written, 100);
            s_underruns++;
        }
    }
}

esp_err_t i2s_dma_init(const i2s_dma_fmt_t *fmt)
{
    if (s_rb) return ESP_ERR_INVALID_STATE;
    if (fmt) s_fmt = *fmt;

    size_t rb_bytes = (size_t)CONFIG_SOKOL_RINGBUFFER_SIZE_KB * 1024;
    /* No-split = byte-oriented ringbuffer in PSRAM. */
    StaticRingbuffer_t *rbs = heap_caps_malloc(sizeof(StaticRingbuffer_t), MALLOC_CAP_INTERNAL);
    uint8_t *rb_storage     = heap_caps_malloc(rb_bytes, MALLOC_CAP_SPIRAM);
    if (!rbs || !rb_storage) {
        ESP_LOGE(TAG, "ringbuffer alloc failed (internal=%p spiram=%p size=%u)",
                 rbs, rb_storage, (unsigned)rb_bytes);
        free(rbs); free(rb_storage);
        return ESP_ERR_NO_MEM;
    }
    s_rb = xRingbufferCreateStatic(rb_bytes, RINGBUF_TYPE_BYTEBUF, rb_storage, rbs);
    ESP_RETURN_ON_FALSE(s_rb, ESP_ERR_NO_MEM, TAG, "ringbuffer create");

    BaseType_t ok = xTaskCreatePinnedToCore(feed_task, "i2s_feed", FEED_TASK_STACK,
                                            NULL, FEED_TASK_PRIO, &s_feed_task, FEED_TASK_CORE);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_FAIL, TAG, "feed task");

    ESP_LOGI(TAG, "DMA pipeline up: %u Hz %u ch, ringbuffer %u KiB in PSRAM",
             (unsigned)s_fmt.sample_rate, s_fmt.channels, (unsigned)(rb_bytes / 1024));
    return ESP_OK;
}

esp_err_t i2s_dma_reconfigure(const i2s_dma_fmt_t *fmt)
{
    if (!fmt) return ESP_ERR_INVALID_ARG;
    if (fmt->sample_rate == s_fmt.sample_rate &&
        fmt->channels == s_fmt.channels &&
        fmt->bits_per_sample == s_fmt.bits_per_sample) {
        return ESP_OK;
    }
    i2s_dma_flush();
    esp_err_t err = i2s_pcm5102a_set_rate((i2s_pcm_rate_t)fmt->sample_rate);
    if (err == ESP_OK) s_fmt = *fmt;
    return err;
}

size_t i2s_dma_push(const void *data, size_t bytes, uint32_t timeout_ms)
{
    if (!s_rb || !data || !bytes) return 0;
    BaseType_t r = xRingbufferSend(s_rb, data, bytes, pdMS_TO_TICKS(timeout_ms));
    if (r == pdTRUE) {
        s_bytes_pushed += bytes;
        return bytes;
    }
    return 0;
}

void i2s_dma_flush(void)
{
    if (!s_rb) return;
    size_t got;
    while (1) {
        void *p = xRingbufferReceive(s_rb, &got, 0);
        if (!p) break;
        vRingbufferReturnItem(s_rb, p);
    }
}

esp_err_t i2s_dma_test_tone(int frequency_hz, int duration_ms, int amplitude_percent)
{
    if (frequency_hz <= 0 || duration_ms <= 0) return ESP_ERR_INVALID_ARG;
    if (amplitude_percent < 0) amplitude_percent = 0;
    if (amplitude_percent > 100) amplitude_percent = 100;

    const uint32_t sr = s_fmt.sample_rate;
    const int ch = s_fmt.channels;
    const int total_frames = (int)((int64_t)sr * duration_ms / 1000);
    const float omega = 2.0f * (float)M_PI * (float)frequency_hz / (float)sr;
    const int16_t peak = (int16_t)(32767 * amplitude_percent / 100);

    int16_t buf[256];
    int frames_done = 0;
    float phase = 0.0f;
    while (frames_done < total_frames) {
        int chunk_frames = (total_frames - frames_done) > (int)(sizeof(buf) / (2 * ch))
                           ? (int)(sizeof(buf) / (2 * ch))
                           : (total_frames - frames_done);
        for (int i = 0; i < chunk_frames; i++) {
            int16_t v = (int16_t)(peak * sinf(phase));
            for (int c = 0; c < ch; c++) buf[i * ch + c] = v;
            phase += omega;
            if (phase > 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
        }
        size_t pushed = 0, total = chunk_frames * ch * 2;
        while (pushed < total) {
            pushed += i2s_dma_push((uint8_t *)buf + pushed, total - pushed, 100);
        }
        frames_done += chunk_frames;
    }
    return ESP_OK;
}

void i2s_dma_get_stats(i2s_dma_stats_t *out)
{
    if (!out) return;
    out->underruns = s_underruns;
    out->bytes_pushed = s_bytes_pushed;
    out->bytes_played = s_bytes_played;
    out->ringbuf_free = s_rb ? xRingbufferGetCurFreeSize(s_rb) : 0;
}
