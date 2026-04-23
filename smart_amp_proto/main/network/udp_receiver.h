#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Wire format (little-endian):
 *   [0..3]  magic 'S','O','K','A'
 *   [4]     codec id (0=raw PCM S16, 1=FLAC, 2=Opus)
 *   [5..7]  sample rate (24-bit, Hz)
 *   [8]     channels
 *   [9..10] payload length (uint16)
 *   [11..]  payload
 */
#define SOKOL_UDP_MAGIC  ((uint32_t)'S' | ((uint32_t)'O' << 8) | ((uint32_t)'K' << 16) | ((uint32_t)'A' << 24))

esp_err_t udp_receiver_init(void);
void      udp_receiver_enable(bool enable);
uint32_t  udp_receiver_get_packet_loss(void);

#ifdef __cplusplus
}
#endif
