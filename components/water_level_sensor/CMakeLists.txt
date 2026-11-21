idf_component_register(SRCS
        "water_level_sensor.c"
        PRIV_REQUIRES esp_driver_gpio freertos
        INCLUDE_DIRS "include")
