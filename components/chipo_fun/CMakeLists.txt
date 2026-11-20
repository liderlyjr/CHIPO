# CHIPO FUN: AMOLED start-up screen, colour lamp, dice and coin.
idf_component_register(
    SRCS chipo_fun_voice.cpp chipo_fun.cpp
    INCLUDE_DIRS "include"
    REQUIRES chipo_drive lvgl log
)
