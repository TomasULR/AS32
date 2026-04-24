#pragma once

/* External Bluetooth A2DP receiver controller.
 *
 * v1.1: ESP32-S3 nemá BR/EDR rádio, takže native A2DP nelze realizovat.
 * Místo toho se používá externí I²S BT receiver (BK3266 nebo kompatibilní).
 * Modul je řízen přes UART AT-příkazy a jeho I²S výstup je multiplexován
 * do PCM5102A přes hardware MUX (viz drivers/i2s_mux.h).
 *
 * Tento header zachovává původní API pro source_manager kompatibilitu —
 * pouze sémantika je nyní "kontroluj externí BT modul", ne "přijímej PCM".
 */

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* AVRCP události z BT receiveru (parsované z UART odpovědí). */
typedef enum {
    BT_AVRCP_PLAY      = 0,
    BT_AVRCP_PAUSE     = 1,
    BT_AVRCP_NEXT      = 2,
    BT_AVRCP_PREV      = 3,
    BT_AVRCP_VOL_UP    = 4,
    BT_AVRCP_VOL_DOWN  = 5,
    BT_AVRCP_CONNECTED    = 10,
    BT_AVRCP_DISCONNECTED = 11,
} bt_avrcp_event_t;

typedef void (*bt_avrcp_cb_t)(bt_avrcp_event_t ev, void *ctx);

esp_err_t bt_a2dp_sink_init(void);
void      bt_a2dp_sink_enable(bool enable);
bool      bt_a2dp_sink_is_connected(void);

/* Registrace callbacku pro AVRCP události (volá se ze source_manageru). */
void      bt_a2dp_sink_on_avrcp(bt_avrcp_cb_t cb, void *ctx);

/* Aktivní řízení vzdálené strany (telefon) přes AVRCP. */
esp_err_t bt_a2dp_sink_send(bt_avrcp_event_t cmd);

/* Hluboký reset BT modulu (pro recovery / unpair). */
esp_err_t bt_a2dp_sink_reset(void);

#ifdef __cplusplus
}
#endif
