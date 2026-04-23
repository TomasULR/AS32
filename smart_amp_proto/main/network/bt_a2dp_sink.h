#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bt_a2dp_sink_init(void);
void      bt_a2dp_sink_enable(bool enable);
bool      bt_a2dp_sink_is_connected(void);

#ifdef __cplusplus
}
#endif
