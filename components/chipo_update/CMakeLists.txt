set(CHIPO_UPDATE_SRCS chipo_update_logic.cpp)
if(ESP_PLATFORM)
    list(APPEND CHIPO_UPDATE_SRCS chipo_update_task.cpp)
endif()

idf_component_register(
    SRCS ${CHIPO_UPDATE_SRCS}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_http_client esp-tls
)
