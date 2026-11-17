set(CHIPO_WEATHER_SRCS
    chipo_weather_data.cpp
    chipo_weather_view.cpp
    chipo_weather_controller.cpp)
if(ESP_PLATFORM)
    list(APPEND CHIPO_WEATHER_SRCS chipo_weather_client.cpp)
endif()

idf_component_register(
    SRCS ${CHIPO_WEATHER_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES lvgl log
    PRIV_REQUIRES esp_http_client esp-tls
)
