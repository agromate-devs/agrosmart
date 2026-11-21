idf_component_register(SRCS
        "humidifier_manager.c"
        PRIV_REQUIRES plant_manager esp_driver_gpio telemetry keystore mqtt_helper
        INCLUDE_DIRS "include")
