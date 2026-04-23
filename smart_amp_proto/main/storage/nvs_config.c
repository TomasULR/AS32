#include "nvs_config.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_check.h"
#include <string.h>

static const char *TAG = "nvs_cfg";
static const char *NS  = "sokol";

esp_err_t nvs_cfg_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase (%s), reformatting", esp_err_to_name(err));
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "erase");
        err = nvs_flash_init();
    }
    return err;
}

static esp_err_t open_rw(nvs_handle_t *h) { return nvs_open(NS, NVS_READWRITE, h); }

int nvs_cfg_get_volume(void)
{
    nvs_handle_t h;
    if (open_rw(&h) != ESP_OK) return 20;
    int32_t v = 20;
    nvs_get_i32(h, "vol", &v);
    nvs_close(h);
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    return (int)v;
}

esp_err_t nvs_cfg_set_volume(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(open_rw(&h), TAG, "open");
    nvs_set_i32(h, "vol", percent);
    nvs_commit(h);
    nvs_close(h);
    return ESP_OK;
}

uint8_t nvs_cfg_get_last_source(void)
{
    nvs_handle_t h;
    if (open_rw(&h) != ESP_OK) return 0;
    uint8_t s = 0;
    nvs_get_u8(h, "last_src", &s);
    nvs_close(h);
    return s;
}

esp_err_t nvs_cfg_set_last_source(uint8_t src)
{
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(open_rw(&h), TAG, "open");
    nvs_set_u8(h, "last_src", src);
    nvs_commit(h);
    nvs_close(h);
    return ESP_OK;
}

esp_err_t nvs_cfg_get_wifi(char *ssid_out, size_t ssid_cap, char *pass_out, size_t pass_cap)
{
    if (ssid_out) ssid_out[0] = 0;
    if (pass_out) pass_out[0] = 0;
    nvs_handle_t h;
    if (open_rw(&h) != ESP_OK) return ESP_ERR_NVS_NOT_FOUND;
    size_t len;
    if (ssid_out) { len = ssid_cap; nvs_get_str(h, "wifi_ssid", ssid_out, &len); }
    if (pass_out) { len = pass_cap; nvs_get_str(h, "wifi_pass", pass_out, &len); }
    nvs_close(h);
    return ESP_OK;
}

esp_err_t nvs_cfg_set_wifi(const char *ssid, const char *password)
{
    if (!ssid || !password) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(open_rw(&h), TAG, "open");
    nvs_set_str(h, "wifi_ssid", ssid);
    nvs_set_str(h, "wifi_pass", password);
    nvs_commit(h);
    nvs_close(h);
    return ESP_OK;
}
