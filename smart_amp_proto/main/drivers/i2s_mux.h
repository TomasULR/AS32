#pragma once

/* Hardware I²S multiplexer (NX3L4684 nebo 74HC4053).
 *
 * MUX přepíná tři I²S signály (BCLK/LRCK/SDATA) mezi dvěma zdroji:
 *   I2S_MUX_ESP — ESP32-S3 jako I²S master (Wi-Fi UDP / FLAC / Opus / tone)
 *   I2S_MUX_BT  — externí BK3266 BT receiver jako I²S master (A2DP)
 *
 * Společný cíl: PCM5102A DAC. Volba zdroje se provádí jediným GPIO pinem
 * (MUX_SEL). LOW = ESP, HIGH = BT (matchuje pin layout NX3L4684 IN1/IN2).
 */

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    I2S_MUX_ESP = 0,
    I2S_MUX_BT  = 1,
} i2s_mux_src_t;

esp_err_t      i2s_mux_init(gpio_num_t sel_pin);
void           i2s_mux_select(i2s_mux_src_t src);
i2s_mux_src_t  i2s_mux_get(void);

#ifdef __cplusplus
}
#endif
