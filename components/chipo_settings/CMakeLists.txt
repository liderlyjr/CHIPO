# CHIPO SETTINGS: touch menu, battery indicator, screen saver, burn-in protection, face touch reactions.
idf_component_register(
    SRCS chipo_settings_logic.cpp chipo_settings.cpp chipo_settings_esp.cpp
    INCLUDE_DIRS "include"
    REQUIRES lvgl log esp_wifi
)
