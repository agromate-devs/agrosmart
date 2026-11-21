idf_component_register(SRCS "keystore.c"
        PRIV_REQUIRES nvs_flash
                    INCLUDE_DIRS "include")
