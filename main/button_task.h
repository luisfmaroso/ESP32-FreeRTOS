/**
 * button_task.h — push button on GPIO4 (active-low, internal pull-up),
 * falling-edge interrupt.
 *
 * FreeRTOS concept: the canonical "DEFERRED INTERRUPT HANDLING" pattern.
 * The ISR does the minimum possible — xSemaphoreGiveFromISR + yield — and a
 * high-priority task blocked on the BINARY SEMAPHORE does the real work
 * (debounce, state update) in task context where blocking APIs are legal.
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Configures the GPIO + ISR and creates the task. */
TaskHandle_t button_task_start(void);
