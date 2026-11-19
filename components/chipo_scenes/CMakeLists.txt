# CHIPO SCENES: sound visualizer, aquarium and lava lamp screens, and the "dance" voice command.
idf_component_register(
    SRCS chipo_scenes_logic.cpp chipo_scenes.cpp
    INCLUDE_DIRS "include"
    REQUIRES chipo_drive chipo_face lvgl log
)
