/**
 * shared_state.h — the single struct shared by every task, protected by one
 * mutex (FreeRTOS concept: MUTUAL EXCLUSION with priority inheritance).
 *
 * Rules of the road (see README "Shared state & the mutex"):
 *  - take the mutex, touch the struct, release immediately;
 *  - never hold the mutex across I2C transactions or any other slow I/O —
 *    copy the struct to a local snapshot instead.
 */
#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define DISPLAY_MODE_SENSOR 0
#define DISPLAY_MODE_STATS  1

typedef struct {
    int      adc_raw;       /* smoothed raw ADC reading, 0..4095 */
    float    adc_voltage;   /* calibrated voltage at the pot wiper, volts */
    int      servo_angle;   /* angle actually applied by the servo task, deg */
    int      display_mode;  /* DISPLAY_MODE_SENSOR or DISPLAY_MODE_STATS */
    uint32_t button_count;  /* debounced press count since boot */
    uint32_t uptime_sec;    /* wall-clock seconds since boot (stats task) */
} system_state_t;

/* Side struct filled by the stats task, rendered by the display task.
 * Stack high-water marks are in BYTES (ESP-IDF port), i.e. the minimum
 * free stack each task has ever had — lower means closer to overflow. */
typedef struct {
    uint32_t free_heap;      /* current free heap, bytes */
    uint32_t min_free_heap;  /* lowest free heap since boot, bytes */
    uint32_t hwm_adc;
    uint32_t hwm_servo;
    uint32_t hwm_display;
    uint32_t hwm_button;
    uint32_t hwm_stats;
} system_stats_t;

/* The shared data. Only touch these between shared_state_lock()/unlock(). */
extern system_state_t g_state;
extern system_stats_t g_stats;

/* Creates the mutex. Must run before any task is created. */
void shared_state_init(void);

void shared_state_lock(void);
void shared_state_unlock(void);

/* Convenience for readers: copies both structs under the mutex so the
 * caller can work on a consistent snapshot without holding the lock.
 * Either pointer may be NULL. */
void shared_state_snapshot(system_state_t *state_out, system_stats_t *stats_out);
