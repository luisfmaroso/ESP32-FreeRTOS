/**
 * servo_task.h — drives the SG90 on GPIO18 with LEDC PWM. Blocks on the
 * angle queue; consumes whatever the ADC task produces.
 *
 * FreeRTOS concepts: event-driven task (no polling — 100% blocked between
 * messages) and the consumer side of a QUEUE.
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/* Configures the LEDC timer/channel and creates the task.
 * angle_queue: the queue produced by the ADC task (int degrees, 0..180). */
TaskHandle_t servo_task_start(QueueHandle_t angle_queue);
