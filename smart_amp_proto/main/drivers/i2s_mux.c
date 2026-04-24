#include "i2s_mux.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "i2s_mux";

static gpio_num_t   s_sel = GPIO_NUM_NC;
static i2s_mux_src_t s_cur = I2S_MUX_ESP;

esp_err_t i2s_mux_init(gpio_num_t sel_pin)
{
    s_sel = sel_pin;
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << sel_pin,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,   /* default ESP po power-on */
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "gpio_config");
    gpio_set_level(sel_pin, 0);
    s_cur = I2S_MUX_ESP;
    ESP_LOGI(TAG, "MUX init on GPIO %d, default = ESP", sel_pin);
    return ESP_OK;
}

void i2s_mux_select(i2s_mux_src_t src)
{
    if (s_sel == GPIO_NUM_NC) return;
    if (s_cur == src) return;
    /* Schválně NEHLÁSÍME chybu — MUX je tichá HW operace, ale logujeme přepnutí. */
    gpio_set_level(s_sel, src == I2S_MUX_BT ? 1 : 0);
    s_cur = src;
    ESP_LOGI(TAG, "MUX -> %s", src == I2S_MUX_BT ? "BT" : "ESP");
}

i2s_mux_src_t i2s_mux_get(void) { return s_cur; }
