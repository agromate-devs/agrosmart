idf_component_register(SRCS "main.c"
        PRIV_REQUIRES spi_flash esp_event nvs_flash esp_wifi wifi keystore mqtt_helper plant_manager light_manager water_manager humidifier_manager ota_manager telemetry water_level_sensor
        INCLUDE_DIRS "")