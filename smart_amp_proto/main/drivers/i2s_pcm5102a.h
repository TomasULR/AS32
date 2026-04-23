#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2s_std.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    I2S_PCM_RATE_44100 = 44100,
    I2S_PCM_RATE_48000 = 48000,
    I2S_PCM_RATE_88200 = 88200,
    I2S_PCM_RATE_96000 = 96000,
} i2s_pcm_rate_t;

esp_err_t i2s_pcm5102a_init(i2s_pcm_rate_t rate);
esp_err_t i2s_pcm5102a_set_rate(i2s_pcm_rate_t rate);
esp_err_t i2s_pcm5102a_write(const void *buf, size_t bytes, size_t *written, uint32_t timeout_ms);
i2s_chan_handle_t i2s_pcm5102a_tx_handle(void);
esp_err_t i2s_pcm5102a_deinit(void);

#ifdef __cplusplus
}
#endif
