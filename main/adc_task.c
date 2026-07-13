#include "adc_task.h"

#include <assert.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

#include "app_config.h"
#include "shared_state.h"

static const char *TAG = "adc_task";

/* GPIO34 on the ESP32 */
#define POT_ADC_UNIT    ADC_UNIT_1
#define POT_ADC_CHANNEL ADC_CHANNEL_6
/* 12 dB attenuation extends the input range to roughly the full 3.3 V rail,
 * which is what a pot across 3V3/GND produces. */
#define POT_ADC_ATTEN   ADC_ATTEN_DB_12

/* Exponential moving average weight, as a fraction NUM/DEN of the new
 * sample. 1/4 settles in ~8 samples (~400 ms) and removes most pot jitter
 * without feeling laggy. Integer math — no float needed in the filter. */
#define EMA_NUM 1
#define EMA_DEN 4

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali; /* NULL if eFuse calibration unavailable */
static QueueHandle_t s_angle_queue;

static int raw_to_millivolts(int raw)
{
    if (s_cali != NULL) {
        int mv = 0;
        if (adc_cali_raw_to_voltage(s_cali, raw, &mv) == ESP_OK) {
            return mv;
        }
    }
    /* Fallback: linear approximation over the nominal 12 dB range. */
    return raw * 3300 / 4095;
}

static void adc_task(void *arg)
{
    int filtered = -1; /* -1 = filter not seeded yet */

    /* vTaskDelayUntil instead of vTaskDelay: the wake time advances in
     * exact 50 ms steps from a fixed reference, so the sample rate does not
     * drift by the (variable) time the loop body takes to run. */
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(ADC_SAMPLE_PERIOD_MS));

        int raw = 0;
        if (adc_oneshot_read(s_adc, POT_ADC_CHANNEL, &raw) != ESP_OK) {
            continue; /* transient read failure: skip this cycle */
        }

        /* Exponential smoothing: filtered += (raw - filtered) * NUM/DEN.
         * Integer division truncates toward zero, which would leave the
         * filter permanently stuck up to DEN/NUM-1 counts away from the
         * target — so nudge by 1 whenever the step rounds to 0. */
        if (filtered < 0) {
            filtered = raw;
        } else {
            int delta = raw - filtered;
            int step = delta * EMA_NUM / EMA_DEN;
            if (step == 0 && delta != 0) {
                step = (delta > 0) ? 1 : -1;
            }
            filtered += step;
        }

        int mv = raw_to_millivolts(filtered);
        int angle = filtered * 180 / 4095;

        /* Publish under the mutex: two assignments, then release. */
        shared_state_lock();
        g_state.adc_raw = filtered;
        g_state.adc_voltage = mv / 1000.0f;
        shared_state_unlock();

        /* Queue as a MAILBOX: the queue is length 1 and written with
         * xQueueOverwrite, so it always holds the latest angle. If the
         * servo task falls behind it skips straight to the newest value
         * instead of replaying a backlog of stale positions — exactly what
         * you want for a position setpoint. xQueueOverwrite cannot block
         * and always "succeeds", so no return check is needed. */
        xQueueOverwrite(s_angle_queue, &angle);
    }
}

TaskHandle_t adc_task_start(QueueHandle_t angle_queue)
{
    s_angle_queue = angle_queue;

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = POT_ADC_UNIT, /* ADC1: ADC2 is unusable alongside WiFi */
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = POT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, POT_ADC_CHANNEL, &chan_cfg));

    /* The classic ESP32 uses the line-fitting calibration scheme, burned
     * into eFuse on most modules. If this chip has no calibration data we
     * fall back to the nominal transfer function (see raw_to_millivolts). */
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = POT_ADC_UNIT,
        .atten = POT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali) != ESP_OK) {
        s_cali = NULL;
        ESP_LOGW(TAG, "no eFuse ADC calibration, using nominal conversion");
    }
#endif

    TaskHandle_t handle = NULL;
    BaseType_t ok = xTaskCreate(adc_task, "adc", STACK_ADC_TASK, NULL,
                                PRIO_ADC_TASK, &handle);
    assert(ok == pdPASS);
    return handle;
}
