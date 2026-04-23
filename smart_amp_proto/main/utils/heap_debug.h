#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t heap_debug_start(uint32_t period_ms);
void      heap_debug_log_now(void);

#ifdef __cplusplus
}
#endif
