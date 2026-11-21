idf_component_register(SRCS "light_manager.c" "time_helper.c"
        PRIV_REQUIRES led_strip telemetry
        INCLUDE_DIRS "include")
