idf_component_register(SRCS "mqtt_helper.c" "mqtt_connection_handler.c"
        PRIV_REQUIRES mqtt nvs_flash esp_netif keystore wifi plant_manager espressif__cjson ota_manager light_manager
        INCLUDE_DIRS "include")
