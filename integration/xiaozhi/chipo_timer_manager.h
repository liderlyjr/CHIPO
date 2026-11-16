#pragma once

#include <array>
#include <cstdio>
#include <mutex>
#include <string>

#include <esp_timer.h>
#include "application.h"
#include "assets/lang_config.h"

class ChipoTimerManager {
public:
    static ChipoTimerManager& GetInstance() {
        static ChipoTimerManager instance;
        return instance;
    }

    int Start(int seconds, const std::string& label) {
        if (seconds < 1 || seconds > 86400) return 0;
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& slot : slots_) {
            if (slot.active) continue;
            if (slot.handle == nullptr) return 0;
            slot.label = label;
            slot.deadline_us = esp_timer_get_time() + static_cast<int64_t>(seconds) * 1000000LL;
            slot.active = true;
            if (esp_timer_start_once(slot.handle, static_cast<uint64_t>(seconds) * 1000000ULL) != ESP_OK) {
                slot.active = false;
                slot.label.clear();
                return 0;
            }
            return slot.id;
        }
        return 0;
    }

    bool Cancel(int id) {
        std::lock_guard<std::mutex> lock(mutex_);
        bool cancelled = false;
        for (auto& slot : slots_) {
            if (slot.active && (id == 0 || slot.id == id)) {
                esp_timer_stop(slot.handle);
                slot.active = false;
                slot.label.clear();
                cancelled = true;
            }
        }
        return cancelled;
    }

    std::string Status() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string result;
        const int64_t now = esp_timer_get_time();
        for (const auto& slot : slots_) {
            if (!slot.active) continue;
            const int64_t remaining_us = slot.deadline_us - now;
            const int remaining = remaining_us > 0 ? static_cast<int>((remaining_us + 999999) / 1000000) : 0;
            char time[24];
            FormatTime(remaining, time, sizeof(time));
            if (!result.empty()) result += "\n";
            result += "Temporizador " + std::to_string(slot.id) + ": " + time;
            if (!slot.label.empty()) result += " (" + slot.label + ")";
        }
        return result.empty() ? "No hay temporizadores activos." : result;
    }

    std::string DisplayText() {
        std::lock_guard<std::mutex> lock(mutex_);
        const int64_t now = esp_timer_get_time();
        const TimerSlot* next = nullptr;
        for (const auto& slot : slots_) {
            if (slot.active && (!next || slot.deadline_us < next->deadline_us)) next = &slot;
        }
        if (!next) return "";
        const int64_t remaining_us = next->deadline_us - now;
        const int remaining = remaining_us > 0 ? static_cast<int>((remaining_us + 999999) / 1000000) : 0;
        char time[24];
        FormatTime(remaining, time, sizeof(time));
        int count = 0;
        for (const auto& slot : slots_) if (slot.active) ++count;
        std::string text = "TMR " + std::string(time);
        if (count > 1) text += " +" + std::to_string(count - 1);
        return text;
    }

private:
    struct TimerSlot {
        esp_timer_handle_t handle = nullptr;
        int id = 0;
        bool active = false;
        int64_t deadline_us = 0;
        std::string label;
    };

    std::array<TimerSlot, 3> slots_{};
    std::mutex mutex_;

    ChipoTimerManager() {
        for (size_t i = 0; i < slots_.size(); ++i) {
            slots_[i].id = static_cast<int>(i + 1);
            esp_timer_create_args_t args = {};
            args.callback = &TimerExpired;
            args.arg = &slots_[i];
            args.dispatch_method = ESP_TIMER_TASK;
            args.name = "chipo_timer";
            if (esp_timer_create(&args, &slots_[i].handle) != ESP_OK) {
                slots_[i].handle = nullptr;
            }
        }
    }

    static void FormatTime(int seconds, char* output, size_t size) {
        if (seconds >= 3600) {
            std::snprintf(output, size, "%02d:%02d:%02d", seconds / 3600, (seconds / 60) % 60, seconds % 60);
        } else {
            std::snprintf(output, size, "%02d:%02d", seconds / 60, seconds % 60);
        }
    }

    static void TimerExpired(void* arg) {
        auto* slot = static_cast<TimerSlot*>(arg);
        if (!slot) return;
        auto& manager = GetInstance();
        std::string label;
        {
            std::lock_guard<std::mutex> lock(manager.mutex_);
            if (!slot->active) return;
            slot->active = false;
            label = slot->label;
            slot->label.clear();
        }
        auto& app = Application::GetInstance();
        app.Schedule([label]() {
            auto& application = Application::GetInstance();
            const std::string message = label.empty() ? "¡Se acabó el tiempo!" : label + ": ¡se acabó el tiempo!";
            application.Alert("TEMPORIZADOR", message.c_str(), "happy", Lang::Sounds::OGG_EXCLAMATION);
        });
    }
};
