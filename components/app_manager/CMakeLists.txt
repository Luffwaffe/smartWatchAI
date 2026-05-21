idf_component_register(SRCS "src/app_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl waveshare__esp32_c6_touch_amoled_2_06)

target_compile_definitions(${COMPONENT_LIB} PRIVATE DEBUG)
