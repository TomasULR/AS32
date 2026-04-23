#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* TPA3110D2 MUTE on GPIO21 is active HIGH.
 * DAC XSMT on GPIO13 (if wired) is active LOW (drive HIGH = normal, LOW = mute).
 */
esp_err_t amp_control_init(void);
esp_err_t amp_control_set_mute(bool mute);
bool      amp_control_is_muted(void);

/* Atomic sequence: mute → callback(arg) → 20 ms settle → unmute.
 * Caller passes a reconfiguration lambda (e.g. sample rate change). */
esp_err_t amp_control_wrap_silent(esp_err_t (*cb)(void *arg), void *arg);

#ifdef __cplusplus
}
#endif
