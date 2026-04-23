#include "heap_debug.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

static const char *TAG = "heap";

void heap_debug_log_now(void)
{
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t spiram_free   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t internal_min  = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    size_t spiram_min    = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "internal: %u free (min %u), PSRAM: %u free (min %u)",
             (unsigned)internal_free, (unsigned)internal_min,
             (unsigned)spiram_free, (unsigned)spiram_min);
}

static void task_fn(void *arg)
{
    uint32_t period = (uint32_t)(uintptr_t)arg;
    while (1) {
        heap_debug_log_now();
        vTaskDelay(pdMS_TO_TICKS(period));
    }
}

esp_err_t heap_debug_start(uint32_t period_ms)
{
    if (period_ms < 1000) period_ms = 1000;
    BaseType_t ok = xTaskCreate(task_fn, "heap_dbg", 3072,
                                (void *)(uintptr_t)period_ms, 1, NULL);
    return (ok == pdPASS) ? ESP_OK : ESP_FAIL;
}
