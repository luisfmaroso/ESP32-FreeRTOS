/**
 * display_task.h — renders shared state to the SSD1306 every 200 ms.
 *
 * FreeRTOS concepts: the SNAPSHOT pattern (copy shared data under the mutex,
 * release, then do slow I/O on the copy) and priority-based scheduling —
 * this task is deliberately below ADC/Servo/Button so a ~26 ms I2C frame
 * can be preempted by all of them without affecting control timing.
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Initializes I2C + the SSD1306 panel and creates the task. */
TaskHandle_t display_task_start(void);
