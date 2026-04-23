#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SRC_NONE = 0,
    SRC_WIFI_UDP,
    SRC_BT_A2DP,
    SRC_COUNT
} audio_source_t;

esp_err_t       source_manager_init(void);
esp_err_t       source_manager_select(audio_source_t src);
audio_source_t  source_manager_get(void);
const char     *source_manager_name(audio_source_t src);
void            source_manager_cycle(void);

#ifdef __cplusplus
}
#endif
