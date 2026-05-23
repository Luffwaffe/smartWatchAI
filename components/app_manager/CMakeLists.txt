idf_component_register(SRCS "src/app_manager.c"
                            "src/app_gesture.c"
                            "src/launcherView.c"
                            "../../assets/general/Background/generated/laucherBackground4.c"
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl quick_panel waveshare__esp32_c6_touch_amoled_2_06)

# target_compile_definitions(${COMPONENT_LIB} PRIVATE DEBUG)
