#pragma once
// Minimal stand-in for ESP-IDF's esp_log.h so the face can be built on a PC.
#include <cstdio>
#define ESP_LOGE(tag, fmt, ...) std::printf(fmt "\n", ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) std::printf(fmt "\n", ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) std::printf(fmt "\n", ##__VA_ARGS__)
