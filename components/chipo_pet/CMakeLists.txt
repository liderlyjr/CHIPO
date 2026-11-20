# CHIPO PET: mood of the little computer and voice games (rock-paper-scissors, guess the number).
idf_component_register(
    SRCS chipo_pet_logic.cpp chipo_pet.cpp
    INCLUDE_DIRS "include"
    REQUIRES chipo_drive lvgl log
)
