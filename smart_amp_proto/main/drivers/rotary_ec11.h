#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ROTARY_EVT_ROTATE_CW  = 1,
    ROTARY_EVT_ROTATE_CCW = 2,
    ROTARY_EVT_PRESS      = 3,   /* short press (release before long threshold) */
    ROTARY_EVT_LONG_PRESS = 4,   /* held >= 800 ms */
} rotary_event_type_t;

typedef struct {
    rotary_event_type_t type;
    int32_t             delta;   /* accumulated ticks for rotate events */
} rotary_event_t;

esp_err_t rotary_ec11_init(void);
QueueHandle_t rotary_ec11_queue(void);

#ifdef __cplusplus
}
#endif
