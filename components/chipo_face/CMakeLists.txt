set(CHIPO_FACE_SRCS chipo_face.cpp chipo_tilt.cpp chipo_gyro.cpp)
if(ESP_PLATFORM)
    list(APPEND CHIPO_FACE_SRCS chipo_imu.cpp)
endif()

idf_component_register(
    SRCS ${CHIPO_FACE_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES lvgl log
    PRIV_REQUIRES esp_driver_i2c esp_timer
)
