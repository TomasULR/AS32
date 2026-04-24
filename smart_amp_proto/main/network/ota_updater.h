#pragma once

/* OTA firmware updater (HTTPS / HTTP).
 *
 * Stahuje binary z URL do volného OTA partition slotu (ota_0/ota_1, viz
 * partitions.csv), ověří hlavičku, označí nový obraz jako "boot pending"
 * a po restartu se z něj nabootuje. Pokud nový obraz nezvládne 2× boot
 * proběhnout úspěšně (rollback test), bootloader se vrátí na předchozí.
 *
 * Použití typicky z CLI: `ota https://example.com/sa32-fw.bin`
 *
 * Bezpečnost: HTTPS s ověřením certifikátu pomocí cert bundle (CONFIG_MBEDTLS_CERTIFICATE_BUNDLE).
 * HTTP varianta povolena pouze pro lokální síť — auto-povoleno pro 192.168.x.x.
 */

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_REBOOTING,
    OTA_STATE_FAILED,
} ota_state_t;

esp_err_t   ota_updater_init(void);

/* Spustí OTA. Vrací okamžitě (běží na background tasku). */
esp_err_t   ota_updater_start(const char *url);

ota_state_t ota_updater_get_state(void);
int         ota_updater_get_progress_percent(void);   /* -1 = neznámé */
const char *ota_updater_last_error(void);

/* Označit aktuální boot jako úspěšný (ruší rollback timer). */
esp_err_t   ota_updater_mark_valid(void);

#ifdef __cplusplus
}
#endif
