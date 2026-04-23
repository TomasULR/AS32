#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t nvs_cfg_init(void);

int       nvs_cfg_get_volume(void);
esp_err_t nvs_cfg_set_volume(int percent);

uint8_t   nvs_cfg_get_last_source(void);
esp_err_t nvs_cfg_set_last_source(uint8_t src);

esp_err_t nvs_cfg_get_wifi(char *ssid_out, size_t ssid_cap, char *pass_out, size_t pass_cap);
esp_err_t nvs_cfg_set_wifi(const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif
