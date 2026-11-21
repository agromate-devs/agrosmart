idf_component_register(SRCS
        "ota_manager.c"
        PRIV_REQUIRES app_update esp_https_ota mbedtls
        INCLUDE_DIRS "include")
