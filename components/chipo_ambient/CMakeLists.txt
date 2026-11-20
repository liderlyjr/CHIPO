# CHIPO AMBIENT: rain / fire / soft noise in the background by voice (pieces of 0.96 s, see scripts/make_ambient_sfx.py).
idf_component_register(
    SRCS chipo_ambient_logic.cpp chipo_ambient.cpp
    INCLUDE_DIRS "include"
    REQUIRES chipo_drive lvgl log
)
