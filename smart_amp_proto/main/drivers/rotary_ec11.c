#include "rotary_ec11.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ENC_A_GPIO   GPIO_NUM_10
#define ENC_B_GPIO   GPIO_NUM_11
#define ENC_SW_GPIO  GPIO_NUM_12

#define LONG_PRESS_US  (800 * 1000)

static const char *TAG = "rotary";
static QueueHandle_t s_q = NULL;

/* Ben Buxton half-step state table — rejects bouncy transitions.
 * State encoding: low nibble = FSM state, high nibble = emit (CW/CCW). */
static const uint8_t ENC_TABLE[7][4] = {
    {0x0, 0x2, 0x4, 0x0},
    {0x3, 0x0, 0x1, 0x10}, /* CCW emit */
    {0x3, 0x2, 0x0, 0x0},
    {0x3, 0x2, 0x1, 0x0},
    {0x6, 0x0, 0x4, 0x0},
    {0x6, 0x5, 0x0, 0x20}, /* CW emit */
    {0x6, 0x5, 0x4, 0x0},
};
#define DIR_CW   0x20
#define DIR_CCW  0x10

static uint8_t s_state = 0;

static void IRAM_ATTR isr_enc(void *arg)
{
    (void)arg;
    int a = gpio_get_level(ENC_A_GPIO);
    int b = gpio_get_level(ENC_B_GPIO);
    s_state = ENC_TABLE[s_state & 0x0F][(a << 1) | b];
    uint8_t dir = s_state & 0x30;
    if (dir) {
        rotary_event_t evt = {
            .type = (dir == DIR_CW) ? ROTARY_EVT_ROTATE_CW : ROTARY_EVT_ROTATE_CCW,
            .delta = (dir == DIR_CW) ? 1 : -1,
        };
        BaseType_t hp = pdFALSE;
        xQueueSendFromISR(s_q, &evt, &hp);
        if (hp) portYIELD_FROM_ISR();
    }
}

static int64_t s_press_start_us = 0;
static bool    s_press_down = false;

static void IRAM_ATTR isr_sw(void *arg)
{
    (void)arg;
    int lvl = gpio_get_level(ENC_SW_GPIO);
    int64_t now = esp_timer_get_time();
    BaseType_t hp = pdFALSE;

    if (lvl == 0 && !s_press_down) {
        s_press_down = true;
        s_press_start_us = now;
    } else if (lvl == 1 && s_press_down) {
        int64_t held = now - s_press_start_us;
        s_press_down = false;
        rotary_event_t evt = {
            .type = (held >= LONG_PRESS_US) ? ROTARY_EVT_LONG_PRESS : ROTARY_EVT_PRESS,
            .delta = 0,
        };
        xQueueSendFromISR(s_q, &evt, &hp);
        if (hp) portYIELD_FROM_ISR();
    }
}

esp_err_t rotary_ec11_init(void)
{
    s_q = xQueueCreate(16, sizeof(rotary_event_t));
    ESP_RETURN_ON_FALSE(s_q, ESP_ERR_NO_MEM, TAG, "queue");

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << ENC_A_GPIO) | (1ULL << ENC_B_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "enc AB");

    cfg.pin_bit_mask = (1ULL << ENC_SW_GPIO);
    cfg.intr_type = GPIO_INTR_ANYEDGE;
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "enc SW");

    /* install_isr_service returns ESP_ERR_INVALID_STATE if someone already did; tolerate. */
    esp_err_t ie = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (ie != ESP_OK && ie != ESP_ERR_INVALID_STATE) return ie;

    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(ENC_A_GPIO, isr_enc, NULL), TAG, "isr A");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(ENC_B_GPIO, isr_enc, NULL), TAG, "isr B");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(ENC_SW_GPIO, isr_sw, NULL), TAG, "isr SW");

    ESP_LOGI(TAG, "EC11 on A=%d B=%d SW=%d", ENC_A_GPIO, ENC_B_GPIO, ENC_SW_GPIO);
    return ESP_OK;
}

QueueHandle_t rotary_ec11_queue(void) { return s_q; }
