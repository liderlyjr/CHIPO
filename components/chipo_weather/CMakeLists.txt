idf_component_register(
    SRCS "chipo_weather.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client json mbedtls log
)
