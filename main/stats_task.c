#include "stats_task.h"

#include <assert.h>

#include "esp_heap_caps.h"
#include "esp_system.h" /* esp_get_free_heap_size / esp_get_minimum_free_heap_size */
#include "esp_timer.h"

#include "app_config.h"
#include "shared_state.h"

static stats_watch_list_t s_watch;

static void stats_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(STATS_PERIOD_MS));

        /* Gather everything BEFORE taking the mutex — these kernel calls
         * are cheap but there is no reason to hold the lock during them. */
        system_stats_t stats = {
            .free_heap = esp_get_free_heap_size(),
            .min_free_heap = esp_get_minimum_free_heap_size(),
            /* uxTaskGetStackHighWaterMark returns the minimum free stack
             * the task has ever had, in BYTES on the ESP-IDF port. A value
             * trending toward 0 means the stack size in app_config.h is
             * too small. */
            .hwm_adc = uxTaskGetStackHighWaterMark(s_watch.adc),
            .hwm_servo = uxTaskGetStackHighWaterMark(s_watch.servo),
            .hwm_display = uxTaskGetStackHighWaterMark(s_watch.display),
            .hwm_button = uxTaskGetStackHighWaterMark(s_watch.button),
            .hwm_stats = uxTaskGetStackHighWaterMark(NULL), /* ourselves */
        };
        uint32_t uptime = (uint32_t)(esp_timer_get_time() / 1000000ULL);

        shared_state_lock();
        g_stats = stats;
        g_state.uptime_sec = uptime;
        shared_state_unlock();
    }
}

TaskHandle_t stats_task_start(const stats_watch_list_t *watch)
{
    s_watch = *watch;

    TaskHandle_t handle = NULL;
    /* Priority 0 = same as the idle task. That is safe here because this
     * task sleeps 99.9% of the time; a task that busy-looped at priority 0
     * would starve the idle task and trip the idle watchdog. */
    BaseType_t ok = xTaskCreate(stats_task, "stats", STACK_STATS_TASK, NULL,
                                PRIO_STATS_TASK, &handle);
    assert(ok == pdPASS);
    return handle;
}
