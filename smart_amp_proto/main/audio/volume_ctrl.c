#include "volume_ctrl.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include <math.h>

static const char *TAG = "vol";

/* Q1.15 fixed-point gain: 32767 == 0 dB, 0 == -inf.
 * target_q15 moves toward cur_q15 in apply_s16 steps — 24 ms fade at 44.1 kHz. */
static volatile int32_t s_target_q15 = 32767;
static volatile int32_t s_cur_q15    = 0;        /* boot silent */
static volatile bool    s_muted      = true;
static int              s_percent    = 0;

/* dB per step at percent=p: -96 dB at 0 … 0 dB at 100, quadratic-ish curve. */
static int32_t percent_to_q15(int p)
{
    if (p <= 0) return 0;
    if (p >= 100) return 32767;
    /* Perceptually-linear taper: normalised x^2 with -60 dB floor. */
    float x = (float)p / 100.0f;
    float lin = x * x;                /* 0..1 */
    lin = 0.001f + 0.999f * lin;      /* clamp floor to ~-60 dB */
    int32_t q = (int32_t)(lin * 32767.0f + 0.5f);
    if (q > 32767) q = 32767;
    if (q < 0) q = 0;
    return q;
}

esp_err_t volume_ctrl_init(void)
{
    s_percent = 20;
    s_target_q15 = percent_to_q15(s_percent);
    s_cur_q15 = 0;
    s_muted = false;
    ESP_LOGI(TAG, "init: vol=%d%% q15=%d", s_percent, (int)s_target_q15);
    return ESP_OK;
}

void volume_ctrl_set_percent(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    s_percent = percent;
    s_target_q15 = percent_to_q15(percent);
}

int volume_ctrl_get_percent(void) { return s_percent; }

void volume_ctrl_set_mute(bool muted) { s_muted = muted; }
bool volume_ctrl_is_muted(void) { return s_muted; }

IRAM_ATTR void volume_ctrl_apply_s16(int16_t *samples, size_t frames, int channels)
{
    int32_t target = s_muted ? 0 : s_target_q15;
    int32_t cur = s_cur_q15;

    /* ~1024 step ramp — inaudible zipper, fast enough for interactive volume. */
    const int32_t step = 32;

    for (size_t f = 0; f < frames; f++) {
        if (cur < target) {
            cur += step; if (cur > target) cur = target;
        } else if (cur > target) {
            cur -= step; if (cur < target) cur = target;
        }
        for (int c = 0; c < channels; c++) {
            int32_t v = (int32_t)samples[f * channels + c];
            v = (v * cur) >> 15;
            if (v > 32767) v = 32767;
            else if (v < -32768) v = -32768;
            samples[f * channels + c] = (int16_t)v;
        }
    }
    s_cur_q15 = cur;
}
