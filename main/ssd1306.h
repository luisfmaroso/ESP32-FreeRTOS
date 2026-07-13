/**
 * ssd1306.h — minimal framebuffer driver for a 128x64 SSD1306 OLED over I2C.
 *
 * Why a hand-rolled driver instead of a managed component? Two reasons,
 * both documented in the README:
 *  1. Zero external dependencies — the project builds with a bare ESP-IDF
 *     install, no component-manager downloads.
 *  2. It keeps the I2C traffic explicit, which matters for the teaching
 *     point of this project: the display task must be able to say exactly
 *     where its slow I/O happens (ssd1306_flush) so the reader can verify
 *     the mutex is never held across it.
 *
 * Usage model: draw into the RAM framebuffer with the text functions, then
 * push the whole 1 KiB buffer to the panel with ssd1306_flush(). Text is a
 * classic 5x7 font in 6x8 cells -> 21 columns x 8 rows.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 64
#define SSD1306_TEXT_COLS (SSD1306_WIDTH / 6)   /* 21 */
#define SSD1306_TEXT_ROWS (SSD1306_HEIGHT / 8)  /* 8  */

/* Creates the I2C master bus + device and sends the panel init sequence.
 * Call once, before any other ssd1306_* function. */
esp_err_t ssd1306_init(void);

/* Clears the framebuffer (RAM only — call ssd1306_flush to update panel). */
void ssd1306_clear_buffer(void);

/* Draws a string into the framebuffer at text cell (row 0..7, col 0..20).
 * Clips at the right edge; characters outside 0x20..0x7E render as spaces. */
void ssd1306_draw_text(uint8_t row, uint8_t col, const char *text);

/* Sends the framebuffer to the panel. This is the slow I2C transaction
 * (~1 KiB @ 400 kHz ≈ 26 ms) — never call it while holding a mutex. */
esp_err_t ssd1306_flush(void);
