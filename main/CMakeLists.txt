idf_component_register(
    SRCS
        "main.c"
        "shared_state.c"
        "adc_task.c"
        "servo_task.c"
        "display_task.c"
        "button_task.c"
        "stats_task.c"
        "heartbeat_timer.c"
        "ssd1306.c"
    INCLUDE_DIRS "."
    # IDF v5.3+ split the old umbrella "driver" component into per-peripheral
    # esp_driver_* components, and in v6 "driver" no longer forwards to them —
    # so each driver used must be required explicitly. esp_adc provides the
    # ADC oneshot + calibration APIs.
    PRIV_REQUIRES esp_driver_gpio esp_driver_ledc esp_driver_i2c esp_adc esp_timer
)
