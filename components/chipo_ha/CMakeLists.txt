# CHIPO Home Assistant: voice control through the REST API (address and token come from NVS).
set(CHIPO_HA_SRCS chipo_ha_logic.cpp chipo_ha_state.cpp chipo_ha.cpp)
if(ESP_PLATFORM)
    list(APPEND CHIPO_HA_SRCS chipo_ha_client.cpp)
endif()

idf_component_register(
    SRCS ${CHIPO_HA_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES chipo_drive lvgl log
    PRIV_REQUIRES esp_http_client esp-tls nvs_flash
)
