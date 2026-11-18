#pragma once

#include <functional>
#include <string>

#include "chipo_ha_logic.h"

// ESP32 only (implemented in chipo_ha_client.cpp).
bool ChipoHaPost(const HaRequest& req, const std::string& token);
void ChipoHaRunAsync(std::function<void()> job);
bool ChipoHaLoadConfig(HaConfig* out);  // from NVS namespace "chipo_ha"; false if not stored / unusable
