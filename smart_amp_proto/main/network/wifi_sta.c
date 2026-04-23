#include "wifi_sta.h"
#include "nvs_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "wifi";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_evg = NULL;
static esp_netif_t        *s_netif = NULL;
static int                 s_retry_s = 1;   /* exponential backoff seed */
static bool                s_connected = false;
static char                s_ip[16] = "0.0.0.0";

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        strcpy(s_ip, "0.0.0.0");
        xEventGroupClearBits(s_evg, WIFI_CONNECTED_BIT);
        int wait = s_retry_s;
        s_retry_s = (s_retry_s < 60) ? (s_retry_s * 2) : 60;
        ESP_LOGW(TAG, "disconnected, retrying in %d s", wait);
        vTaskDelay(pdMS_TO_TICKS(wait * 1000));
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&e->ip_info.ip));
        s_connected = true;
        s_retry_s = 1;
        ESP_LOGI(TAG, "got ip %s", s_ip);
        xEventGroupSetBits(s_evg, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_sta_init(void)
{
    s_evg = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_evg, ESP_ERR_NO_MEM, TAG, "evg");

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");
    s_netif = esp_netif_create_default_wifi_sta();
    ESP_RETURN_ON_FALSE(s_netif, ESP_FAIL, TAG, "default sta netif");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init");

    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   event_handler, NULL),
                        TAG, "evt wifi");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                   event_handler, NULL),
                        TAG, "evt ip");

    char ssid[33] = {0}, pass[65] = {0};
    nvs_cfg_get_wifi(ssid, sizeof(ssid), pass, sizeof(pass));
    if (!ssid[0]) {
        ESP_LOGW(TAG, "no WiFi credentials; use CLI `wifi set <ssid> <pass>` then reboot.");
        return ESP_OK;
    }

    wifi_config_t wc = { 0 };
    strncpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid));
    strncpy((char *)wc.sta.password, pass, sizeof(wc.sta.password));
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wc), TAG, "config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start");
    ESP_LOGI(TAG, "connecting to '%s'", ssid);
    return ESP_OK;
}

esp_err_t wifi_sta_set_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password) return ESP_ERR_INVALID_ARG;
    return nvs_cfg_set_wifi(ssid, password);
}

bool wifi_sta_is_connected(void) { return s_connected; }

void wifi_sta_get_ip(char out[16]) { strncpy(out, s_ip, 16); }
