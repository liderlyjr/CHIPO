# CHIPO boot summary: a few clear lines in the serial log (version, memory, which modules started or failed).
idf_component_register(
    SRCS chipo_boot_report.cpp
    INCLUDE_DIRS "include"
    REQUIRES log esp_system heap
)
