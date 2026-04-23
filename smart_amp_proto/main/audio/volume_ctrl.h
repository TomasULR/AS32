#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t volume_ctrl_init(void);

/* 0..100 user-facing scale. Internally mapped to a logarithmic attenuation
 * (-96 dB at 0, 0 dB at 100). Setting the same value is a no-op. */
void volume_ctrl_set_percent(int percent);
int  volume_ctrl_get_percent(void);

/* Apply current gain + one-step fade-ramp to a PCM S16 buffer in place. */
void volume_ctrl_apply_s16(int16_t *samples, size_t frames, int channels);

/* Hard mute flag overrides the percent during apply_s16. */
void volume_ctrl_set_mute(bool muted);
bool volume_ctrl_is_muted(void);

#ifdef __cplusplus
}
#endif
