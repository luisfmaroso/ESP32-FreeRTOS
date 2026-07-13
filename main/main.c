/**
 * main.c — wiring only. Creates the shared objects (mutex, queue), starts
 * each subsystem, and hands the task handles to the stats task. All the
 * interesting code lives in the per-subsystem modules; see README.md for
 * the full architecture walkthrough.
 *
 * Startup order matters and is deliberate:
 *  1. shared_state_init()  — the mutex must exist before any task that
 *     could touch g_state is created (a task may run immediately if its
 *     priority beats app_main's).
 *  2. Queue creation       — same reasoning, for the producer/consumer pair.
 *  3. Tasks + timer        — safe to start in any order after 1 and 2.
 * (Future Work in the README: an event group would let tasks start blocked
 * and be released together once all init is done — not needed at this size.)
 */
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "adc_task.h"
#include "button_task.h"
#include "display_task.h"
#include "heartbeat_timer.h"
#include "servo_task.h"
#include "shared_state.h"
#include "stats_task.h"

void app_main(void)
{
    shared_state_init();

    /* Length 1 on purpose: used as a MAILBOX (xQueueOverwrite) carrying the
     * latest servo setpoint. See adc_task.c for the rationale. */
    QueueHandle_t angle_queue = xQueueCreate(1, sizeof(int));
    assert(angle_queue != NULL);

    stats_watch_list_t watch = {
        .adc = adc_task_start(angle_queue),
        .servo = servo_task_start(angle_queue),
        .display = display_task_start(),
        .button = button_task_start(),
    };
    stats_task_start(&watch);

    heartbeat_timer_start();

    /* app_main returns; its (main task's) resources are freed by IDF and
     * the five tasks + timer service carry on under the scheduler. */
}
