#pragma once

/* LDAC / aptX / aptX HD: STRICTLY DISABLED — proprietary.
 * AAC (ISO/IEC 14496-3) decode-only usage is license-safe since 2023 patent
 * expirations; SBC is mandatory for Bluetooth A2DP and royalty-free. */

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* BT A2DP SBC frames arrive pre-decoded by Bluedroid (raw S16 PCM),
 * so this module is currently a thin sink that forwards into i2s_dma. */
esp_err_t bt_pcm_sink_init(void);
esp_err_t bt_pcm_sink_push(const uint8_t *pcm, size_t len, uint32_t sample_rate, uint8_t channels);

#ifdef __cplusplus
}
#endif
