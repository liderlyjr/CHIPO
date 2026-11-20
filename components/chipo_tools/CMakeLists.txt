# CHIPO TOOLS: info strip (time + weather), pomodoro ring, shake, spirit level, magic 8-ball, canvas, subtitles.
idf_component_register(
    SRCS chipo_tools_logic.cpp chipo_tools.cpp
    INCLUDE_DIRS "include"
    REQUIRES chipo_drive chipo_face lvgl log heap
)
