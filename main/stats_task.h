/**
 * stats_task.h — once per second, records free heap, uptime, and the stack
 * high-water mark of every task into shared state for the Stats view.
 *
 * FreeRTOS concepts: task INTROSPECTION (uxTaskGetStackHighWaterMark) and a
 * background task at idle priority — it only runs when nothing else wants
 * the CPU, which is exactly right for bookkeeping.
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Handles of the tasks to be measured. The stats task adds itself. */
typedef struct {
    TaskHandle_t adc;
    TaskHandle_t servo;
    TaskHandle_t display;
    TaskHandle_t button;
} stats_watch_list_t;

/* Creates the task. `watch` is copied, the pointer need not stay valid. */
TaskHandle_t stats_task_start(const stats_watch_list_t *watch);
