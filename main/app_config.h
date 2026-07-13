/**
 * app_config.h — single place for every pin, priority, stack size and period
 * used by the project, so the whole scheduling/wiring picture is visible at
 * a glance (mirrors the tables in README.md).
 */
#pragma once

/* ------------------------------------------------------------------ pins */
#define PIN_POT_ADC        34   /* ADC1_CH6 — must be ADC1; ADC2 conflicts with WiFi */
#define PIN_SERVO_PWM      18   /* SG90 signal, LEDC low-speed channel 0 */
#define PIN_I2C_SDA        21   /* SSD1306 SDA */
#define PIN_I2C_SCL        22   /* SSD1306 SCL */
#define PIN_BUTTON         4    /* active-low push button, internal pull-up */
#define PIN_HEARTBEAT_LED  2    /* onboard LED, toggled by a software timer */

/* ------------------------------------------------------------- I2C / OLED */
#define I2C_BUS_SPEED_HZ   400000  /* SSD1306 handles 400 kHz fast mode fine */
#define SSD1306_I2C_ADDR   0x3C    /* most 128x64 modules; some use 0x3D */

/* ----------------------------------------------------------------- servo */
/* SG90 pulse range. Datasheet says 1000-2000 us for 0-180 deg but virtually
 * every real SG90 needs ~500-2500 us to reach the full range. If your servo
 * buzzes at the end stops, narrow these. */
#define SERVO_MIN_PULSE_US 500
#define SERVO_MAX_PULSE_US 2500
#define SERVO_FREQ_HZ      50      /* standard 20 ms servo frame */

/* ------------------------------------------------------- task priorities */
/* Higher number = higher priority (FreeRTOS convention).
 * Button is highest so the deferred ISR work preempts everything else the
 * moment the semaphore is given. ADC and Servo share a priority (they form
 * one producer/consumer pipeline and never busy-wait). Display is below
 * them: a slow I2C frame must never delay sampling or servo updates. Stats
 * runs at idle priority — it is pure bookkeeping. */
#define PRIO_BUTTON_TASK   3
#define PRIO_ADC_TASK      2
#define PRIO_SERVO_TASK    2
#define PRIO_DISPLAY_TASK  1
#define PRIO_STATS_TASK    0

/* ------------------------------------------------------- task stack sizes */
/* ESP-IDF stack sizes are in BYTES (not words like vanilla FreeRTOS).
 * Sizes were chosen per task instead of a blanket 4096:
 *  - ADC needs room for the calibration driver call chain.
 *  - Display does snprintf formatting + the I2C driver, the deepest chain here.
 *  - Servo/Button/Stats only call shallow driver/kernel APIs.
 * The Stats view shows each task's high-water mark so these numbers can be
 * tuned against reality: keep >= ~512 B of measured headroom. */
#define STACK_ADC_TASK     3072
#define STACK_SERVO_TASK   2048
#define STACK_DISPLAY_TASK 3328
#define STACK_BUTTON_TASK  2048
#define STACK_STATS_TASK   2048

/* --------------------------------------------------------------- periods */
#define ADC_SAMPLE_PERIOD_MS   50
#define DISPLAY_PERIOD_MS      200
#define STATS_PERIOD_MS        1000
#define HEARTBEAT_PERIOD_MS    500
#define BUTTON_DEBOUNCE_MS     30
