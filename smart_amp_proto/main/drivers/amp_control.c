#include "amp_control.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>

#define AMP_MUTE_GPIO  GPIO_NUM_21
#define DAC_MUTE_GPIO  GPIO_NUM_13

static const char *TAG = "amp_ctrl";
static atomic_bool s_muted = ATOMIC_VAR_INIT(true);

static inline void drive_amp(bool mute)
{
    /* TPA3110D2 MUTE: HIGH = muted, LOW = playing. */
    gpio_set_level(AMP_MUTE_GPIO, mute ? 1 : 0);
}

static inline void drive_dac(bool mute)
{
#if CONFIG_SOKOL_DAC_MUTE_PIN_ENABLE
    /* PCM5102A XSMT: LOW = soft-mute, HIGH = normal. */
    gpio_set_level(DAC_MUTE_GPIO, mute ? 0 : 1);
#else
    (void)mute;
#endif
}

esp_err_t amp_control_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << AMP_MUTE_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "amp mute gpio");

#if CONFIG_SOKOL_DAC_MUTE_PIN_ENABLE
    cfg.pin_bit_mask = (1ULL << DAC_MUTE_GPIO);
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "dac mute gpio");
#endif

    /* Boot safe: both muted until app explicitly unmutes. */
    drive_amp(true);
    drive_dac(true);
    atomic_store(&s_muted, true);
    ESP_LOGI(TAG, "Amp MUTE on GPIO%d (active HIGH), booted muted.", AMP_MUTE_GPIO);
    return ESP_OK;
}

esp_err_t amp_control_set_mute(bool mute)
{
    bool prev = atomic_exchange(&s_muted, mute);
    if (prev == mute) return ESP_OK;

    if (mute) {
        drive_amp(true);
        drive_dac(true);
    } else {
        /* Un-mute sequence: DAC first, then settle, then amp — avoids thump. */
        drive_dac(false);
        vTaskDelay(pdMS_TO_TICKS(20));
        drive_amp(false);
    }
    ESP_LOGD(TAG, "mute=%d", mute);
    return ESP_OK;
}

bool amp_control_is_muted(void)
{
    return atomic_load(&s_muted);
}

esp_err_t amp_control_wrap_silent(esp_err_t (*cb)(void *arg), void *arg)
{
    bool was_muted = amp_control_is_muted();
    amp_control_set_mute(true);
    vTaskDelay(pdMS_TO_TICKS(5));

    esp_err_t err = cb ? cb(arg) : ESP_OK;

    vTaskDelay(pdMS_TO_TICKS(20));
    if (!was_muted) {
        amp_control_set_mute(false);
    }
    return err;
}
