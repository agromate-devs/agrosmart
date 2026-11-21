idf_component_register(SRCS
        "water_manager.c"
        PRIV_REQUIRES plant_manager esp_driver_gpio telemetry light_manager keystore mqtt_helper water_level_sensor
        INCLUDE_DIRS "include")
