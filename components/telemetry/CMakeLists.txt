idf_component_register(SRCS "telemetry.c"
        PRIV_REQUIRES nvs_flash esp_http_client keystore esp_wifi esp_netif bootloader_support esp_app_format esp_hw_support cjson driver
        INCLUDE_DIRS "include")
