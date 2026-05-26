include("${CMAKE_CURRENT_LIST_DIR}/../../assets/font/c_font/CMakeLists.txt")
include("${CMAKE_CURRENT_LIST_DIR}/../../assets/general/icons/generated/CMakeLists.txt")

idf_component_register(SRCS "src/app_manager.c"
                            "src/app_gesture.c"
                            "src/launcherView.c"
                            "src/notification_popup.c"
                            "../../assets/general/Background/generated/laucherBackground4.c"
                            ${APP_FONT_SRCS}
                            ${APP_GENERAL_ICON_SRCS}
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl common quick_panel data_center waveshare__esp32_c6_touch_amoled_2_06)

# target_compile_definitions(${COMPONENT_LIB} PRIVATE DEBUG)
