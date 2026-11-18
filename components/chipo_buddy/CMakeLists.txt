# CHIPO BUDDY: timer, pomodoro, stopwatch, alarm and bedside mode, all by voice.
idf_component_register(
    SRCS chipo_buddy_voice.cpp
         chipo_buddy_engine.cpp
         chipo_buddy_view.cpp
         chipo_buddy.cpp
    INCLUDE_DIRS "include"
    REQUIRES chipo_drive lvgl log
)
