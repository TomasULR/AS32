#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_W 128
#define OLED_H 64

esp_err_t oled_init(void);
void oled_clear(void);
void oled_flush(void);

/* Draw 6x8 ASCII text at pixel column x, page y (0..7). */
void oled_draw_text(int x, int page, const char *s);

/* Volume bar on full bottom row: 0..100. */
void oled_draw_volume_bar(int percent);

/* Source indicator in the top-right corner (3 chars). */
void oled_draw_source(const char *tag);

/* Convenience: header + numeric volume + source + bar. */
void oled_render_status(const char *title, const char *source, int vol_percent);

#ifdef __cplusplus
}
#endif
