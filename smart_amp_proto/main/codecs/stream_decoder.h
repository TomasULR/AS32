#pragma once

/* LDAC / aptX / aptX HD: STRICTLY DISABLED — proprietary codecs with
 * royalty requirements incompatible with this project's license stance.
 * Supported here: FLAC (BSD 3-Clause) and Opus (Xiph.org BSD). */

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STREAM_CODEC_RAW_PCM = 0,
    STREAM_CODEC_FLAC    = 1,
    STREAM_CODEC_OPUS    = 2,
} stream_codec_t;

esp_err_t stream_decoder_init(void);
esp_err_t stream_decoder_begin(stream_codec_t codec, uint32_t sample_rate, uint8_t channels);

/* Feed one compressed packet. Decoded PCM S16 goes to i2s_dma_push internally. */
esp_err_t stream_decoder_feed(const uint8_t *data, size_t len);
esp_err_t stream_decoder_end(void);

#ifdef __cplusplus
}
#endif
