/**
 * adc_task.h — samples the potentiometer (GPIO34 / ADC1_CH6) every 50 ms,
 * smooths it, publishes it to shared state, and feeds the servo queue.
 *
 * FreeRTOS concepts: periodic task with vTaskDelayUntil (fixed-rate, no
 * drift) and the producer side of a QUEUE.
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/* Initializes the ADC oneshot driver + calibration and creates the task.
 * angle_queue: length-1 queue of int (degrees, 0..180) written with
 * xQueueOverwrite. Returns the task handle (for stack HWM reporting). */
TaskHandle_t adc_task_start(QueueHandle_t angle_queue);
