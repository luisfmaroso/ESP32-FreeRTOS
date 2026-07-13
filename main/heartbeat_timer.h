/**
 * heartbeat_timer.h — blinks the onboard LED (GPIO2) at 1 Hz.
 *
 * FreeRTOS concept: SOFTWARE TIMER. Deliberately implemented as a timer and
 * NOT a task: a 2-line periodic action does not deserve its own stack.
 * All timer callbacks share the timer service ("Tmr Svc") task's stack, so
 * the marginal cost of this heartbeat is one 48-byte-ish timer struct
 * versus ~2 KiB of stack + a TCB for a dedicated task. The trade-off:
 * callbacks run in the timer task's context and therefore MUST NOT block —
 * fine here, gpio_set_level takes nanoseconds.
 */
#pragma once

/* Configures the LED GPIO, creates and starts the auto-reload timer. */
void heartbeat_timer_start(void);
