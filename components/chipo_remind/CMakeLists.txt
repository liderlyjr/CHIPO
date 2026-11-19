# CHIPO REMIND: spoken reminders at a time of day, kept across reboots.
idf_component_register(
    SRCS chipo_remind_logic.cpp chipo_remind.cpp
    INCLUDE_DIRS "include"
    REQUIRES chipo_drive chipo_buddy lvgl log
)
