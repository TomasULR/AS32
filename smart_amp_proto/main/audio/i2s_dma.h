#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t sample_rate;
    uint8_t  channels;
    uint8_t  bits_per_sample;   /* 16 only in Phase 1 */
} i2s_dma_fmt_t;

esp_err_t i2s_dma_init(const i2s_dma_fmt_t *fmt);
esp_err_t i2s_dma_reconfigure(const i2s_dma_fmt_t *fmt);

/* Producer API: push PCM S16 interleaved into the ringbuffer.
 * Returns bytes actually written (may be less on ringbuffer full). */
size_t i2s_dma_push(const void *data, size_t bytes, uint32_t timeout_ms);

/* Drop pending audio (used on source switch). */
void i2s_dma_flush(void);

/* Inject a 1 kHz sine test tone for `duration_ms`. Blocks. */
esp_err_t i2s_dma_test_tone(int frequency_hz, int duration_ms, int amplitude_percent);

/* Runtime stats. */
typedef struct {
    uint32_t underruns;
    uint32_t bytes_pushed;
    uint32_t bytes_played;
    size_t   ringbuf_free;
} i2s_dma_stats_t;
void i2s_dma_get_stats(i2s_dma_stats_t *out);

#ifdef __cplusplus
}
#endif
