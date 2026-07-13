#include "servo_task.h"

#include <assert.h>

#include "driver/ledc.h"

#include "app_config.h"
#include "shared_state.h"

#define SERVO_LEDC_MODE    LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_TIMER   LEDC_TIMER_0
#define SERVO_LEDC_CHANNEL LEDC_CHANNEL_0

/* 14-bit resolution at 50 Hz: one duty step = 20000 us / 16384 ≈ 1.22 us,
 * i.e. ~0.11 deg of servo resolution — far finer than an SG90 can move.
 * (The ESP32 LEDC could go higher, but 14 bits keeps the numbers readable.) */
#define SERVO_LEDC_RES     LEDC_TIMER_14_BIT
#define SERVO_DUTY_MAX     ((1 << 14) - 1)
#define SERVO_PERIOD_US    (1000000 / SERVO_FREQ_HZ)

static QueueHandle_t s_angle_queue;

static uint32_t angle_to_duty(int angle)
{
    if (angle < 0) {
        angle = 0;
    } else if (angle > 180) {
        angle = 180;
    }
    uint32_t pulse_us = SERVO_MIN_PULSE_US +
        (uint32_t)angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) / 180;
    return pulse_us * (SERVO_DUTY_MAX + 1) / SERVO_PERIOD_US;
}

static void servo_task(void *arg)
{
    for (;;) {
        int angle = 0;
        /* Block indefinitely: this task consumes zero CPU until the ADC
         * task publishes an angle. Compare with the ADC task, which is
         * time-driven — together they show the two canonical task styles. */
        if (xQueueReceive(s_angle_queue, &angle, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ledc_set_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL, angle_to_duty(angle));
        ledc_update_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL);

        /* Report the angle actually applied (post-clamp), so the display
         * shows reality rather than the raw request. */
        shared_state_lock();
        g_state.servo_angle = angle;
        shared_state_unlock();
    }
}

TaskHandle_t servo_task_start(QueueHandle_t angle_queue)
{
    s_angle_queue = angle_queue;

    ledc_timer_config_t timer_cfg = {
        .speed_mode = SERVO_LEDC_MODE,
        .duty_resolution = SERVO_LEDC_RES,
        .timer_num = SERVO_LEDC_TIMER,
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t chan_cfg = {
        .gpio_num = PIN_SERVO_PWM,
        .speed_mode = SERVO_LEDC_MODE,
        .channel = SERVO_LEDC_CHANNEL,
        .timer_sel = SERVO_LEDC_TIMER,
        .duty = angle_to_duty(90), /* start centered */
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&chan_cfg));

    TaskHandle_t handle = NULL;
    BaseType_t ok = xTaskCreate(servo_task, "servo", STACK_SERVO_TASK, NULL,
                                PRIO_SERVO_TASK, &handle);
    assert(ok == pdPASS);
    return handle;
}
