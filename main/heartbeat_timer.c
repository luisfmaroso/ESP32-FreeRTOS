#include "heartbeat_timer.h"

#include <assert.h>
#include <stdbool.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "app_config.h"

/* Runs in the timer service task's context: keep it short, never block.
 * Note it does not touch shared state, so it needs no mutex — another
 * reason a timer fits: no synchronization story at all. */
static void heartbeat_cb(TimerHandle_t timer)
{
    static bool level = false;
    level = !level;
    gpio_set_level(PIN_HEARTBEAT_LED, level);
}

void heartbeat_timer_start(void)
{
    gpio_config_t io_cfg = {
        .pin_bit_mask = 1ULL << PIN_HEARTBEAT_LED,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&io_cfg));

    TimerHandle_t timer = xTimerCreate(
        "heartbeat",
        pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS),
        pdTRUE, /* auto-reload: re-fires every period, unlike a one-shot */
        NULL,   /* timer ID unused */
        heartbeat_cb);
    assert(timer != NULL);

    /* Start command is queued to the timer service task; a zero block time
     * is fine because the timer command queue is empty at startup. */
    BaseType_t ok = xTimerStart(timer, 0);
    assert(ok == pdPASS);
}
