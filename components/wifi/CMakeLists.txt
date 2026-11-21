idf_component_register(SRCS "wifi.c" "smartconfig.c"
        PRIV_REQUIRES esp_wifi nvs_flash esp_event keystore light_manager
                    INCLUDE_DIRS "include")
