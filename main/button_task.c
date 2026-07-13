#include "button_task.h"

#include <assert.h>

#include "driver/gpio.h"
#include "freertos/semphr.h"

#include "app_config.h"
#include "shared_state.h"

static SemaphoreHandle_t s_press_sem;

/* IRAM_ATTR: the handler must be executable even while the flash cache is
 * disabled (e.g. during flash writes), so it lives in internal RAM.
 *
 * Why so little code here? ISRs run above the scheduler: while one runs,
 * NO task runs on this core, and only the *FromISR API subset is legal.
 * Every microsecond spent here is added latency for the whole system, so
 * the ISR only signals and leaves. A binary semaphore (not a counting one)
 * is deliberate: button bounce produces bursts of edges, and we want them
 * to collapse into one "wake up" event, not queue up as N phantom presses
 * — the task-side debounce handles the rest. */
static void IRAM_ATTR button_isr_handler(void *arg)
{
    BaseType_t higher_prio_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_press_sem, &higher_prio_woken);
    /* If giving the semaphore unblocked the (higher-priority) button task,
     * request a context switch at ISR exit so it runs immediately instead
     * of waiting for the next tick. */
    portYIELD_FROM_ISR(higher_prio_woken);
}

static void button_task(void *arg)
{
    for (;;) {
        /* Blocked here — zero CPU — until the ISR gives the semaphore. */
        if (xSemaphoreTake(s_press_sem, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* Debounce in task context (this is exactly what we could NOT do in
         * the ISR): wait out the bounce window, then require the button to
         * still be held low before counting the press. 30 ms covers typical
         * tactile-switch bounce (~1-20 ms) with margin. */
        vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));
        if (gpio_get_level(PIN_BUTTON) != 0) {
            continue; /* released already (or noise) — not a press */
        }

        shared_state_lock();
        g_state.button_count++;
        g_state.display_mode = (g_state.display_mode == DISPLAY_MODE_SENSOR)
                                   ? DISPLAY_MODE_STATS
                                   : DISPLAY_MODE_SENSOR;
        shared_state_unlock();

        /* Swallow any semaphore give that bounce edges produced while we
         * were debouncing, so one physical press = one count. */
        xSemaphoreTake(s_press_sem, 0);
    }
}

TaskHandle_t button_task_start(void)
{
    s_press_sem = xSemaphoreCreateBinary();
    assert(s_press_sem != NULL);

    gpio_config_t io_cfg = {
        .pin_bit_mask = 1ULL << PIN_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, /* idle high, press pulls to GND */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,   /* fire on press, not release */
    };
    ESP_ERROR_CHECK(gpio_config(&io_cfg));

    /* Per-pin ISR dispatcher; 0 = default interrupt allocation flags. */
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_BUTTON, button_isr_handler, NULL));

    TaskHandle_t handle = NULL;
    BaseType_t ok = xTaskCreate(button_task, "button", STACK_BUTTON_TASK, NULL,
                                PRIO_BUTTON_TASK, &handle);
    assert(ok == pdPASS);
    return handle;
}
