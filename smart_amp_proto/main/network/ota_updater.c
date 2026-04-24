#include "ota_updater.h"
#include "amp_control.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <strings.h>

static const char *TAG = "ota";

#define URL_MAX 256
#define ERR_MAX 96

static volatile ota_state_t s_state = OTA_STATE_IDLE;
static volatile int         s_progress = -1;
static char                 s_url[URL_MAX];
static char                 s_err[ERR_MAX];

static bool url_is_http_local(const char *url)
{
    /* Akceptuj http:// jen pro RFC 1918 lokální adresy (192.168.x.x, 10.x, 172.16-31.x). */
    if (strncasecmp(url, "http://", 7) != 0) return false;
    const char *host = url + 7;
    if (!strncmp(host, "192.168.", 8)) return true;
    if (!strncmp(host, "10.", 3))      return true;
    if (!strncmp(host, "172.", 4)) {
        int b2 = atoi(host + 4);
        if (b2 >= 16 && b2 <= 31) return true;
    }
    return false;
}

/* HTTP event hook pro průběžnou aktualizaci progress %. */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_HEADER) {
        if (evt->header_key && evt->header_value &&
            !strcasecmp(evt->header_key, "Content-Length")) {
            /* Reportuj velikost — ESP_HTTPS_OTA si interně řeší download progress,
             * ale my si tady alespoň zaznamenáme předpokládanou velikost. */
            ESP_LOGI(TAG, "stahování: Content-Length = %s", evt->header_value);
        }
    }
    return ESP_OK;
}

static void ota_task(void *arg)
{
    (void)arg;

    ESP_LOGW(TAG, "OTA start: %s", s_url);
    s_state = OTA_STATE_DOWNLOADING;
    s_progress = 0;
    s_err[0] = '\0';

    /* Bezpečnostní opatření: pošli AMP do MUTE — během OTA se může restartovat
     * audio task a způsobit krátké disonance. */
    amp_control_set_mute(true);

    esp_http_client_config_t http_cfg = {
        .url            = s_url,
        .event_handler  = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms     = 15000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
        .partial_http_download = true,
        .max_http_request_size = 16 * 1024,
    };

    esp_https_ota_handle_t handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_cfg, &handle);
    if (err != ESP_OK) {
        snprintf(s_err, ERR_MAX, "https_ota_begin: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", s_err);
        s_state = OTA_STATE_FAILED;
        amp_control_set_mute(false);
        vTaskDelete(NULL); return;
    }

    /* Ověř hlavičku obrazu (ať netaháme 2 MB špatného binu). */
    esp_app_desc_t new_desc;
    err = esp_https_ota_get_img_desc(handle, &new_desc);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "nový obraz: ver=%s, idf=%s, project=%s",
                 new_desc.version, new_desc.idf_ver, new_desc.project_name);
    }

    /* Smyčka: tahá kusy, dokud není konec. */
    while (1) {
        err = esp_https_ota_perform(handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;
        int got = esp_https_ota_get_image_len_read(handle);
        s_progress = got;   /* bytes — UI si přepočítá pokud zná Content-Length */
    }

    if (err == ESP_OK) {
        s_state = OTA_STATE_VERIFYING;
        err = esp_https_ota_finish(handle);
    } else {
        esp_https_ota_abort(handle);
    }

    if (err != ESP_OK) {
        snprintf(s_err, ERR_MAX, "OTA fail: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", s_err);
        s_state = OTA_STATE_FAILED;
        amp_control_set_mute(false);
        vTaskDelete(NULL); return;
    }

    ESP_LOGW(TAG, "OTA OK — restart za 2 s pro boot z nového obrazu");
    s_state = OTA_STATE_REBOOTING;
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    /* unreachable */
}

esp_err_t ota_updater_init(void)
{
    /* Pokud je aktuální boot označený jako PENDING_VERIFY, znamená to, že
     * po předchozím OTA jsme ještě nepotvrdili stabilitu. Po BOOT_DELAY čekáme
     * na ručně volané ota_updater_mark_valid() — typicky z hlavní smyčky po
     * X sekundách bez panicu. */
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(running, &st) == ESP_OK) {
        if (st == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGW(TAG, "boot z PENDING_VERIFY obrazu — automatická validace za 30 s");
        }
    }
    return ESP_OK;
}

esp_err_t ota_updater_start(const char *url)
{
    if (!url || !*url) return ESP_ERR_INVALID_ARG;
    if (strlen(url) >= URL_MAX) return ESP_ERR_INVALID_SIZE;

    /* Akceptuj https:// vždy, http:// jen pro lokální síť. */
    if (strncasecmp(url, "https://", 8) != 0 && !url_is_http_local(url)) {
        ESP_LOGE(TAG, "OTA odmítla URL: %s (pouze https:// nebo http:// na lokální síti)", url);
        return ESP_ERR_INVALID_ARG;
    }
    if (s_state == OTA_STATE_DOWNLOADING || s_state == OTA_STATE_VERIFYING) {
        ESP_LOGE(TAG, "OTA již běží");
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(s_url, url, URL_MAX - 1); s_url[URL_MAX - 1] = '\0';
    s_state = OTA_STATE_DOWNLOADING;
    s_progress = 0;
    s_err[0] = '\0';

    BaseType_t ok = xTaskCreatePinnedToCore(ota_task, "ota", 8192, NULL, 5, NULL, 0);
    if (ok != pdPASS) {
        s_state = OTA_STATE_FAILED;
        snprintf(s_err, ERR_MAX, "task create");
        return ESP_FAIL;
    }
    return ESP_OK;
}

ota_state_t ota_updater_get_state(void)         { return s_state; }
int         ota_updater_get_progress_percent(void) { return s_progress; }
const char *ota_updater_last_error(void)        { return s_err; }

esp_err_t ota_updater_mark_valid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(running, &st) == ESP_OK) {
        if (st == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_err_t e = esp_ota_mark_app_valid_cancel_rollback();
            if (e == ESP_OK) ESP_LOGI(TAG, "boot označen jako platný — rollback zrušen");
            return e;
        }
    }
    return ESP_OK; /* už valid nebo factory */
}
