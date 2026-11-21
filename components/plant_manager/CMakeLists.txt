idf_component_register(SRCS
        "plant_manager.c"
        "sensors/temp_hum/temp_hum_monitor.c"
        "monitoring_tasks.c"
        "sensors/soil_moisture/soil_moisture_monitor.c"
        "config_parser/utility.c"
        "config_parser/config_parser.c"
        PRIV_REQUIRES mqtt_helper nvs_flash esp_netif keystore wifi dht adc light_manager water_manager humidifier_manager water_level_sensor
        REQUIRES espressif__cjson
        INCLUDE_DIRS "include")
