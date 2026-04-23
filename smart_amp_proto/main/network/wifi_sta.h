#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_sta_init(void);
esp_err_t wifi_sta_set_credentials(const char *ssid, const char *password);
bool      wifi_sta_is_connected(void);
void      wifi_sta_get_ip(char out[16]);

#ifdef __cplusplus
}
#endif
