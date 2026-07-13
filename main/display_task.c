#include "display_task.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "app_config.h"
#include "shared_state.h"
#include "ssd1306.h"

/* One text line is 21 columns; +1 for the NUL. */
#define LINE_LEN (SSD1306_TEXT_COLS + 1)

static void draw_line(uint8_t row, const char *fmt, ...)
{
    char line[LINE_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    ssd1306_draw_text(row, 0, line);
}

static void render_sensor_view(const system_state_t *st)
{
    draw_line(0, "    SENSOR  VIEW");
    draw_line(1, "---------------------");
    draw_line(2, "ADC raw : %4d", st->adc_raw);
    draw_line(3, "Voltage : %.2f V", st->adc_voltage);
    draw_line(4, "Servo   : %3d deg", st->servo_angle);
    draw_line(6, "Presses : %lu", (unsigned long)st->button_count);
    draw_line(7, "Up      : %lu s", (unsigned long)st->uptime_sec);
}

static void render_stats_view(const system_state_t *st, const system_stats_t *sx)
{
    draw_line(0, "    SYSTEM  STATS");
    draw_line(1, "---------------------");
    draw_line(2, "Heap : %6lu B", (unsigned long)sx->free_heap);
    draw_line(3, "Min  : %6lu B", (unsigned long)sx->min_free_heap);
    /* Stack high-water marks = minimum free stack ever, in bytes. */
    draw_line(4, "adc %4lu  srv %4lu", (unsigned long)sx->hwm_adc,
              (unsigned long)sx->hwm_servo);
    draw_line(5, "dsp %4lu  btn %4lu", (unsigned long)sx->hwm_display,
              (unsigned long)sx->hwm_button);
    draw_line(6, "sts %4lu", (unsigned long)sx->hwm_stats);
    draw_line(7, "Up   : %lu s", (unsigned long)st->uptime_sec);
}

static void display_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(DISPLAY_PERIOD_MS));

        /* THE key pattern of this project: hold the mutex only long enough
         * to memcpy two small structs. The ~26 ms I2C flush below happens
         * on the local copies, with the mutex free — so the button task,
         * for example, is never blocked behind a display refresh. */
        system_state_t st;
        system_stats_t sx;
        shared_state_snapshot(&st, &sx);

        ssd1306_clear_buffer();
        if (st.display_mode == DISPLAY_MODE_STATS) {
            render_stats_view(&st, &sx);
        } else {
            render_sensor_view(&st);
        }
        ssd1306_flush(); /* slow I2C happens here, lock-free */
    }
}

TaskHandle_t display_task_start(void)
{
    ESP_ERROR_CHECK(ssd1306_init());

    TaskHandle_t handle = NULL;
    BaseType_t ok = xTaskCreate(display_task, "display", STACK_DISPLAY_TASK,
                                NULL, PRIO_DISPLAY_TASK, &handle);
    assert(ok == pdPASS);
    return handle;
}
