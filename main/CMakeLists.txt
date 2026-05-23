idf_component_register(
    SRCS
        main.c
        app_init.c
        
    INCLUDE_DIRS
        .
    REQUIRES
        app_manager
        rtc_manager
        ai_talk
        clock
        setting
        test
        bsp_extra
        nvs_flash
)
